// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/contexts/camera_pan_tilt_zoom_permission_context.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

using content_settings::PageSpecificContentSettings;

class MediaStreamDevicesControllerTest : public WebRtcTestBase {
 public:
  MediaStreamDevicesControllerTest()
      : example_audio_id_("fake_audio_dev"),
        example_video_id_("fake_video_dev"),
        media_stream_result_(
            blink::mojom::MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS) {
    // `kLeftHandSideActivityIndicators` should be disabled as it changes the UI
    // of the camera/mic activity indicator. The new UI will be tested
    // separately.
    scoped_feature_list_.InitWithFeatures(
        {}, {content_settings::features::kLeftHandSideActivityIndicators});
  }

  void OnMediaStreamResponse(
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      blink::mojom::MediaStreamRequestResult result,
      std::unique_ptr<content::MediaStreamUI> ui) {
    blink::MediaStreamDevices devices_list =
        blink::ToMediaStreamDevicesList(stream_devices_set);
    EXPECT_EQ(devices_list.empty(), !ui);
    media_stream_devices_ = devices_list;
    media_stream_result_ = result;
    std::move(quit_closure_).Run();
  }

 protected:
  enum DeviceType { DEVICE_TYPE_AUDIO, DEVICE_TYPE_VIDEO };
  enum Access { ACCESS_ALLOWED, ACCESS_DENIED };

  const GURL& example_url() const { return example_url_; }

  PageSpecificContentSettings* GetContentSettings() {
    return PageSpecificContentSettings::GetForFrame(
        GetWebContents()->GetPrimaryMainFrame());
  }

  const std::string& example_audio_id() const { return example_audio_id_; }
  const std::string& example_video_id() const { return example_video_id_; }

  blink::mojom::MediaStreamRequestResult media_stream_result() const {
    return media_stream_result_;
  }

  void RequestPermissions(content::WebContents* web_contents,
                          const content::MediaStreamRequest& request) {
    base::RunLoop run_loop;
    ASSERT_TRUE(quit_closure_.is_null());
    quit_closure_ = run_loop.QuitClosure();
    permission_bubble_media_access_handler_->HandleRequest(
        web_contents, request,
        base::BindOnce(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                       base::Unretained(this)),
        nullptr);
    run_loop.Run();
  }

  // Sets the device policy-controlled |access| for |example_url_| to be for the
  // selected |device_type|.
  void SetDevicePolicy(DeviceType device_type, Access access) {
    PrefService* prefs = Profile::FromBrowserContext(
        GetWebContents()->GetBrowserContext())->GetPrefs();
    const char* policy_name = nullptr;
    switch (device_type) {
      case DEVICE_TYPE_AUDIO:
        policy_name = prefs::kAudioCaptureAllowed;
        break;
      case DEVICE_TYPE_VIDEO:
        policy_name = prefs::kVideoCaptureAllowed;
        break;
    }
    prefs->SetBoolean(policy_name, access == ACCESS_ALLOWED);
  }

  // Set the content settings for mic/cam/ptz.
  void SetContentSettings(ContentSetting mic_setting,
                          ContentSetting cam_setting,
                          ContentSetting ptz_setting) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(
            Profile::FromBrowserContext(GetWebContents()->GetBrowserContext()));
    content_settings->SetContentSettingDefaultScope(
        example_url_, GURL(), ContentSettingsType::MEDIASTREAM_MIC,
        mic_setting);
    content_settings->SetContentSettingDefaultScope(
        example_url_, GURL(), ContentSettingsType::MEDIASTREAM_CAMERA,
        cam_setting);
    content_settings->SetContentSettingDefaultScope(
        example_url_, GURL(), ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
        ptz_setting);
  }

  // Checks whether the devices returned in OnMediaStreamResponse contains a
  // microphone and/or camera device.
  bool CheckDevicesListContains(blink::mojom::MediaStreamType type) {
    for (const auto& device : media_stream_devices_) {
      if (device.type == type) {
        return true;
      }
    }
    return false;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Creates a MediaStreamRequest, asking for those media types, which have a
  // non-empty id string.
  content::MediaStreamRequest CreateRequestWithType(
      const std::string& audio_id,
      const std::string& video_id,
      bool request_pan_tilt_zoom_permission,
      blink::MediaStreamRequestType request_type) {
    blink::mojom::MediaStreamType audio_type =
        audio_id.empty() ? blink::mojom::MediaStreamType::NO_SERVICE
                         : blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
    blink::mojom::MediaStreamType video_type =
        video_id.empty() ? blink::mojom::MediaStreamType::NO_SERVICE
                         : blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;
    if (!GetWebContents()
             ->GetPrimaryMainFrame()
             ->GetLastCommittedOrigin()
             .GetURL()
             .is_empty()) {
      EXPECT_EQ(example_url().DeprecatedGetOriginAsURL(),
                GetWebContents()
                    ->GetPrimaryMainFrame()
                    ->GetLastCommittedOrigin()
                    .GetURL());
    }
    int render_process_id =
        GetWebContents()->GetPrimaryMainFrame()->GetProcess()->GetID();
    int render_frame_id =
        GetWebContents()->GetPrimaryMainFrame()->GetRoutingID();
    return content::MediaStreamRequest(
        render_process_id, render_frame_id, 0,
        url::Origin::Create(example_url()), false, request_type,
        /*requested_audio_device_ids=*/{audio_id},
        /*requested_video_device_ids=*/{video_id}, audio_type, video_type,
        /*disable_local_echo=*/false, request_pan_tilt_zoom_permission,
        /*captured_surface_control_active=*/false);
  }

  content::MediaStreamRequest CreateRequest(
      const std::string& audio_id,
      const std::string& video_id,
      bool request_pan_tilt_zoom_permission) {
    return CreateRequestWithType(audio_id, video_id,
                                 request_pan_tilt_zoom_permission,
                                 blink::MEDIA_GENERATE_STREAM);
  }

  void InitWithUrl(const GURL& url) {
    DCHECK(example_url_.is_empty());
    example_url_ = url;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), example_url_));
    EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().empty());
  }

  virtual media::VideoCaptureControlSupport GetControlSupport() const {
    return media::VideoCaptureControlSupport();
  }

  permissions::MockPermissionPromptFactory* prompt_factory() {
    return prompt_factory_.get();
  }

  void VerifyResultState(blink::mojom::MediaStreamRequestResult result,
                         bool has_audio,
                         bool has_video) {
    EXPECT_EQ(result, media_stream_result());
    EXPECT_EQ(has_audio,
              CheckDevicesListContains(
                  blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
    EXPECT_EQ(has_video,
              CheckDevicesListContains(
                  blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
  }

  void SetUpOnMainThread() override {
    WebRtcTestBase::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    permission_bubble_media_access_handler_ =
        std::make_unique<PermissionBubbleMediaAccessHandler>();

    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    // Cleanup.
    media_stream_devices_.clear();
    media_stream_result_ =
        blink::mojom::MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS;

    blink::MediaStreamDevices audio_devices;
    blink::MediaStreamDevice fake_audio_device(
        blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, example_audio_id_,
        "Fake Audio Device");
    audio_devices.push_back(fake_audio_device);
    MediaCaptureDevicesDispatcher::GetInstance()->SetTestAudioCaptureDevices(
        audio_devices);

    blink::MediaStreamDevices video_devices;
    blink::MediaStreamDevice fake_video_device(
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, example_video_id_,
        "Fake Video Device", GetControlSupport(),
        media::MEDIA_VIDEO_FACING_NONE, std::nullopt);
    video_devices.push_back(fake_video_device);
    MediaCaptureDevicesDispatcher::GetInstance()->SetTestVideoCaptureDevices(
        video_devices);
  }

  void TearDownOnMainThread() override {
    permission_bubble_media_access_handler_.reset();
    prompt_factory_.reset();

    WebRtcTestBase::TearDownOnMainThread();
  }

  GURL example_url_;
  const std::string example_audio_id_;
  const std::string example_video_id_;

  blink::MediaStreamDevices media_stream_devices_;
  blink::mojom::MediaStreamRequestResult media_stream_result_;

  base::OnceClosure quit_closure_;

  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;

  std::unique_ptr<PermissionBubbleMediaAccessHandler>
      permission_bubble_media_access_handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class MediaStreamDevicesControllerPtzTest
    : public MediaStreamDevicesControllerTest,
      public ::testing::WithParamInterface<media::VideoCaptureControlSupport> {
 protected:
  media::VideoCaptureControlSupport GetControlSupport() const override {
    return GetParam();
  }
};

// Request and allow microphone access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest, RequestAndAllowMic) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_ALLOWED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(GetWebContents(),
                     CreateRequest(example_audio_id(), std::string(), false));

  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kMicrophoneAccessed));
}

// Request and allow camera access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest, RequestAndAllowCam) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_ALLOWED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(GetWebContents(),
                     CreateRequest(std::string(), example_video_id(), false));

  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));
}

// Request and block microphone access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest, RequestAndBlockMic) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_DENIED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(GetWebContents(),
                     CreateRequest(example_audio_id(), std::string(), false));

  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().HasAll(
      {PageSpecificContentSettings::kMicrophoneAccessed,
       PageSpecificContentSettings::kMicrophoneBlocked}));
}

// Request and block camera access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest, RequestAndBlockCam) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_DENIED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(GetWebContents(),
                     CreateRequest(std::string(), example_video_id(), false));

  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().HasAll(
      {PageSpecificContentSettings::kCameraAccessed,
       PageSpecificContentSettings::kCameraBlocked}));
}

// Request and allow microphone and camera access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestAndAllowMicCam) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_ALLOWED);
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_ALLOWED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(),
      CreateRequest(example_audio_id(), example_video_id(), false));

  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().HasAll(
      {PageSpecificContentSettings::kMicrophoneAccessed,
       PageSpecificContentSettings::kCameraAccessed}));
}

// Request and block microphone and camera access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestAndBlockMicCam) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_DENIED);
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_DENIED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(),
      CreateRequest(example_audio_id(), example_video_id(), false));

  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().HasAll(
      {PageSpecificContentSettings::kMicrophoneAccessed,
       PageSpecificContentSettings::kMicrophoneBlocked,
       PageSpecificContentSettings::kCameraAccessed,
       PageSpecificContentSettings::kCameraBlocked}));
}

// Request microphone and camera access. Camera is denied, thus everything
// must be denied.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestMicCamBlockCam) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_ALLOWED);
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_DENIED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(),
      CreateRequest(example_audio_id(), example_video_id(), false));

  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().HasAll(
      {PageSpecificContentSettings::kMicrophoneAccessed,
       PageSpecificContentSettings::kMicrophoneBlocked,
       PageSpecificContentSettings::kCameraAccessed,
       PageSpecificContentSettings::kCameraBlocked}));
}

// Request microphone and camera access. Microphone is denied, thus everything
// must be denied.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestMicCamBlockMic) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_DENIED);
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_ALLOWED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(),
      CreateRequest(example_audio_id(), example_video_id(), false));

  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().HasAll(
      {PageSpecificContentSettings::kMicrophoneAccessed,
       PageSpecificContentSettings::kMicrophoneBlocked,
       PageSpecificContentSettings::kCameraAccessed,
       PageSpecificContentSettings::kCameraBlocked}));
}

// Request microphone access. Requesting camera should not change microphone
// state.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestCamDoesNotChangeMic) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  // Request mic and deny.
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_DENIED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(GetWebContents(),
                     CreateRequest(example_audio_id(), std::string(), false));
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));

  // Request cam and allow
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_ALLOWED);
  RequestPermissions(GetWebContents(),
                     CreateRequest(std::string(), example_video_id(), false));
  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  // Mic state should not have changed.
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
}

// Denying mic access after camera access should still show the camera as state.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       DenyMicDoesNotChangeCam) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  // Request cam and allow
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_ALLOWED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(GetWebContents(),
                     CreateRequest(std::string(), example_video_id(), false));
  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  // Simulate that an a video stream is now being captured.
  blink::mojom::StreamDevices devices;
  devices.video_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, example_video_id(),
      example_video_id());
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  dispatcher->SetTestVideoCaptureDevices({devices.video_device.value()});
  std::unique_ptr<content::MediaStreamUI> video_stream_ui =
      dispatcher->GetMediaStreamCaptureIndicator()->RegisterMediaStream(
          GetWebContents(), devices);
  video_stream_ui->OnStarted(base::RepeatingClosure(),
                             content::MediaStreamUI::SourceCallback(),
                             /*label=*/std::string(), /*screen_capture_ids=*/{},
                             content::MediaStreamUI::StateChangeCallback());

  // Request mic and deny.
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_DENIED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(GetWebContents(),
                     CreateRequest(example_audio_id(), std::string(), false));
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));

  // Cam should still be included in the state.
  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().HasAll(
      {PageSpecificContentSettings::kMicrophoneAccessed,
       PageSpecificContentSettings::kMicrophoneBlocked,
       PageSpecificContentSettings::kCameraAccessed}));

  // After ending the camera capture, the camera permission is no longer
  // relevant, so it should no be included in the mic/cam state.
  video_stream_ui.reset();
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().HasAll(
      {PageSpecificContentSettings::kMicrophoneAccessed,
       PageSpecificContentSettings::kMicrophoneBlocked}));
}

// Stores the ContentSettings inputs for a particular test and has functions
// which return the expected outputs for that test.
struct ContentSettingsTestData {
  // The initial value of the mic/cam/ptz content settings.
  ContentSetting mic;
  ContentSetting cam;
  ContentSetting ptz;
  // Whether the infobar should be accepted if it's shown.
  bool accept_infobar;

  // Whether the infobar should be displayed to request mic/cam/ptz for the
  // given content settings inputs.
  bool ExpectMicInfobar(
      const media::VideoCaptureControlSupport& control_support) const {
    if (cam == CONTENT_SETTING_BLOCK)
      return false;
    if (control_support.pan || control_support.tilt || control_support.zoom) {
      if (ptz == CONTENT_SETTING_BLOCK)
        return false;
    }
    return mic == CONTENT_SETTING_ASK;
  }
  bool ExpectCamInfobar(
      const media::VideoCaptureControlSupport& control_support) const {
    if (mic == CONTENT_SETTING_BLOCK)
      return false;
    if (control_support.pan || control_support.tilt || control_support.zoom) {
      if (ptz == CONTENT_SETTING_BLOCK)
        return false;
    }
    return cam == CONTENT_SETTING_ASK;
  }
  bool ExpectPtzInfobar(
      const media::VideoCaptureControlSupport& control_support) const {
    if (mic == CONTENT_SETTING_BLOCK || cam == CONTENT_SETTING_BLOCK)
      return false;
    if (!(control_support.pan || control_support.tilt || control_support.zoom))
      return false;
    return ptz == CONTENT_SETTING_ASK;
  }

  // Whether or not the mic/cam/ptz should be allowed after clicking accept/deny
  // for the given inputs.
  bool ExpectMicAllowed() const {
    return mic == CONTENT_SETTING_ALLOW ||
           (mic == CONTENT_SETTING_ASK && accept_infobar);
  }
  bool ExpectCamAllowed() const {
    return cam == CONTENT_SETTING_ALLOW ||
           (cam == CONTENT_SETTING_ASK && accept_infobar);
  }
  bool ExpectPtzAllowed() const {
    return ptz == CONTENT_SETTING_ALLOW ||
           (ptz == CONTENT_SETTING_ASK && accept_infobar);
  }

  // The expected media stream result after clicking accept/deny for the given
  // inputs.
  blink::mojom::MediaStreamRequestResult ExpectedMediaStreamResult(
      const media::VideoCaptureControlSupport& control_support) const {
    if (ExpectMicAllowed() && ExpectCamAllowed()) {
      if (!control_support.pan && !control_support.tilt &&
          !control_support.zoom) {
        return blink::mojom::MediaStreamRequestResult::OK;
      }
      if (ptz != CONTENT_SETTING_BLOCK) {
        return blink::mojom::MediaStreamRequestResult::OK;
      }
    }
    return blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
  }
};

// Test all combinations of cam/mic content settings. Then tests the result of
// clicking both accept/deny on the infobar. Both cam/mic are requested.
IN_PROC_BROWSER_TEST_P(MediaStreamDevicesControllerPtzTest, ContentSettings) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  static const ContentSettingsTestData tests[] = {
      // Settings that won't result in an infobar.
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW,
       false},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
       false},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW,
       false},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
       false},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW,
       false},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
       false},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW,
       false},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
       false},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, false},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK, false},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK, false},

      // Settings that will result in an infobar. Test both accept and deny.
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
       false},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, true},

      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW,
       false},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, true},

      {CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, false},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, true},

      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, false},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, true},

      {CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, false},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, true},

      {CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, false},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, true},

      {CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, false},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, true},
  };

  // Prevent automatic camera permission change when camera PTZ gets updated.
  permissions::CameraPanTiltZoomPermissionContext*
      camera_pan_tilt_zoom_permission_context =
          static_cast<permissions::CameraPanTiltZoomPermissionContext*>(
              PermissionManagerFactory::GetForProfile(
                  Profile::FromBrowserContext(
                      GetWebContents()->GetBrowserContext()))
                  ->GetPermissionContextForTesting(
                      ContentSettingsType::CAMERA_PAN_TILT_ZOOM));
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(GetWebContents()->GetBrowserContext()));
  content_settings->RemoveObserver(camera_pan_tilt_zoom_permission_context);

  const auto& control_support = GetControlSupport();
  for (auto& test : tests) {
    SetContentSettings(test.mic, test.cam, test.ptz);

    prompt_factory()->ResetCounts();

    // Accept or deny the infobar if it's showing.
    if (test.ExpectMicInfobar(control_support) ||
        test.ExpectCamInfobar(control_support) ||
        test.ExpectPtzInfobar(control_support)) {
      if (test.accept_infobar) {
        prompt_factory()->set_response_type(
            permissions::PermissionRequestManager::ACCEPT_ALL);
      } else {
        prompt_factory()->set_response_type(
            permissions::PermissionRequestManager::DENY_ALL);
      }
    } else {
      prompt_factory()->set_response_type(
          permissions::PermissionRequestManager::NONE);
    }
    RequestPermissions(
        GetWebContents(),
        CreateRequest(example_audio_id(), example_video_id(), true));

    ASSERT_LE(prompt_factory()->TotalRequestCount(), 3);
    EXPECT_EQ(test.ExpectMicInfobar(control_support),
              prompt_factory()->RequestTypeSeen(
                  permissions::RequestType::kMicStream));
    EXPECT_EQ(test.ExpectCamInfobar(control_support),
              prompt_factory()->RequestTypeSeen(
                  permissions::RequestType::kCameraStream));
    EXPECT_EQ(test.ExpectPtzInfobar(control_support),
              prompt_factory()->RequestTypeSeen(
                  permissions::RequestType::kCameraPanTiltZoom));

    // Check the media stream result is expected and the devices returned are
    // expected;
    VerifyResultState(test.ExpectedMediaStreamResult(control_support),
                      test.ExpectMicAllowed() && test.ExpectCamAllowed() &&
                          (!(control_support.pan || control_support.tilt ||
                             control_support.zoom) ||
                           test.ptz != CONTENT_SETTING_BLOCK),
                      test.ExpectMicAllowed() && test.ExpectCamAllowed() &&
                          (!(control_support.pan || control_support.tilt ||
                             control_support.zoom) ||
                           test.ptz != CONTENT_SETTING_BLOCK));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaStreamDevicesControllerPtzTest,
    ::testing::Values(media::VideoCaptureControlSupport({false, false, false}),
                      media::VideoCaptureControlSupport({false, false, true}),
                      media::VideoCaptureControlSupport({true, true, true})));

// Request and allow camera access on WebUI pages without prompting.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       WebUIRequestAndAllowCam) {
  InitWithUrl(GURL(chrome::kChromeUIVersionURL));
  RequestPermissions(GetWebContents(),
                     CreateRequest(std::string(), example_video_id(), false));

  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  VerifyResultState(blink::mojom::MediaStreamRequestResult::OK, false, true);
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       ExtensionRequestMicCam) {
  std::string pdf_extension_page = std::string(extensions::kExtensionScheme) +
                                   "://" + extension_misc::kPdfExtensionId +
                                   "/index.html";
  InitWithUrl(GURL(pdf_extension_page));
  // Test that a prompt is required.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(),
      CreateRequest(example_audio_id(), example_video_id(), false));
  ASSERT_EQ(2, prompt_factory()->TotalRequestCount());
  ASSERT_TRUE(prompt_factory()->RequestTypeSeen(
      permissions::RequestType::kCameraStream));
  ASSERT_TRUE(
      prompt_factory()->RequestTypeSeen(permissions::RequestType::kMicStream));

  VerifyResultState(blink::mojom::MediaStreamRequestResult::OK, true, true);

  // Check that re-requesting allows without prompting.
  prompt_factory()->ResetCounts();
  RequestPermissions(
      GetWebContents(),
      CreateRequest(example_audio_id(), example_video_id(), false));
  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  VerifyResultState(blink::mojom::MediaStreamRequestResult::OK, true, true);
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       PepperRequestInsecure) {
  InitWithUrl(GURL("http://www.example.com"));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  RequestPermissions(
      GetWebContents(),
      CreateRequestWithType(example_audio_id(), example_video_id(), false,
                            blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY));
  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  VerifyResultState(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
                    false, false);
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest, WebContentsDestroyed) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  content::MediaStreamRequest request =
      CreateRequest(example_audio_id(), example_video_id(), false);
  // Simulate a destroyed RenderFrameHost.
  request.render_frame_id = 0;
  request.render_process_id = 0;

  RequestPermissions(nullptr, request);
  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  VerifyResultState(
      blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN, false,
      false);
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       WebContentsDestroyedDuringRequest) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  content::WebContents* prompt_contents = GetWebContents();
  const int prompt_contents_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(prompt_contents);

  // Now request permissions, but before the request is handled, destroy the
  // tab.
  permission_bubble_media_access_handler_->HandleRequest(
      prompt_contents,
      CreateRequest(example_audio_id(), example_video_id(), false),
      base::BindOnce(
          [](const blink::mojom::StreamDevicesSet& stream_devices_set,
             blink::mojom::MediaStreamRequestResult result,
             std::unique_ptr<content::MediaStreamUI> ui) {
            // The permission may be dismissed before we have a chance to delete
            // the request.
            EXPECT_EQ(
                blink::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED,
                result);
          }),
      nullptr);
  // Since the mock prompt factory holds a reference to the
  // PermissionRequestManager for the WebContents and uses that reference in its
  // destructor, it has to be destroyed before the tab.
  prompt_factory_.reset();
  int previous_tab_count = browser()->tab_strip_model()->count();
  browser()->tab_strip_model()->CloseWebContentsAt(
      prompt_contents_index, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(previous_tab_count - 1, browser()->tab_strip_model()->count());
  base::RunLoop().RunUntilIdle();

  VerifyResultState(
      blink::mojom::MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS, false,
      false);
}

// Request and block microphone and camera access with kill switch.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestAndKillSwitchMicCam) {
  std::map<std::string, std::string> params;
  params[permissions::PermissionUtil::GetPermissionString(
      ContentSettingsType::MEDIASTREAM_MIC)] =
      permissions::PermissionContextBase::kPermissionsKillSwitchBlockedValue;
  params[permissions::PermissionUtil::GetPermissionString(
      ContentSettingsType::MEDIASTREAM_CAMERA)] =
      permissions::PermissionContextBase::kPermissionsKillSwitchBlockedValue;
  base::AssociateFieldTrialParams(
      permissions::PermissionContextBase::kPermissionsKillSwitchFieldStudy,
      "TestGroup", params);
  base::FieldTrialList::CreateFieldTrial(
      permissions::PermissionContextBase::kPermissionsKillSwitchFieldStudy,
      "TestGroup");
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_ALLOWED);
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_ALLOWED);
  RequestPermissions(
      GetWebContents(),
      CreateRequest(example_audio_id(), example_video_id(), false));

  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());
  VerifyResultState(blink::mojom::MediaStreamRequestResult::KILL_SWITCH_ON,
                    false, false);
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestCamAndMicBlockedByPermissionsPolicy) {
  InitWithUrl(embedded_test_server()->GetURL("/iframe_blank.html"));

  // Create a cross-origin request by using localhost as the iframe origin.
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  GURL cross_origin_url = embedded_test_server()
                              ->GetURL("/simple.html")
                              .ReplaceComponents(replace_host);
  content::NavigateIframeToURL(GetWebContents(), "test",
                               GURL(cross_origin_url));
  content::RenderFrameHost* child_frame =
      ChildFrameAt(GetWebContents()->GetPrimaryMainFrame(), 0);

  content::MediaStreamRequest request =
      CreateRequest(example_audio_id(), example_video_id(), false);
  // Make the child frame the source of the request.
  request.render_process_id = child_frame->GetProcess()->GetID();
  request.render_frame_id = child_frame->GetRoutingID();
  request.security_origin = child_frame->GetLastCommittedOrigin().GetURL();

  RequestPermissions(GetWebContents(), request);

  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  VerifyResultState(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
                    false, false);
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().empty());
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestCamBlockedByPermissionsPolicy) {
  InitWithUrl(embedded_test_server()->GetURL("/iframe_blank.html"));

  // Create a cross-origin request by using localhost as the iframe origin.
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  GURL cross_origin_url = embedded_test_server()
                              ->GetURL("/simple.html")
                              .ReplaceComponents(replace_host);
  content::NavigateIframeToURL(GetWebContents(), "test",
                               GURL(cross_origin_url));
  content::RenderFrameHost* child_frame =
      ChildFrameAt(GetWebContents()->GetPrimaryMainFrame(), 0);

  content::MediaStreamRequest request =
      CreateRequest(std::string(), example_video_id(), false);
  // Make the child frame the source of the request.
  request.render_process_id = child_frame->GetProcess()->GetID();
  request.render_frame_id = child_frame->GetRoutingID();
  request.security_origin = child_frame->GetLastCommittedOrigin().GetURL();

  RequestPermissions(GetWebContents(), request);

  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  VerifyResultState(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
                    false, false);
  EXPECT_TRUE(GetContentSettings()->GetMicrophoneCameraState().empty());
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       PepperAudioRequestNoCamera) {
  MediaCaptureDevicesDispatcher::GetInstance()->SetTestVideoCaptureDevices({});
  InitWithUrl(GURL(chrome::kChromeUIVersionURL));
  RequestPermissions(
      GetWebContents(),
      CreateRequestWithType(example_audio_id(), std::string(), false,
                            blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY));
  VerifyResultState(blink::mojom::MediaStreamRequestResult::OK, true, false);
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       PepperVideoRequestNoMic) {
  MediaCaptureDevicesDispatcher::GetInstance()->SetTestAudioCaptureDevices({});
  InitWithUrl(GURL(chrome::kChromeUIVersionURL));
  RequestPermissions(
      GetWebContents(),
      CreateRequestWithType(std::string(), example_video_id(), false,
                            blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY));
  VerifyResultState(blink::mojom::MediaStreamRequestResult::OK, false, true);
}
