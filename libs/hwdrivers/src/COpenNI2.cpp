/* +---------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)               |
   |                          http://www.mrpt.org/                             |
   |                                                                           |
   | Copyright (c) 2005-2014, Individual contributors, see AUTHORS file        |
   | See: http://www.mrpt.org/Authors - All rights reserved.                   |
   | Released under BSD License. See details in http://www.mrpt.org/License    |
   +---------------------------------------------------------------------------+ */
//#if MRPT_HAS_OPENNI2

#include <OpenNI.h>
#include <PS1080.h>

#include <mrpt/hwdrivers.h> // Precompiled header

#include <mrpt/hwdrivers/COpenNI2.h>
#include <mrpt/utils/CTimeLogger.h>

// Universal include for all versions of OpenCV
#include <mrpt/otherlibs/do_opencv_includes.h>

using namespace mrpt::hwdrivers;
using namespace mrpt::system;
using namespace mrpt::synch;

IMPLEMENTS_GENERIC_SENSOR(COpenNI2,mrpt::hwdrivers)

// Whether to profile memory allocations:
//#define KINECT_PROFILE_MEM_ALLOC
#ifdef KINECT_PROFILE_MEM_ALLOC
mrpt::utils::CTimeLogger alloc_tim;
#endif

#define DEPTH_STREAM_PTR (reinterpret_cast<openni::VideoStream*>(p_depth_stream))
#define RGB_STREAM_PTR (reinterpret_cast<openni::VideoStream*>(p_rgb_stream))

//int int a; // Check compilation

/*-------------------------------------------------------------
		ctor
 -------------------------------------------------------------*/
COpenNI2::COpenNI2() :
	m_sensorPoseOnRobot(),
	m_preview_window(false),
	m_preview_window_decimation(1),
	m_preview_decim_counter_range(0),
	m_preview_decim_counter_rgb(0),

  width(320),
  height(240),
  fps(30),

	m_relativePoseIntensityWRTDepth(0,-0.02,0, DEG2RAD(-90),DEG2RAD(0),DEG2RAD(-90)),
	m_initial_tilt_angle(360),
	m_user_device_number(0),
	m_grab_image(true),
	m_grab_depth(true),
	m_grab_3D_points(true)
////	m_video_channel(VIDEO_CHANNEL_RGB)
{
//	// Get maximum range:
////	m_maxRange=m_range2meters[KINECT_RANGES_TABLE_LEN-2];  // Recall: r[Max-1] means error.

	// Default label:
	m_sensorLabel = "OPENNI2";

	// =========== Default params ===========
	// ----- RGB -----
	m_cameraParamsRGB.ncols = 640; // By default set 640x480, on connect we'll update this.
	m_cameraParamsRGB.nrows = 480;

	m_cameraParamsRGB.cx(328.94272028759258);
	m_cameraParamsRGB.cy(267.48068171871557);
	m_cameraParamsRGB.fx(529.2151);
	m_cameraParamsRGB.fy(525.5639);

	m_cameraParamsRGB.dist.zeros();

	// ----- Depth -----
	m_cameraParamsDepth.ncols = 640;
	m_cameraParamsDepth.nrows = 488;

	m_cameraParamsDepth.cx(339.30781);
	m_cameraParamsDepth.cy(242.7391);
	m_cameraParamsDepth.fx(594.21434);
	m_cameraParamsDepth.fy(591.04054);

	m_cameraParamsDepth.dist.zeros();
}

/*-------------------------------------------------------------
			dtor
 -------------------------------------------------------------*/
COpenNI2::~COpenNI2()
{
	this->close();
}

/** This method can or cannot be implemented in the derived class, depending on the need for it.
*  \exception This method must throw an exception with a descriptive message if some critical error is found.
*/
void COpenNI2::initialize()
{
  int rc = openni::OpenNI::initialize();
	if(rc != openni::STATUS_OK)
    THROW_EXCEPTION(mrpt::format("After initialization:\n %s\n", openni::OpenNI::getExtendedError()))
//  printf("After initialization:\n %s\n", openni::OpenNI::getExtendedError());

  // Show devices list
  openni::Array<openni::DeviceInfo> deviceList;
  openni::OpenNI::enumerateDevices(&deviceList);
  printf("Get device list. %d devices connected\n", deviceList.getSize() );
  for (unsigned i=0; i < deviceList.getSize(); i++)
  {
    int product_id = deviceList[i].getUsbProductId();
    printf("Device %u: name=%s uri=%s vendor=%s product=%i \n", i+1 , deviceList[i].getName(), deviceList[i].getUri(), deviceList[i].getVendor(), product_id);
  }
  if(deviceList.getSize() == 0)
  {
    cout << "No devices connected -> EXIT\n";
    return;
  }

	open();
}

/** This method will be invoked at a minimum rate of "process_rate" (Hz)
*  \exception This method must throw an exception with a descriptive message if some critical error is found.
*/
void COpenNI2::doProcess()
{
	bool	thereIs, hwError;

	CObservation3DRangeScanPtr newObs = CObservation3DRangeScan::Create();

	getNextObservation( *newObs, thereIs, hwError );

	if (hwError)
	{
		m_state = ssError;
	    THROW_EXCEPTION("Couldn't communicate to the Kinect sensor!");
	}

	if (thereIs)
	{
		m_state = ssWorking;

		vector<CSerializablePtr> objs;
		if (m_grab_image || m_grab_depth || m_grab_3D_points)  objs.push_back(newObs);

		appendObservations( objs );
	}
}

/** Loads specific configuration for the device from a given source of configuration parameters, for example, an ".ini" file, loading from the section "[iniSection]" (see utils::CConfigFileBase and derived classes)
*  \exception This method must throw an exception with a descriptive message if some critical parameter is missing or has an invalid value.
*/
void  COpenNI2::loadConfig_sensorSpecific(
	const mrpt::utils::CConfigFileBase &configSource,
	const std::string			&iniSection )
{
	m_sensorPoseOnRobot.setFromValues(
		configSource.read_float(iniSection,"pose_x",0),
		configSource.read_float(iniSection,"pose_y",0),
		configSource.read_float(iniSection,"pose_z",0),
		DEG2RAD( configSource.read_float(iniSection,"pose_yaw",0) ),
		DEG2RAD( configSource.read_float(iniSection,"pose_pitch",0) ),
		DEG2RAD( configSource.read_float(iniSection,"pose_roll",0) )
		);

	m_preview_window = configSource.read_bool(iniSection,"preview_window",m_preview_window);

  width = configSource.read_int(iniSection,"width",0);
  height = configSource.read_int(iniSection,"height",0);
  fps = configSource.read_float(iniSection,"fps",0);
std::cout << "width " << width << " height " << height << " fps " << fps << endl;

	const mrpt::poses::CPose3D twist(0,0,0,DEG2RAD(-90),DEG2RAD(0),DEG2RAD(-90));

	mrpt::utils::TStereoCamera  sc;
	sc.leftCamera  = m_cameraParamsDepth;  // Load default values so that if we fail to load from cfg at least we have some reasonable numbers.
	sc.rightCamera = m_cameraParamsRGB;
	sc.rightCameraPose = mrpt::poses::CPose3DQuat(m_relativePoseIntensityWRTDepth - twist);

	try {
		sc.loadFromConfigFile(iniSection,configSource);
	} catch (std::exception &e) {
		std::cout << "[COpenNI2::loadConfig_sensorSpecific] Warning: Ignoring error loading calibration parameters:\n" << e.what();
	}
	m_cameraParamsDepth = sc.leftCamera;
	m_cameraParamsRGB   = sc.rightCamera;
	m_relativePoseIntensityWRTDepth = twist + mrpt::poses::CPose3D(sc.rightCameraPose);

	// Id:
	m_user_device_number = configSource.read_int(iniSection,"device_number",m_user_device_number );

	m_grab_image = configSource.read_bool(iniSection,"grab_image",m_grab_image);
	m_grab_depth = configSource.read_bool(iniSection,"grab_depth",m_grab_depth);
	m_grab_3D_points = configSource.read_bool(iniSection,"grab_3D_points",m_grab_3D_points);
//	m_grab_IMU = configSource.read_bool(iniSection,"grab_IMU",m_grab_IMU );

//	m_video_channel = configSource.read_enum<TVideoChannel>(iniSection,"video_channel",m_video_channel);

	{
		std::string s = configSource.read_string(iniSection,"relativePoseIntensityWRTDepth","");
		if (!s.empty())
			m_relativePoseIntensityWRTDepth.fromString(s);
	}

	m_initial_tilt_angle = configSource.read_int(iniSection,"initial_tilt_angle",m_initial_tilt_angle);
}

bool COpenNI2::isOpen() const
{

}

//
////#if MRPT_HAS_KINECT_FREENECT
////// ========  GLOBAL CALLBACK FUNCTIONS ========
////void depth_cb(freenect_device *dev, void *v_depth, uint32_t timestamp)
////{
////	const freenect_frame_mode frMode = freenect_get_current_video_mode(dev);
////
////	uint16_t *depth = reinterpret_cast<uint16_t *>(v_depth);
////
////	COpenNI2 *obj = reinterpret_cast<COpenNI2*>(freenect_get_user(dev));
////
////	// Update of the timestamps at the end:
////	CObservation3DRangeScan &obs = obj->internal_latest_obs();
////	mrpt::synch::CCriticalSectionLocker lock( &obj->internal_latest_obs_cs() );
////
////	obs.hasRangeImage  = true;
////	obs.range_is_depth = true;
////
////#ifdef KINECT_PROFILE_MEM_ALLOC
////	alloc_tim.enter("depth_cb alloc");
////#endif
////
////	// This method will try to exploit memory pooling if possible:
////	obs.rangeImage_setSize(frMode.height,frMode.width);
////
////#ifdef KINECT_PROFILE_MEM_ALLOC
////	alloc_tim.leave("depth_cb alloc");
////#endif
////
////	const COpenNI2::TDepth2RangeArray &r2m = obj->getRawDepth2RangeConversion();
////	for (int r=0;r<frMode.height;r++)
////		for (int c=0;c<frMode.width;c++)
////		{
////			// For now, quickly save the depth as it comes from the sensor, it'll
////			//  transformed later on in getNextObservation()
////			const uint16_t v = *depth++;
////			obs.rangeImage.coeffRef(r,c) = r2m[v & KINECT_RANGES_TABLE_MASK];
////		}
////	obj->internal_tim_latest_depth() = timestamp;
////}
////
////void rgb_cb(freenect_device *dev, void *img_data, uint32_t timestamp)
////{
////	COpenNI2 *obj = reinterpret_cast<COpenNI2*>(freenect_get_user(dev));
////	const freenect_frame_mode frMode = freenect_get_current_video_mode(dev);
////
////	// Update of the timestamps at the end:
////	CObservation3DRangeScan &obs = obj->internal_latest_obs();
////	mrpt::synch::CCriticalSectionLocker lock( &obj->internal_latest_obs_cs() );
////
////#ifdef KINECT_PROFILE_MEM_ALLOC
////	alloc_tim.enter("depth_rgb loadFromMemoryBuffer");
////#endif
////
////	obs.hasIntensityImage = true;
////	if (obj->getVideoChannel()==COpenNI2::VIDEO_CHANNEL_RGB)
////	{
////	     // Color image: We asked for Bayer data, so we can decode it outselves here
////	     //  and avoid having to reorder Green<->Red channels, as would be needed with
////	     //  the RGB image from freenect.
////          obs.intensityImageChannel = mrpt::slam::CObservation3DRangeScan::CH_VISIBLE;
////          obs.intensityImage.resize(frMode.width, frMode.height, CH_RGB, true /* origin=top-left */ );
////
////#if MRPT_HAS_OPENCV
////#	if MRPT_OPENCV_VERSION_NUM<0x200
////		  // Version for VERY OLD OpenCV versions:
////		  IplImage *src_img_bayer = cvCreateImageHeader(cvSize(frMode.width,frMode.height),8,1);
////		  src_img_bayer->imageDataOrigin = reinterpret_cast<char*>(img_data);
////		  src_img_bayer->imageData = src_img_bayer->imageDataOrigin;
////		  src_img_bayer->widthStep = frMode.width;
////
////		  IplImage *dst_img_RGB = obs.intensityImage.getAs<IplImage>();
////
////          // Decode Bayer image:
////		  cvCvtColor(src_img_bayer, dst_img_RGB, CV_BayerGB2BGR);
////
////#	else
////		  // Version for modern OpenCV:
////          const cv::Mat  src_img_bayer( frMode.height, frMode.width, CV_8UC1, img_data, frMode.width );
////
////          cv::Mat        dst_img_RGB= cv::cvarrToMat( obs.intensityImage.getAs<IplImage>(), false /* dont copy buffers */ );
////
////          // Decode Bayer image:
////          cv::cvtColor(src_img_bayer, dst_img_RGB, CV_BayerGB2BGR);
////#	endif
////#else
////     THROW_EXCEPTION("Need building with OpenCV!")
////#endif
////
////	}
////	else
////	{
////	     // IR data: grayscale 8bit
////          obs.intensityImageChannel = mrpt::slam::CObservation3DRangeScan::CH_IR;
////          obs.intensityImage.loadFromMemoryBuffer(
////               frMode.width,
////               frMode.height,
////               false, // Color image?
////               reinterpret_cast<unsigned char*>(img_data)
////               );
////
////	}
////
////	//obs.intensityImage.setChannelsOrder_RGB();
////
////#ifdef KINECT_PROFILE_MEM_ALLOC
////	alloc_tim.leave("depth_rgb loadFromMemoryBuffer");
////#endif
////
////	obj->internal_tim_latest_rgb() = timestamp;
////}
////// ========  END OF GLOBAL CALLBACK FUNCTIONS ========
////#endif // MRPT_HAS_KINECT_FREENECT
////
//
void COpenNI2::open()
{
//	if(isOpen())
//		close();

	// Alloc memory, if this is the first time:
	m_buf_depth.resize(640*480*3); // We'll resize this below if needed
	m_buf_rgb.resize(640*480*3);

  openni::Array<openni::DeviceInfo> deviceList;
  openni::OpenNI::enumerateDevices(&deviceList);
	int nr_devices = deviceList.getSize();
	//printf("[COpenNI2] Number of devices found: %d\n", nr_devices);

	if (!nr_devices)
		THROW_EXCEPTION("No Kinect devices found.")

	// Open the given device number:
	openni::Device		device;
	const char* deviceURI = openni::ANY_DEVICE;
	int rc = device.open(deviceURI);
	if(rc != openni::STATUS_OK)
	{
    THROW_EXCEPTION_CUSTOM_MSG1("Device open failed:\n%s\n", openni::OpenNI::getExtendedError());
//		printf("Device open failed:\n%s\n", openni::OpenNI::getExtendedError());
		openni::OpenNI::shutdown();
	}
	//cout << endl << "Do we have IR sensor? " << device.hasSensor(openni::SENSOR_IR);
	//cout << endl << "Do we have RGB sensor? " << device.hasSensor(openni::SENSOR_COLOR);
	//cout << endl << "Do we have Depth sensor? " << device.hasSensor(openni::SENSOR_DEPTH);

	//								Create RGB and Depth channels
	//========================================================================================
	p_depth_stream = new openni::VideoStream;
	p_rgb_stream = new openni::VideoStream;
	rc = DEPTH_STREAM_PTR->create(device, openni::SENSOR_DEPTH);
	if (rc == openni::STATUS_OK)
	{
		rc = DEPTH_STREAM_PTR->start();
		if (rc != openni::STATUS_OK)
		{
      THROW_EXCEPTION_CUSTOM_MSG1("Couldn't start depth stream:\n%s\n", openni::OpenNI::getExtendedError());
//			printf("Couldn't start depth stream:\n%s\n", openni::OpenNI::getExtendedError());
			DEPTH_STREAM_PTR->destroy();
		}
	}
	else
	{
		printf("Couldn't find depth stream:\n%s\n", openni::OpenNI::getExtendedError());
	}

	rc = RGB_STREAM_PTR->create(device, openni::SENSOR_COLOR);
	if (rc == openni::STATUS_OK)
	{
		rc = RGB_STREAM_PTR->start();
		if (rc != openni::STATUS_OK)
		{
      THROW_EXCEPTION_CUSTOM_MSG1("Couldn't start infrared stream:\n%s\n", openni::OpenNI::getExtendedError());
//			printf("Couldn't start infrared stream:\n%s\n", openni::OpenNI::getExtendedError());
			RGB_STREAM_PTR->destroy();
		}
	}
	else
	{
		printf("Couldn't find infrared stream:\n%s\n", openni::OpenNI::getExtendedError());
	}

	if (!DEPTH_STREAM_PTR->isValid() || !RGB_STREAM_PTR->isValid())
	{
		printf("No valid streams. Exiting\n");
		openni::OpenNI::shutdown();
		return;
	}

	if (rc != openni::STATUS_OK)
	{
		openni::OpenNI::shutdown();
		return;
	}

	//						Configure some properties (resolution)
	//========================================================================================
	openni::VideoMode	options;
	if (device.isImageRegistrationModeSupported(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR))
		rc = device.setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);
	else
		cout << "Device doesn't do image registration!" << endl;

  if (device.setDepthColorSyncEnabled(true) == openni::STATUS_OK)
    cout << "setDepthColorSyncEnabled" << endl;
  else
    cout << "setDepthColorSyncEnabled failed!" << endl;

	//rc = device.setImageRegistrationMode(openni::IMAGE_REGISTRATION_OFF);

	options = RGB_STREAM_PTR->getVideoMode();
	printf("\nInitial resolution RGB (%d, %d)", options.getResolutionX(), options.getResolutionY());
	options.setResolution(width,height);
	options.setFps(fps);
	options.setPixelFormat(openni::PIXEL_FORMAT_RGB888);
	rc = RGB_STREAM_PTR->setVideoMode(options);
	if (rc!=openni::STATUS_OK)
	{
		printf("Failed to change RGB resolution!\n");
		return;
	}
	rc = RGB_STREAM_PTR->setMirroringEnabled(false);

	options = DEPTH_STREAM_PTR->getVideoMode();
	printf("\nInitial resolution Depth(%d, %d)", options.getResolutionX(), options.getResolutionY());
	options.setResolution(width,height);
	options.setFps(30);
	options.setPixelFormat(openni::PIXEL_FORMAT_DEPTH_1_MM);
	rc = DEPTH_STREAM_PTR->setVideoMode(options);
	if (rc!=openni::STATUS_OK)
	{
		printf("Failed to change depth resolution!\n");
		return;
	}
	rc = DEPTH_STREAM_PTR->setMirroringEnabled(false);

	options = DEPTH_STREAM_PTR->getVideoMode();
	printf("\nNew resolution (%d, %d) \n", options.getResolutionX(), options.getResolutionY());

	//Allow detection of closer points (although they will flicker)
	//bool CloseRange;
	//DEPTH_STREAM_PTR->setProperty(XN_STREAM_PROPERTY_CLOSE_RANGE, 1);
	//DEPTH_STREAM_PTR->getProperty(XN_STREAM_PROPERTY_CLOSE_RANGE, &CloseRange);
	//printf("\nClose range: %s", CloseRange?"On":"Off");



//	// Setup:
//	if(m_initial_tilt_angle!=360) // 360 means no motor command.
//    setTiltAngleDegrees(m_initial_tilt_angle);
//
//	// rgb or IR channel:
//	const freenect_frame_mode desiredFrMode = freenect_find_video_mode(
//		FREENECT_RESOLUTION_MEDIUM,
//		m_video_channel==VIDEO_CHANNEL_IR ?
//			FREENECT_VIDEO_IR_8BIT
//			:
//			FREENECT_VIDEO_BAYER // FREENECT_VIDEO_RGB: Use Bayer instead so we can directly decode it here
//		);
//
//	// Switch to that video mode:
//	if (freenect_set_video_mode(f_dev, desiredFrMode)<0)
//		THROW_EXCEPTION("Error setting Kinect video mode.")
//
//
//	// Get video mode:
//	const freenect_frame_mode frMode = freenect_get_current_video_mode(f_dev);
//
	// Realloc mem:
	m_buf_depth.resize(width*height*3);
	m_buf_rgb.resize(width*height*3);

	// Save resolution:
	m_cameraParamsRGB.ncols = width;
	m_cameraParamsRGB.nrows = height;

	m_cameraParamsDepth.ncols = width;
	m_cameraParamsDepth.nrows = height;
//
//	freenect_set_video_buffer(f_dev, &m_buf_rgb[0]);
//	freenect_set_depth_buffer(f_dev, &m_buf_depth[0]);
//
//	freenect_set_depth_mode(f_dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_10BIT));
//
//	// Set user data = pointer to "this":
//	freenect_set_user(f_dev, this);
//
//	if (freenect_start_depth(f_dev)<0)
//		THROW_EXCEPTION("Error starting depth streaming.")
//
//	if (freenect_start_video(f_dev)<0)
//		THROW_EXCEPTION("Error starting video streaming.")

}

void COpenNI2::close()
{
  delete DEPTH_STREAM_PTR;
  delete RGB_STREAM_PTR;
	openni::OpenNI::shutdown();
}


//
/////** Changes the video channel to open (RGB or IR) - you can call this method before start grabbing or in the middle of streaming and the video source will change on the fly.
////	Default is RGB channel.
////*/
////void  COpenNI2::setVideoChannel(const TVideoChannel vch)
////{
////#if MRPT_HAS_KINECT_FREENECT
////	m_video_channel = vch;
////	if (!isOpen()) return; // Nothing else to do here.
////
////	// rgb or IR channel:
////	freenect_stop_video(f_dev);
////
////	// rgb or IR channel:
////	const freenect_frame_mode desiredFrMode = freenect_find_video_mode(
////		FREENECT_RESOLUTION_MEDIUM,
////		m_video_channel==VIDEO_CHANNEL_IR ?
////			FREENECT_VIDEO_IR_8BIT
////			:
////			FREENECT_VIDEO_BAYER // FREENECT_VIDEO_RGB: Use Bayer instead so we can directly decode it here
////		);
////
////	// Switch to that video mode:
////	if (freenect_set_video_mode(f_dev, desiredFrMode)<0)
////		THROW_EXCEPTION("Error setting Kinect video mode.")
////
////	freenect_start_video(f_dev);
////
////#endif // MRPT_HAS_KINECT_FREENECT
////
////#if MRPT_HAS_KINECT_CL_NUI
////	THROW_EXCEPTION("Grabbing IR intensity is not available with CL NUI Kinect driver.")
////#endif // MRPT_HAS_KINECT_CL_NUI
////
////
////}
////
//
/** The main data retrieving function, to be called after calling loadConfig() and initialize().
  *  \param out_obs The output retrieved observation (only if there_is_obs=true).
  *  \param there_is_obs If set to false, there was no new observation.
  *  \param hardware_error True on hardware/comms error.
  *
  * \sa doProcess
  */
void COpenNI2::getNextObservation(
	mrpt::slam::CObservation3DRangeScan &_out_obs,
	bool &there_is_obs,
	bool &hardware_error )
{
//	there_is_obs=false;
//	hardware_error = false;
//
//#if MRPT_HAS_KINECT_FREENECT
//
//	static const double max_wait_seconds = 1./25.;
//	static const TTimeStamp max_wait = mrpt::system::secondsToTimestamp(max_wait_seconds);
//
//	// Mark previous observation's timestamp as out-dated:
//	m_latest_obs.hasPoints3D        = false;
//	m_latest_obs.hasRangeImage      = false;
//	m_latest_obs.hasIntensityImage  = false;
//	m_latest_obs.hasConfidenceImage = false;
//
//	const TTimeStamp tim0 = mrpt::system::now();
//
//	// Reset these timestamp flags so if they are !=0 in the next call we're sure they're new frames.
//	m_latest_obs_cs.enter();
//	m_tim_latest_rgb   = 0;
//	m_tim_latest_depth = 0;
//	m_latest_obs_cs.leave();
//
//	while (freenect_process_events(f_ctx)>=0 && mrpt::system::now()<(tim0+max_wait) )
//	{
//		// Got a new frame?
//		if ( (!m_grab_image || m_tim_latest_rgb!=0) &&   // If we are NOT grabbing RGB or we are and there's a new frame...
//			 (!m_grab_depth || m_tim_latest_depth!=0)    // If we are NOT grabbing Depth or we are and there's a new frame...
//		   )
//		{
//			// Approx: 0.5ms delay between depth frame (first) and RGB frame (second).
//			//cout << "m_tim_latest_rgb: " << m_tim_latest_rgb << " m_tim_latest_depth: "<< m_tim_latest_depth <<endl;
//			there_is_obs=true;
//			break;
//		}
//	}
//
//	// Handle the case when there is NOT depth frames (if there's something very close blocking the IR sensor) but we have RGB:
//	if ( (m_grab_image && m_tim_latest_rgb!=0) &&
//		 (m_grab_depth && m_tim_latest_depth==0) )
//	{
//		// Mark the entire range data as invalid:
//		m_latest_obs.hasRangeImage = true;
//		m_latest_obs.range_is_depth = true;
//
//		m_latest_obs_cs.enter(); // Important: if system is running slow, etc. we cannot tell for sure that the depth buffer is not beeing filled right now:
//		m_latest_obs.rangeImage.setSize(m_cameraParamsDepth.nrows,m_cameraParamsDepth.ncols);
//		m_latest_obs.rangeImage.setConstant(0); // "0" means: error in range
//		m_latest_obs_cs.leave();
//		there_is_obs=true;
//	}
//
//
//	if (!there_is_obs)
//		return;
//
//
//	// We DO have a fresh new observation:
//
//	// Quick save the observation to the user's object:
//	m_latest_obs_cs.enter();
//		_out_obs.swap(m_latest_obs);
//	m_latest_obs_cs.leave();
//
//#elif MRPT_HAS_KINECT_CL_NUI
//
//	const int waitTimeout = 200;
//	const bool there_is_rgb   = GetNUICameraColorFrameRGB24(m_clnui_cam, &m_buf_rgb[0],waitTimeout);
//	const bool there_is_depth = GetNUICameraDepthFrameRAW(m_clnui_cam, (PUSHORT)&m_buf_depth[0],waitTimeout);
//
//	there_is_obs = (!m_grab_image  || there_is_rgb) &&
//	               (!m_grab_depth  || there_is_depth);
//
//	if (!there_is_obs)
//		return;
//
//	// We DO have a fresh new observation:
//	{
//		CObservation3DRangeScan  newObs;
//
//		newObs.hasConfidenceImage = false;
//
//		// Set intensity image ----------------------
//		if (m_grab_image)
//		{
//			newObs.hasIntensityImage  = true;
//			newObs.intensityImageChannel = mrpt::slam::CObservation3DRangeScan::CH_VISIBLE;
//			newObs.intensityImage.loadFromMemoryBuffer(KINECT_W,KINECT_H,true,&m_buf_rgb[0]);
//		}
//
//		// Set range image --------------------------
//		if (m_grab_depth || m_grab_3D_points)
//		{
//			newObs.hasRangeImage = true;
//			newObs.range_is_depth = true;
//			newObs.rangeImage.setSize(KINECT_H,KINECT_W);
//			PUSHORT depthPtr = (PUSHORT)&m_buf_depth[0];
//			for (int r=0;r<KINECT_H;r++)
//				for (int c=0;c<KINECT_W;c++)
//				{
//					const uint16_t v = (*depthPtr++);
//					newObs.rangeImage.coeffRef(r,c) = m_range2meters[v & KINECT_RANGES_TABLE_MASK];
//				}
//		}
//
//		// Save 3D point cloud ---------------------
//		// 3d points are generated above, in the code common to libfreenect & CL NUI.
//
//
//		// Save the observation to the user's object:
//		_out_obs.swap(newObs);
//	}
//
//#endif  // end MRPT_HAS_KINECT_CL_NUI
//
//	// Set common data into observation:
//	// --------------------------------------
//	_out_obs.sensorLabel = m_sensorLabel;
//	_out_obs.timestamp = mrpt::system::now();
//	_out_obs.sensorPose = m_sensorPoseOnRobot;
//	_out_obs.relativePoseIntensityWRTDepth = m_relativePoseIntensityWRTDepth;
//
//	_out_obs.cameraParams          = m_cameraParamsDepth;
//	_out_obs.cameraParamsIntensity = m_cameraParamsRGB;
//
//	// 3D point cloud:
//	if ( _out_obs.hasRangeImage && m_grab_3D_points )
//	{
//		_out_obs.project3DPointsFromDepthImage();
//
//		if ( !m_grab_depth )
//		{
//			_out_obs.hasRangeImage = false;
//			_out_obs.rangeImage.resize(0,0);
//		}
//
//	}
//
//	// preview in real-time?
//	if (m_preview_window)
//	{
//		if ( _out_obs.hasRangeImage )
//		{
//			if (++m_preview_decim_counter_range>m_preview_window_decimation)
//			{
//				m_preview_decim_counter_range=0;
//				if (!m_win_range)	{ m_win_range = mrpt::gui::CDisplayWindow::Create("Preview RANGE"); m_win_range->setPos(5,5); }
//
//				// Normalize the image
//				mrpt::utils::CImage  img;
//				img.setFromMatrix(_out_obs.rangeImage);
//				CMatrixFloat r = _out_obs.rangeImage * float(1.0/this->m_maxRange);
//				m_win_range->showImage(img);
//			}
//		}
//		if ( _out_obs.hasIntensityImage )
//		{
//			if (++m_preview_decim_counter_rgb>m_preview_window_decimation)
//			{
//				m_preview_decim_counter_rgb=0;
//				if (!m_win_int)		{ m_win_int = mrpt::gui::CDisplayWindow::Create("Preview INTENSITY"); m_win_int->setPos(300,5); }
//				m_win_int->showImage(_out_obs.intensityImage );
//			}
//		}
//	}
//	else
//	{
//		if (m_win_range) m_win_range.clear();
//		if (m_win_int) m_win_int.clear();
//	}
}
//
//

/* -----------------------------------------------------
				setPathForExternalImages
----------------------------------------------------- */
void COpenNI2::setPathForExternalImages( const std::string &directory )
{
	// Ignore for now. It seems performance is better grabbing everything
	// to a single big file than creating hundreds of smaller files per second...
	return;

//	if (!mrpt::system::createDirectory( directory ))
//	{
//		THROW_EXCEPTION_CUSTOM_MSG1("Error: Cannot create the directory for externally saved images: %s",directory.c_str() )
//	}
//	m_path_for_external_images = directory;
}

////
/////** Change tilt angle \note Sensor must be open first. */
////void COpenNI2::setTiltAngleDegrees(double angle)
////{
////	ASSERTMSG_(isOpen(),"Sensor must be open first")
////
////#if MRPT_HAS_KINECT_FREENECT
////	freenect_set_tilt_degs(f_dev,angle);
////#elif MRPT_HAS_KINECT_CL_NUI
////	// JL: I deduced this formula empirically, since CLNUI seems not to have documented this API!!
////	const short int send_angle =  ( (angle<-31) ? -31 : ((angle>31) ? 31 : angle) ) * (0x4000)/31.0;
////	SetNUIMotorPosition(clnui_motor, send_angle);
////#endif
////
////}
////
////double COpenNI2::getTiltAngleDegrees()
////{
////	ASSERTMSG_(isOpen(),"Sensor must be open first")
////
////#if MRPT_KINECT_WITH_FREENECT
////	freenect_update_tilt_state(f_dev);
////	freenect_raw_tilt_state *ts=freenect_get_tilt_state(f_dev);
////	return freenect_get_tilt_degs(ts);
////
////#elif MRPT_HAS_KINECT_CL_NUI
////	// TODO: Does CL NUI provides this??
////	return 0;
////#else
////	return 0;
////#endif
////}
//
////#endif
