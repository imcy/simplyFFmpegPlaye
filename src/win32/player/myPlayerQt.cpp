#include "myPlayerQt.h"
#include "media/readPacketsThread.h"
#include "media/playerMediaTimer.h"
#include "videoWidget.h"
#include "log/logger.h"
#include "Wininet.h"
#include <thread>

static bool isPlay = false;
static bool isPressSlider = false;
static bool isMute = false;
static bool openFinshed = false;
static bool autoPause = false;
static bool isSeek = false;

static std::map<QString, int> videoFiles;
static int curVolume;
static playerMedia * media;


bool checkNetwork()
{
	DWORD   flag;
	bool    con = InternetGetConnectedState(&flag, 0);
	return con;
}

/** ����Ƶ�߳�

	openVideo()�����е��ã�
	���ڷ�ֹavformat_open_input����ʱ��������������ڳ�ʱ��δ��Ӧ
	
	@param file     ��Ƶ·�������ص�ַ����������
*/
void openFileThread(std::string file)
{
	openFinshed = false;
	media = playerMedia::getInstance()->setMediaFile(file.c_str())->open();
	openFinshed = true;
}

myPlayerQt::myPlayerQt(QWidget *parent)
	: QMainWindow(parent)
{

	ui.setupUi(this);
	popMenu = new QMenu(this);

	QObject::connect(this,
		SIGNAL(sendPos(float)),
		readPacketsThread::getInstance(),
		SLOT(receivePos(float))
	);
	QObject::connect(ui.openGL_widget,
		SIGNAL(showSlider(bool)),
		this,
		SLOT(showPlayGroup(bool))
	);
	setAcceptDrops(true);

	initLogger("Logger.log");	

	QFile file("./style.qss");
	file.open(QFile::ReadOnly);
	QString styleSheet = tr(file.readAll());
	this->setStyleSheet(styleSheet);
	ui.videoListWidget->setStyleSheet(styleSheet);
	ui.playSlider->setStyleSheet(styleSheet);
	ui.groupBox->setStyleSheet(styleSheet);
	ui.videoInfo->setStyleSheet(styleSheet);
	file.close();

	this->windowWidth = this->width();
	
	movie = new QMovie("./Resources/loading.gif");
	ui.loadingLabel->setAttribute(Qt::WA_TranslucentBackground);
	movie->start();
	ui.loadingLabel->setMovie(movie);
	ui.loadingLabel->hide();
	ui.textLabel->hide();

	startTimer(40);
	pauseTime = 0;

	sprintf(textBuf, "���ٴ���: %d\n", pauseTime);
	ui.pauseTime->setText(QString::fromLocal8Bit(textBuf));
	sprintf(textBuf, "ƽ������ʱ��: %.2f s\n", 0);
	ui.bufferTime->setText(QString::fromLocal8Bit(textBuf));
	sprintf(textBuf, "��ʱ��: %.2f s\n", 0);
	ui.openTime->setText(QString::fromLocal8Bit(textBuf));

	sprintf(textBuf, "���ٴ���: %d\n", 0);
	ui.pauseTime->setText(QString::fromLocal8Bit(textBuf));

	addVideoItem(QString::fromLocal8Bit("rtmp://10.212.0.157/live/1"), url);
	addVideoItem(QString::fromLocal8Bit("rtmp://10.212.0.157/live/2"), url);
	addVideoItem(QString::fromLocal8Bit("http://pull.v.cc.163.com/pushstation/TESTe797e39d7260608e21337559.flv?wsSecret=bf87bd53f5ba6e9e976b1e9f44ef458e&wsTime=5ff5a32d"), url);
	ui.videoInfoWidget->hide();
	//resizeEvent(NULL);
}


void myPlayerQt::open()
{
	QString name = QFileDialog::getOpenFileName(
		this, QString::fromLocal8Bit("ѡ����Ƶ�ļ�"));
	int type = local;
	if (!name.isEmpty() && formatCheck(name, type))
	{
		addVideoItem(name, type);
		openVideo(name, type);
	}
	
}

void myPlayerQt::close()
{
	if (isPlay)
	{
		readPacketsThread::getInstance()->stop();
		readPacketsThread::getInstance()->exit();

		playerMedia::getInstance()->video->exit();
		playerMedia::getInstance()->close();
		
		playTime.invalidate();
		isPlay = false;
	}

}

void myPlayerQt::netAddressInput()
{
	bool ok = false;
	QString name = QInputDialog::getText(this,
		QString::fromLocal8Bit("��������"),
		QString::fromLocal8Bit("�������ַ"),
		QLineEdit::Normal, QString::null, &ok);
	int type = url;
	/*if (ok && !name.isEmpty() && formatCheck(name, type)) */
	if (ok && !name.isEmpty())
	{
		addVideoItem(name, type);
		openVideo(name, type);
	}
}

void myPlayerQt::openVideo(QString name, int type)
{
	playerMedia::getInstance()->interruptFlag = true;
	
	ui.videoInfo->clear();
	this->setWindowTitle(name);
	ui.iconBackground->setVisible(false);
	loadingShow(true, QString::fromLocal8Bit("���ڴ�..."));
	pauseTime = 0;
	if (isPlay)
	{
		close();
	}
	//else
	//{
		QElapsedTimer etime;
		etime.start();
		while (etime.elapsed() < 1000); // ��ʱ��ֹffmpeg����δ�˳�
	//}

	if (isPressSlider)
	{
		isPressSlider = false;
	}

	if (type == url && !checkNetwork())
	{
		QMessageBox::warning(this, QString::fromLocal8Bit("����"),
			QString::fromLocal8Bit("�޷��������磬������������!"));
		LOG(LOG_INFO) << "Internet connecting fail." << std::endl;
		loadingShow(false, NULL);
		return;
	}

	std::string file = name.toLocal8Bit().data();
	std::thread t(openFileThread, file);
	t.detach();

	QElapsedTimer et;
	et.start();
	while (!openFinshed)
	{
		QCoreApplication::processEvents();
		if (et.elapsed() > 200000)
		{
			playerMedia::getInstance()->interruptFlag = true;
			QMessageBox::warning(this, QString::fromLocal8Bit("����"),
				QString::fromLocal8Bit("���ļ���ʱ��"));
			LOG(LOG_INFO) << "Can not open file, open time out: " << file << std::endl;
			playerMedia::getInstance()->close();
			loadingShow(false, NULL);
			return;
		}
	}
	auto time = et.elapsed() / 1000.0;
	sprintf(textBuf, "��ʱ��: %.2f s\n", time);
	ui.openTime->setText(QString::fromLocal8Bit(textBuf));
	readPacketsThread::getInstance()->isExit = false;
	readPacketsThread::getInstance()->start();
	playerMedia::getInstance()->video->isExit = false;
	playerMedia::getInstance()->video->start();
	
	if (media == nullptr)
	{
		QMessageBox::warning(this, QString::fromLocal8Bit("����"), 
		QString::fromLocal8Bit("�޷����ļ���"));
		LOG(LOG_INFO) << "Media get nullptr, can not open file: "<< file << std::endl;
		loadingShow(false, NULL);
		return;
	}
	if (media->getAVFormatContext())
	{
		media->audio->setVolume(0.5 * SDL_MIX_MAXVOLUME);
		curVolume = int(ui.volumeSlider->maximum() / 2);
		ui.volumeSlider->setValue(curVolume);
	}
	media->type = type;
	showVideoInfo(media);
	loadingShow(false, NULL);

	ui.videoListWidget->setVisible(true);
	resizeEvent(NULL);

	autoPause = false;
	isPlay = false;
	play();
	playTime.start();
}


void myPlayerQt::play()
{
	if (!playerMedia::getInstance()->getAVFormatContext())
	{
		QMessageBox::warning(this, QString::fromLocal8Bit("����"),
			QString::fromLocal8Bit("���ȴ�һ����Ƶ�ļ���"));
		return;
	}
	isPlay = !isPlay;
	playerMediaTimer::getInstance()->setPlay(isPlay);
	if (isPlay)
	{
		ui.playButton->setStyleSheet(PAUSE);
		ui.pauseAction->setText(QString::fromLocal8Bit("��ͣ"));
		if (playerMedia::getInstance()->audio->getAVCodecContext())
			playerMedia::getInstance()->audio->audioOpen();
	}
	else
	{
		ui.playButton->setStyleSheet(PLAY);
		ui.pauseAction->setText(QString::fromLocal8Bit("����"));
		if (playerMedia::getInstance()->audio->getAudioQueueSize() > 0
			&& playerMedia::getInstance()->getAVFormatContext())
			SDL_CloseAudio();
	}

}

void myPlayerQt::setVolume() {
	float pos = 0;
	pos = (float)ui.volumeSlider->value() / (float)(ui.volumeSlider->maximum() + 1);
	curVolume = ui.volumeSlider->value();
	if (playerMedia::getInstance()->getAVFormatContext())
	{
		if (curVolume == 0 && !isMute)
		{
			setMute();
		}
		else if (curVolume >0 && isMute)
		{
			setMute();
		}
		else
		{
			playerMedia::getInstance()->audio->setVolume(pos * SDL_MIX_MAXVOLUME);
		}
	}
}
void myPlayerQt::setMute()
{
	if (videoFiles.size() == 0)
	{
		return;
	}
	isMute = !isMute;
	if (isMute)
	{
		ui.volumeButton->setStyleSheet(VOLUME_MUTE);
		ui.volumeSlider->setValue(0);
		playerMedia::getInstance()->audio->setVolume(0);
		ui.volumeMute->setText(QString::fromLocal8Bit("ȡ������"));
	}
	else
	{
		ui.volumeButton->setStyleSheet(VOLUME);
		ui.volumeSlider->setValue(curVolume);
		ui.volumeMute->setText(QString::fromLocal8Bit("����"));
		setVolume();
	}
}

void myPlayerQt::sliderPressed()
{
	isPressSlider = true;
}
void myPlayerQt::sliderRelease()
{
	loadingShow(true, QString::fromLocal8Bit("������ת..."));
	float pos = 0;
	pos = (float)ui.playSlider->value() / (float)(ui.playSlider->maximum() + 1);
	emit sendPos(pos);
	if (playerMedia::getInstance()->type == local)
	{
		isPressSlider = false;	
	}
	loadingShow(false, NULL);
}

void myPlayerQt::skipPrevious()
{
	QListWidgetItem *item = new QListWidgetItem;
	int row = ui.videoListWidget->currentRow();
	if (row >= 1)
	{
		clickedVideoList(ui.videoListWidget->item(row - 1));
	}
}
void myPlayerQt::skipNext()
{
	QListWidgetItem *item = new QListWidgetItem;
	int row = ui.videoListWidget->currentRow();
	if (row < ui.videoListWidget->count() - 1)
	{
		clickedVideoList(ui.videoListWidget->item(row + 1));
	}
}

void myPlayerQt::seekBack()
{
	float pos = 0;
	pos = (float)(ui.playSlider->value() - 40) / (float)(ui.playSlider->maximum() + 1);
	emit sendPos(pos);
}
void myPlayerQt::seekForward()
{
	float pos = 0;
	pos = (float)(ui.playSlider->value() + 40) / (float)(ui.playSlider->maximum() + 1);
	emit sendPos(pos);
}

void myPlayerQt::showPlayGroup(bool isShow)
{
	if (windowState() &  Qt::WindowFullScreen)
	{
		ui.groupBox->setVisible(isShow);
		ui.mainToolBar->setVisible(isShow);
		ui.menuBar->setVisible(isShow);
		ui.statusBar->setVisible(isShow);
	}
}

void myPlayerQt::fullScreen()
{
	if (windowState() &  Qt::WindowFullScreen)
	{
		ui.groupBox->setVisible(true);
		ui.mainToolBar->setVisible(true);
		ui.menuBar->setVisible(true);
		ui.statusBar->setVisible(true);

		showNormal();
		ui.fullScreenAction->setText(QString::fromLocal8Bit("ȫ��"));
		ui.openGL_widget->resize(this->windowWidth, this->height() - 140);
	}
	else
	{
		ui.groupBox->setVisible(false);
		ui.mainToolBar->setVisible(false);
		ui.menuBar->setVisible(false); 
		ui.statusBar->setVisible(false);
		ui.videoInfoWidget->setVisible(false);

		showFullScreen();
		ui.fullScreenAction->setText(QString::fromLocal8Bit("�˳�ȫ��"));
		ui.openGL_widget->resize(this->windowWidth, this->height() - 20);
	}
	playerMediaTimer::getInstance()->resetImage(this->windowWidth, ui.openGL_widget->height());
}

void myPlayerQt::showVideoList()
{
	if (ui.videoListWidget->isVisible())
	{
		ui.videoInfoWidget->setVisible(false);
	}
	else
	{
		ui.videoInfoWidget->setVisible(true);
	}
	
	resizeEvent(NULL);
}
void myPlayerQt::clickedVideoList(QListWidgetItem *item)
{
	ui.videoListWidget->setCurrentItem(item);
	QString name = item->text();
	openVideo(name, videoFiles[name]);
}

void myPlayerQt::setPlayTime(double pts, double rate)
{
	memset(textBuf, 0, 1024);
	int hour = (int)(pts / 1000000 / 60 / 60);
	int min = (int)(pts / 1000000 / 60) % 60;
	int sec = (int)(pts / 1000000) % 60 % 60;
	sprintf(textBuf, "%02d:%02d:%02d", hour, min, sec);
	ui.curTime->setText(textBuf);
	if (!isPressSlider && isPlay) {
		ui.playSlider->setValue(rate);
		if (playerMedia::getInstance()->type == local && 
			(int)rate >= (int)(ui.playSlider->maximum()))
		{
			play();
			emit sendPos(0);
			ui.playSlider->setValue(0);
			
		}
		
	}
}

void myPlayerQt::urlTimeoutCheck(double rate)
{
	if (isPlay && playerMedia::getInstance()->video->getVideoQueueSize() == 0)
	{
		if (playerMedia::getInstance()->audio->getAudioQueueCount() > 0)
		{
			return;
		}
		pauseTime += 1;
		
		sprintf(textBuf, "���ٴ���: %d\n", pauseTime);
		ui.pauseTime->setText(QString::fromLocal8Bit(textBuf));
		
		// ����Ƿ�����Ƶĩβ
		if ((int)rate >= (int)(ui.playSlider->maximum() - 20))
		{
			emit sendPos(0);
			ui.playSlider->setValue(0);
			play();
			return;
		}
		
		loadingShow(true, QString::fromLocal8Bit("���ڻ���..."));
		autoPause = true;
		play();
		QElapsedTimer et;
		et.start();
		while (true)
		{
			if ( playerMedia::getInstance()->video->getVideoQueueCount() >= 100)
			{
				loadingShow(false, NULL);
				autoPause = false;
				if (isPressSlider)
				{
					isPressSlider = false;
				}
				play();
				// ��ֹ����ʱ�л�Ϊ������Ƶ������Ƶδ���׹ر�
				if (playerMedia::getInstance()->type == local)
				{
					play();
				}
				bufferTime += et.elapsed() / 1000;

				sprintf(textBuf, "ƽ������ʱ��: %.2f s\n", (bufferTime / pauseTime));
				ui.bufferTime->setText(QString::fromLocal8Bit(textBuf));
				break;
			}
			if (et.elapsed() > 2000000)
			{
				QString message = "����";
				playerMedia::getInstance()->interruptFlag = true;
				if (!checkNetwork())
				{
					message = QString::fromLocal8Bit("�޷��������磬������������!");
				}
				else
				{
					message = QString::fromLocal8Bit("����״̬��޷�����");
				}
				QMessageBox::warning(this, QString::fromLocal8Bit("����"), message);
				loadingShow(false, NULL);
				//playerMedia::getInstance()->close();
				break;
			}
			QCoreApplication::processEvents();
		}
	}
}
bool myPlayerQt::formatCheck(QString name, int type)
{
	QStringList titles = name.split("/");
	QString postfix = titles.constLast().split(".").last().toLower();
	if (type == url)
	{
		if (postfix == QString::fromLocal8Bit("mp4")
			|| postfix == QString::fromLocal8Bit("flv")
			|| postfix == QString::fromLocal8Bit("m3u8"))
		{
			return true;
		}
	}
	else
	{
		if (postfix == QString::fromLocal8Bit("mp4")
			|| postfix == QString::fromLocal8Bit("rmvb")
			|| postfix == QString::fromLocal8Bit("flv")
			|| postfix == QString::fromLocal8Bit("avi")
			|| postfix == QString::fromLocal8Bit("mkv")
			|| postfix == QString::fromLocal8Bit("mov")
			|| postfix == QString::fromLocal8Bit("wmv")
			|| postfix == QString::fromLocal8Bit("h264"))
		{
			return true;
		}
	}
	QMessageBox::warning(this, QString::fromLocal8Bit("����"),
		QString::fromLocal8Bit("��ʽ��֧�֣�������ѡ��!"));
	return false;
}
void myPlayerQt::addVideoItem(QString name, int type)
{
	if (videoFiles.find(name) == videoFiles.end())
	{
		videoFiles.insert(std::map<QString, int>::value_type(name, type));
		QListWidgetItem *item = new QListWidgetItem;
		item->setText(name);
		ui.videoListWidget->addItem(item);
		ui.videoListWidget->setCurrentItem(item);
	}
}
void myPlayerQt::loadingShow(bool isShow, QString text)
{
	if (isShow)
	{
		ui.loadingLabel->setVisible(true);
		ui.textLabel->setText(text);
		ui.textLabel->setVisible(true);
	}
	else
	{
		ui.loadingLabel->setVisible(false);
		ui.textLabel->setVisible(false);
	}
}

void myPlayerQt::timerEvent(QTimerEvent * pEvent)
{
	if (!isPlay)
	{
		return;
	}
	
	//if (playerMedia::getInstance()->totalTime > 0)
	//{
		double pts = (double)playerMedia::getInstance()->pts * 1000;
		double total = (double)playerMedia::getInstance()->totalTime;
		double rate = pts / total;
		setPlayTime(pts, rate);
		if (!isSeek && playerMedia::getInstance()->type == url)
		{
			urlTimeoutCheck(rate);
		}

	//}
	if (playerMedia::getInstance()->video)
	{
		sprintf(textBuf, "��Ƶ����Ŀ: %d\n", playerMedia::getInstance()->video->getVideoQueueCount());
		ui.videoPackets->setText(QString::fromLocal8Bit(textBuf));
	}
	if (playerMedia::getInstance()->audio)
	{
		sprintf(textBuf, "��Ƶ��Ŀ: %d\n", playerMedia::getInstance()->audio->getAudioQueueCount());
		ui.audioPackets->setText(QString::fromLocal8Bit(textBuf));
	}
	
	if (playTime.isValid())
	{
		sprintf(textBuf, "����ʱ��: %.2f s\n", playTime.elapsed() / 1000.0);
		ui.playTime->setText(QString::fromLocal8Bit(textBuf));
	}
	
}
void myPlayerQt::resizeEvent(QResizeEvent *pEvent)
{
	if (ui.videoInfoWidget->isVisible())
	{
		this->windowWidth = this->width() - 200;
		ui.videoInfoWidget->move(this->windowWidth, 0);
		ui.videoInfoWidget->resize(210, this->height());
		ui.videoListWidget->resize(200, this->height());
		ui.videoInfo->resize(200, this->height());
	}
	else
	{
		this->windowWidth = this->width();
	}

	ui.openGL_widget->resize(this->windowWidth, this->height() - 140);
	
	ui.groupBox->move(0, this->height() - 140);
	ui.groupBox->resize(this->windowWidth, 90);
	ui.playSlider->resize(this->windowWidth, 20);
	ui.playButton->move(this->windowWidth / 2, 20);
	ui.curTime->move(20, ui.playButton->y());
	ui.totalTime->move(this->windowWidth - 70, ui.curTime->y());

	ui.volumeSlider->move(ui.totalTime->x(), ui.totalTime->y() + 20);
	ui.volumeButton->move(ui.volumeSlider->x()-30, ui.volumeSlider->y());
	ui.previousButton->move(ui.playButton->x()-120, ui.playButton->y()+2);
	ui.nextButton->move(ui.playButton->x() + 130, ui.previousButton->y());
	ui.rewinButton->move(ui.playButton->x() - 60, ui.previousButton->y());
	ui.forwardButton->move(ui.playButton->x() + 70, ui.previousButton->y());
	
	ui.loadingLabel->move(ui.playButton->x(), ui.openGL_widget->height() / 2);
	ui.loadingLabel->resize(20, 20);
	ui.textLabel->move(ui.loadingLabel->x() - 50, ui.loadingLabel->y() + 40);

	ui.iconBackground->move(ui.playButton->x()-10, ui.loadingLabel->y()-20);

	playerMediaTimer::getInstance()->resetImage(this->windowWidth, ui.openGL_widget->height());
}
void myPlayerQt::closeEvent(QCloseEvent *pEvent)
{
}
void myPlayerQt::keyPressEvent(QKeyEvent * pEvent)
{
	switch (pEvent->key())
	{
	case Qt::Key_Space:
		play();
		break;
	case Qt::Key_Left:
		seekBack();
		break;
	case Qt::Key_Right:
		seekForward();
		break;
	case Qt::Key_Escape:
		if (windowState() &  Qt::WindowFullScreen)
		{
			fullScreen();
		}
		else
		{
			QApplication::exit();
		}
		break;
	}
}
void myPlayerQt::mouseDoubleClickEvent(QMouseEvent * pEvent)
{
	fullScreen();
}
void myPlayerQt::contextMenuEvent(QContextMenuEvent * pEvent)
{
	popMenu->clear();
	popMenu->addAction(ui.openLocalFile);
	popMenu->addAction(ui.openWebFile);
	popMenu->addAction(ui.openVideoList);
	popMenu->addAction(ui.pauseAction);
	popMenu->addAction(ui.fullScreenAction);
	popMenu->addAction(ui.volumeMute);
	popMenu->addAction(ui.previousVideo);
	popMenu->addAction(ui.nextVideo);

	popMenu->exec(QCursor::pos());
	pEvent->accept();
}
void myPlayerQt::dragEnterEvent(QDragEnterEvent * pEvent)
{
	if (pEvent->mimeData()->hasFormat("text/uri-list"))
		pEvent->acceptProposedAction();
}
void myPlayerQt::dropEvent(QDropEvent * pEvent)
{
	QList<QUrl> urls = pEvent->mimeData()->urls();
	if (urls.isEmpty())
		return;
	ui.videoInfoWidget->setVisible(true);
	resizeEvent(NULL);
	foreach(QUrl url, urls) {
		QString file_name = url.toLocalFile();
		if (formatCheck(file_name, local))
		{
			addVideoItem(file_name, local);
		}
	}
}

void myPlayerQt::showVideoInfo(playerMedia *media)
{
	memset(textBuf, 0, 1024);
	double totalTime = media->totalTime;
	int hour = (int)(totalTime / 1000 / 60 / 60);
	int min = (int)(totalTime / 1000 / 60) % 60;
	int sec = (int)(totalTime / 1000) % 60 % 60;
	sprintf(textBuf, "%02d:%02d:%02d", hour, min, sec);
	ui.totalTime->setText(textBuf);

	AVFormatContext * pFormatCtx = media->getAVFormatContext();
	if (pFormatCtx == nullptr)
	{
		return;
	}
	sprintf(textBuf, "��Ƶ���ƣ�%s\n��װ��ʽ��%s\n", 
		pFormatCtx->filename,
		pFormatCtx->iformat->name);
	ui.videoInfo->append(QString::fromLocal8Bit(textBuf));

	sprintf(textBuf, "��Ƶʱ����%02d:%02d:%02d\n", hour, min, sec);
	ui.videoInfo->append(QString::fromLocal8Bit(textBuf));

	AVCodecContext *videoCtx = media->video->getAVCodecContext();
	if (videoCtx)
	{
		sprintf(textBuf, "��Ƶ������: %s\n ��Ƶ�ߴ磺%dx%d\n",
			videoCtx->codec->name,
			videoCtx->width, videoCtx->height);
		ui.videoInfo->append(QString::fromLocal8Bit(textBuf));
	}
	
	AVCodecContext *audioCtx = media->audio->getAVCodecContext();
	if (audioCtx)
	{
		sprintf(textBuf, "��Ƶ������: %s\n�����ʣ�%d\n��������%d\n\n",
			audioCtx->codec->name,
			audioCtx->sample_rate,
			audioCtx->channels);
		ui.videoInfo->append(QString::fromLocal8Bit(textBuf));
	}

}