// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

class MediaStreamDevicesControllerTest : public WebRtcTestBase {
 public:
  MediaStreamDevicesControllerTest()
      : example_audio_id_("fake_audio_dev"),
        example_video_id_("fake_video_dev"),
        media_stream_result_(
            blink::mojom::MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS) {
  }

  // Dummy callback for when we deny the current request directly.
  void OnMediaStreamResponse(const blink::MediaStreamDevices& devices,
                             blink::mojom::MediaStreamRequestResult result,
                             std::unique_ptr<content::MediaStreamUI> ui) {
    media_stream_devices_ = devices;
    media_stream_result_ = result;
    quit_closure_.Run();
    quit_closure_ = base::Closure();
  }

 protected:
  enum DeviceType { DEVICE_TYPE_AUDIO, DEVICE_TYPE_VIDEO };
  enum Access { ACCESS_ALLOWED, ACCESS_DENIED };

  const GURL& example_url() const { return example_url_; }

  TabSpecificContentSettings* GetContentSettings() {
    return TabSpecificContentSettings::FromWebContents(GetWebContents());
  }

  const std::string& example_audio_id() const { return example_audio_id_; }
  const std::string& example_video_id() const { return example_video_id_; }

  blink::mojom::MediaStreamRequestResult media_stream_result() const {
    return media_stream_result_;
  }

  void RequestPermissions(content::WebContents* web_contents,
                          const content::MediaStreamRequest& request,
                          content::MediaResponseCallback callback) {
    base::RunLoop run_loop;
    ASSERT_TRUE(quit_closure_.is_null());
    quit_closure_ = run_loop.QuitClosure();
    MediaStreamDevicesController::RequestPermissions(request,
                                                     std::move(callback));
    run_loop.Run();
  }

  // Sets the device policy-controlled |access| for |example_url_| to be for the
  // selected |device_type|.
  void SetDevicePolicy(DeviceType device_type, Access access) {
    PrefService* prefs = Profile::FromBrowserContext(
        GetWebContents()->GetBrowserContext())->GetPrefs();
    const char* policy_name = NULL;
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

  // Set the content settings for mic/cam.
  void SetContentSettings(ContentSetting mic_setting,
                          ContentSetting cam_setting) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(
            Profile::FromBrowserContext(GetWebContents()->GetBrowserContext()));
    content_settings->SetContentSettingDefaultScope(
        example_url_, GURL(), ContentSettingsType::MEDIASTREAM_MIC,
        std::string(), mic_setting);
    content_settings->SetContentSettingDefaultScope(
        example_url_, GURL(), ContentSettingsType::MEDIASTREAM_CAMERA,
        std::string(), cam_setting);
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
      blink::MediaStreamRequestType request_type) {
    blink::mojom::MediaStreamType audio_type =
        audio_id.empty() ? blink::mojom::MediaStreamType::NO_SERVICE
                         : blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
    blink::mojom::MediaStreamType video_type =
        video_id.empty() ? blink::mojom::MediaStreamType::NO_SERVICE
                         : blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;
    EXPECT_EQ(example_url(),
              GetWebContents()->GetMainFrame()->GetLastCommittedURL());
    int render_process_id =
        GetWebContents()->GetMainFrame()->GetProcess()->GetID();
    int render_frame_id = GetWebContents()->GetMainFrame()->GetRoutingID();
    return content::MediaStreamRequest(
        render_process_id, render_frame_id, 0, example_url(), false,
        request_type, audio_id, video_id, audio_type, video_type, false);
  }

  content::MediaStreamRequest CreateRequest(const std::string& audio_id,
                                            const std::string& video_id) {
    return CreateRequestWithType(audio_id, video_id,
                                 blink::MEDIA_DEVICE_ACCESS);
  }

  void InitWithUrl(const GURL& url) {
    DCHECK(example_url_.is_empty());
    example_url_ = url;
    ui_test_utils::NavigateToURL(browser(), example_url_);
    EXPECT_EQ(TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED,
              GetContentSettings()->GetMicrophoneCameraState());
  }

  MockPermissionPromptFactory* prompt_factory() {
    return prompt_factory_.get();
  }

 private:
  void SetUpOnMainThread() override {
    WebRtcTestBase::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    PermissionRequestManager* manager =
        PermissionRequestManager::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
    prompt_factory_.reset(new MockPermissionPromptFactory(manager));

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
        "Fake Video Device");
    video_devices.push_back(fake_video_device);
    MediaCaptureDevicesDispatcher::GetInstance()->SetTestVideoCaptureDevices(
        video_devices);
  }

  void TearDownOnMainThread() override {
    prompt_factory_.reset();

    WebRtcTestBase::TearDownOnMainThread();
  }

  GURL example_url_;
  const std::string example_audio_id_;
  const std::string example_video_id_;

  blink::MediaStreamDevices media_stream_devices_;
  blink::mojom::MediaStreamRequestResult media_stream_result_;

  base::Closure quit_closure_;

  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
};

// Request and allow microphone access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest, RequestAndAllowMic) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_ALLOWED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(example_audio_id(), std::string()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_EQ(TabSpecificContentSettings::MICROPHONE_ACCESSED,
            GetContentSettings()->GetMicrophoneCameraState());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_requested_audio_device());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_selected_audio_device());
  EXPECT_EQ(std::string(),
            GetContentSettings()->media_stream_requested_video_device());
  EXPECT_EQ(std::string(),
            GetContentSettings()->media_stream_selected_video_device());
}

// Request and allow camera access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest, RequestAndAllowCam) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_ALLOWED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(std::string(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_EQ(TabSpecificContentSettings::CAMERA_ACCESSED,
            GetContentSettings()->GetMicrophoneCameraState());
  EXPECT_EQ(std::string(),
            GetContentSettings()->media_stream_requested_audio_device());
  EXPECT_EQ(std::string(),
            GetContentSettings()->media_stream_selected_audio_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_requested_video_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_selected_video_device());
}

// Request and block microphone access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest, RequestAndBlockMic) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_DENIED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(example_audio_id(), std::string()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_EQ(TabSpecificContentSettings::MICROPHONE_ACCESSED |
                TabSpecificContentSettings::MICROPHONE_BLOCKED,
            GetContentSettings()->GetMicrophoneCameraState());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_requested_audio_device());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_selected_audio_device());
  EXPECT_EQ(std::string(),
            GetContentSettings()->media_stream_requested_video_device());
  EXPECT_EQ(std::string(),
            GetContentSettings()->media_stream_selected_video_device());
}

// Request and block camera access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest, RequestAndBlockCam) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_DENIED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(std::string(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_EQ(TabSpecificContentSettings::CAMERA_ACCESSED |
                TabSpecificContentSettings::CAMERA_BLOCKED,
            GetContentSettings()->GetMicrophoneCameraState());
  EXPECT_EQ(std::string(),
            GetContentSettings()->media_stream_requested_audio_device());
  EXPECT_EQ(std::string(),
            GetContentSettings()->media_stream_selected_audio_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_requested_video_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_selected_video_device());
}

// Request and allow microphone and camera access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestAndAllowMicCam) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_ALLOWED);
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_ALLOWED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(example_audio_id(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_EQ(TabSpecificContentSettings::MICROPHONE_ACCESSED |
                TabSpecificContentSettings::CAMERA_ACCESSED,
            GetContentSettings()->GetMicrophoneCameraState());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_requested_audio_device());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_selected_audio_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_requested_video_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_selected_video_device());
}

// Request and block microphone and camera access.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestAndBlockMicCam) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_DENIED);
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_DENIED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(example_audio_id(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_EQ(TabSpecificContentSettings::MICROPHONE_ACCESSED |
                TabSpecificContentSettings::MICROPHONE_BLOCKED |
                TabSpecificContentSettings::CAMERA_ACCESSED |
                TabSpecificContentSettings::CAMERA_BLOCKED,
            GetContentSettings()->GetMicrophoneCameraState());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_requested_audio_device());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_selected_audio_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_requested_video_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_selected_video_device());
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
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(example_audio_id(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_EQ(TabSpecificContentSettings::MICROPHONE_ACCESSED |
                TabSpecificContentSettings::MICROPHONE_BLOCKED |
                TabSpecificContentSettings::CAMERA_ACCESSED |
                TabSpecificContentSettings::CAMERA_BLOCKED,
            GetContentSettings()->GetMicrophoneCameraState());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_requested_audio_device());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_selected_audio_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_requested_video_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_selected_video_device());
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
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(example_audio_id(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_EQ(TabSpecificContentSettings::MICROPHONE_ACCESSED |
                TabSpecificContentSettings::MICROPHONE_BLOCKED |
                TabSpecificContentSettings::CAMERA_ACCESSED |
                TabSpecificContentSettings::CAMERA_BLOCKED,
            GetContentSettings()->GetMicrophoneCameraState());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_requested_audio_device());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_selected_audio_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_requested_video_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_selected_video_device());
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
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(example_audio_id(), std::string()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_requested_audio_device());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_selected_audio_device());

  // Request cam and allow
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_ALLOWED);
  RequestPermissions(
      GetWebContents(), CreateRequest(std::string(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));
  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_requested_video_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_selected_video_device());

  // Mic state should not have changed.
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_requested_audio_device());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_selected_audio_device());
}

// Denying mic access after camera access should still show the camera as state.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       DenyMicDoesNotChangeCam) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  // Request cam and allow
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_ALLOWED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(std::string(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));
  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_requested_video_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_selected_video_device());
  EXPECT_EQ(TabSpecificContentSettings::CAMERA_ACCESSED,
            GetContentSettings()->GetMicrophoneCameraState());

  // Simulate that an a video stream is now being captured.
  blink::MediaStreamDevices video_devices(1);
  video_devices[0] = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, example_video_id(),
      example_video_id());
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  dispatcher->SetTestVideoCaptureDevices(video_devices);
  std::unique_ptr<content::MediaStreamUI> video_stream_ui =
      dispatcher->GetMediaStreamCaptureIndicator()->RegisterMediaStream(
          GetWebContents(), video_devices);
  video_stream_ui->OnStarted(base::OnceClosure(),
                             content::MediaStreamUI::SourceCallback());

  // Request mic and deny.
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_DENIED);
  // Ensure the prompt is accepted if necessary such that tab specific content
  // settings are updated.
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(example_audio_id(), std::string()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));
  EXPECT_FALSE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_requested_audio_device());
  EXPECT_EQ(example_audio_id(),
            GetContentSettings()->media_stream_selected_audio_device());

  // Cam should still be included in the state.
  EXPECT_TRUE(GetContentSettings()->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(GetContentSettings()->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_requested_video_device());
  EXPECT_EQ(example_video_id(),
            GetContentSettings()->media_stream_selected_video_device());
  EXPECT_EQ(TabSpecificContentSettings::MICROPHONE_ACCESSED |
                TabSpecificContentSettings::MICROPHONE_BLOCKED |
                TabSpecificContentSettings::CAMERA_ACCESSED,
            GetContentSettings()->GetMicrophoneCameraState());

  // After ending the camera capture, the camera permission is no longer
  // relevant, so it should no be included in the mic/cam state.
  video_stream_ui.reset();
  EXPECT_EQ(TabSpecificContentSettings::MICROPHONE_ACCESSED |
                TabSpecificContentSettings::MICROPHONE_BLOCKED,
            GetContentSettings()->GetMicrophoneCameraState());
}

// Stores the ContentSettings inputs for a particular test and has functions
// which return the expected outputs for that test.
struct ContentSettingsTestData {
  // The initial value of the mic/cam content settings.
  ContentSetting mic;
  ContentSetting cam;
  // Whether the infobar should be accepted if it's shown.
  bool accept_infobar;

  // Whether the infobar should be displayed to request mic/cam for the given
  // content settings inputs.
  bool ExpectMicInfobar() const {
    return mic == CONTENT_SETTING_ASK && cam != CONTENT_SETTING_BLOCK;
  }
  bool ExpectCamInfobar() const {
    return cam == CONTENT_SETTING_ASK && mic != CONTENT_SETTING_BLOCK;
  }

  // Whether or not the mic/cam should be allowed after clicking accept/deny for
  // the given inputs.
  bool ExpectMicAllowed() const {
    return mic == CONTENT_SETTING_ALLOW ||
           (mic == CONTENT_SETTING_ASK && accept_infobar);
  }
  bool ExpectCamAllowed() const {
    return cam == CONTENT_SETTING_ALLOW ||
           (cam == CONTENT_SETTING_ASK && accept_infobar);
  }

  // The expected media stream result after clicking accept/deny for the given
  // inputs.
  blink::mojom::MediaStreamRequestResult ExpectedMediaStreamResult() const {
    if (ExpectMicAllowed() && ExpectCamAllowed())
      return blink::mojom::MediaStreamRequestResult::OK;
    return blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
  }
};

// Test all combinations of cam/mic content settings. Then tests the result of
// clicking both accept/deny on the infobar. Both cam/mic are requested.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest, ContentSettings) {
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  static const ContentSettingsTestData tests[] = {
      // Settings that won't result in an infobar.
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW, false},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, false},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW, false},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK, false},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK, false},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK, false},

      // Settings that will result in an infobar. Test both accept and deny.
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, false},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, true},

      {CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, false},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_ASK, true},

      {CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, false},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, true},
  };

  for (auto& test : tests) {
    SetContentSettings(test.mic, test.cam);

    prompt_factory()->ResetCounts();

    // Accept or deny the infobar if it's showing.
    if (test.ExpectMicInfobar() || test.ExpectCamInfobar()) {
      if (test.accept_infobar) {
        prompt_factory()->set_response_type(
            PermissionRequestManager::ACCEPT_ALL);
      } else {
        prompt_factory()->set_response_type(PermissionRequestManager::DENY_ALL);
      }
    } else {
      prompt_factory()->set_response_type(PermissionRequestManager::NONE);
    }
    RequestPermissions(
        GetWebContents(), CreateRequest(example_audio_id(), example_video_id()),
        base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                   base::Unretained(this)));

    ASSERT_LE(prompt_factory()->TotalRequestCount(), 2);
    ASSERT_EQ(test.ExpectMicInfobar(),
              prompt_factory()->RequestTypeSeen(
                  PermissionRequestType::PERMISSION_MEDIASTREAM_MIC));
    ASSERT_EQ(test.ExpectCamInfobar(),
              prompt_factory()->RequestTypeSeen(
                  PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA));

    // Check the media stream result is expected and the devices returned are
    // expected;
    ASSERT_EQ(test.ExpectedMediaStreamResult(), media_stream_result());
    ASSERT_EQ(CheckDevicesListContains(
                  blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE),
              test.ExpectMicAllowed() && test.ExpectCamAllowed());
    ASSERT_EQ(CheckDevicesListContains(
                  blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE),
              test.ExpectMicAllowed() && test.ExpectCamAllowed());
  }
}

// Request and allow camera access on WebUI pages without prompting.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       WebUIRequestAndAllowCam) {
  InitWithUrl(GURL(chrome::kChromeUIVersionURL));
  RequestPermissions(
      GetWebContents(), CreateRequest(std::string(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  ASSERT_EQ(blink::mojom::MediaStreamRequestResult::OK, media_stream_result());
  ASSERT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  ASSERT_TRUE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       ExtensionRequestMicCam) {
  std::string pdf_extension_page = std::string(extensions::kExtensionScheme) +
                                   "://" + extension_misc::kPdfExtensionId +
                                   "/index.html";
  InitWithUrl(GURL(pdf_extension_page));
  // Test that a prompt is required.
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  RequestPermissions(
      GetWebContents(), CreateRequest(example_audio_id(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));
  ASSERT_EQ(2, prompt_factory()->TotalRequestCount());
  ASSERT_TRUE(prompt_factory()->RequestTypeSeen(
      PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA));
  ASSERT_TRUE(prompt_factory()->RequestTypeSeen(
      PermissionRequestType::PERMISSION_MEDIASTREAM_MIC));

  // Accept the prompt.
  ASSERT_EQ(blink::mojom::MediaStreamRequestResult::OK, media_stream_result());
  ASSERT_TRUE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  ASSERT_TRUE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

  // Check that re-requesting allows without prompting.
  prompt_factory()->ResetCounts();
  RequestPermissions(
      GetWebContents(), CreateRequest(example_audio_id(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));
  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  ASSERT_EQ(blink::mojom::MediaStreamRequestResult::OK, media_stream_result());
  ASSERT_TRUE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  ASSERT_TRUE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       PepperRequestInsecure) {
  InitWithUrl(GURL("http://www.example.com"));

  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);

  RequestPermissions(
      GetWebContents(),
      CreateRequestWithType(example_audio_id(), example_video_id(),
                            blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));
  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  ASSERT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            media_stream_result());
  ASSERT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  ASSERT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest, WebContentsDestroyed) {
  InitWithUrl(GURL("http://www.example.com"));

  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);

  content::MediaStreamRequest request =
      CreateRequest(example_audio_id(), example_video_id());
  // Simulate a destroyed RenderFrameHost.
  request.render_frame_id = 0;
  request.render_process_id = 0;

  RequestPermissions(
      nullptr, request,
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));
  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  ASSERT_EQ(blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
            media_stream_result());
  ASSERT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  ASSERT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
}

// Request and block microphone and camera access with kill switch.
IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestAndKillSwitchMicCam) {
  std::map<std::string, std::string> params;
  params[PermissionUtil::GetPermissionString(
      ContentSettingsType::MEDIASTREAM_MIC)] =
      PermissionContextBase::kPermissionsKillSwitchBlockedValue;
  params[PermissionUtil::GetPermissionString(
      ContentSettingsType::MEDIASTREAM_CAMERA)] =
      PermissionContextBase::kPermissionsKillSwitchBlockedValue;
  variations::AssociateVariationParams(
      PermissionContextBase::kPermissionsKillSwitchFieldStudy,
      "TestGroup", params);
  base::FieldTrialList::CreateFieldTrial(
      PermissionContextBase::kPermissionsKillSwitchFieldStudy,
      "TestGroup");
  InitWithUrl(embedded_test_server()->GetURL("/simple.html"));
  SetDevicePolicy(DEVICE_TYPE_AUDIO, ACCESS_ALLOWED);
  SetDevicePolicy(DEVICE_TYPE_VIDEO, ACCESS_ALLOWED);
  RequestPermissions(
      GetWebContents(), CreateRequest(example_audio_id(), example_video_id()),
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  ASSERT_EQ(blink::mojom::MediaStreamRequestResult::KILL_SWITCH_ON,
            media_stream_result());
  ASSERT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  ASSERT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestCamAndMicBlockedByFeaturePolicy) {
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
      ChildFrameAt(GetWebContents()->GetMainFrame(), 0);

  content::MediaStreamRequest request =
      CreateRequest(example_audio_id(), example_video_id());
  // Make the child frame the source of the request.
  request.render_process_id = child_frame->GetProcess()->GetID();
  request.render_frame_id = child_frame->GetRoutingID();
  RequestPermissions(
      GetWebContents(), request,
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  ASSERT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            media_stream_result());
  ASSERT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  ASSERT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
  EXPECT_EQ(TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED,
            GetContentSettings()->GetMicrophoneCameraState());
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       RequestCamBlockedByFeaturePolicy) {
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
      ChildFrameAt(GetWebContents()->GetMainFrame(), 0);

  content::MediaStreamRequest request =
      CreateRequest(std::string(), example_video_id());
  // Make the child frame the source of the request.
  request.render_process_id = child_frame->GetProcess()->GetID();
  request.render_frame_id = child_frame->GetRoutingID();
  RequestPermissions(
      GetWebContents(), request,
      base::Bind(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                 base::Unretained(this)));

  ASSERT_EQ(0, prompt_factory()->TotalRequestCount());

  ASSERT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            media_stream_result());
  ASSERT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  ASSERT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
  EXPECT_EQ(TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED,
            GetContentSettings()->GetMicrophoneCameraState());
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       PepperAudioRequestNoCamera) {
  MediaCaptureDevicesDispatcher::GetInstance()->SetTestVideoCaptureDevices({});
  InitWithUrl(GURL(chrome::kChromeUIVersionURL));
  RequestPermissions(
      GetWebContents(),
      CreateRequestWithType(example_audio_id(), std::string(),
                            blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY),
      base::BindOnce(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                     base::Unretained(this)));

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, media_stream_result());
  EXPECT_TRUE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  EXPECT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
}

IN_PROC_BROWSER_TEST_F(MediaStreamDevicesControllerTest,
                       PepperVideoRequestNoMic) {
  MediaCaptureDevicesDispatcher::GetInstance()->SetTestAudioCaptureDevices({});
  InitWithUrl(GURL(chrome::kChromeUIVersionURL));
  RequestPermissions(
      GetWebContents(),
      CreateRequestWithType(std::string(), example_video_id(),
                            blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY),
      base::BindOnce(&MediaStreamDevicesControllerTest::OnMediaStreamResponse,
                     base::Unretained(this)));

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, media_stream_result());
  EXPECT_FALSE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));
  EXPECT_TRUE(CheckDevicesListContains(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
}
