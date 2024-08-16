// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/webrtc/media_stream_devices_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

using testing::_;

namespace policy {

class MediaStreamDevicesControllerBrowserTest
    : public PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  MediaStreamDevicesControllerBrowserTest()
      : request_url_allowed_via_allowlist_(false) {
    policy_value_ = GetParam();
  }
  virtual ~MediaStreamDevicesControllerBrowserTest() {}

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
    request_url_ = embedded_test_server()->GetURL("/simple.html");
    request_pattern_ = request_url_.DeprecatedGetOriginAsURL().spec();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), request_url_));

    // Testing both the new (PermissionManager) and old code-paths is not simple
    // since we are already using WithParamInterface. We only test whichever one
    // is enabled in chrome_features.cc since we won't keep the old path around
    // for long once we flip the flag.
    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);
    prompt_factory_->set_response_type(
        permissions::PermissionRequestManager::ACCEPT_ALL);
  }

  void TearDownOnMainThread() override { prompt_factory_.reset(); }

  content::MediaStreamRequest CreateRequest(
      blink::mojom::MediaStreamType audio_request_type,
      blink::mojom::MediaStreamType video_request_type) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(request_url_,
              web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());
    int render_process_id =
        web_contents->GetPrimaryMainFrame()->GetProcess()->GetID();
    int render_frame_id = web_contents->GetPrimaryMainFrame()->GetRoutingID();
    return content::MediaStreamRequest(
        render_process_id, render_frame_id, 0,
        url::Origin::Create(request_url_), false, blink::MEDIA_GENERATE_STREAM,
        /*requested_audio_device_ids=*/{}, /*requested_video_device_ids=*/{},
        audio_request_type, video_request_type,
        /*disable_local_echo=*/false,
        /*request_pan_tilt_zoom_permission=*/false,
        /*captured_surface_control_active=*/false);
  }

  // Configure a given policy map. The |policy_name| is the name of either the
  // audio or video capture allow policy and must never be nullptr.
  // |allowlist_policy| and |allow_rule| are optional.  If nullptr, no allowlist
  // policy is set.  If non-nullptr, the allowlist policy is set to contain
  // either the |allow_rule| (if non-nullptr) or an "allow all" wildcard.
  void ConfigurePolicyMap(PolicyMap* policies,
                          const char* policy_name,
                          const char* allowlist_policy,
                          const char* allow_rule) {
    policies->Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, base::Value(policy_value_), nullptr);

    if (allowlist_policy) {
      // Add an entry to the allowlist that allows the specified URL regardless
      // of the setting of kAudioCapturedAllowed.
      base::Value::List list;
      if (allow_rule) {
        list.Append(allow_rule);
        request_url_allowed_via_allowlist_ = true;
      } else {
        list.Append(ContentSettingsPattern::Wildcard().ToString());
        // We should ignore all wildcard entries in the allowlist, so even
        // though we've added an entry, it should be ignored and our expectation
        // is that the request has not been allowed via the allowlist.
        request_url_allowed_via_allowlist_ = false;
      }
      policies->Set(allowlist_policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    POLICY_SOURCE_CLOUD, base::Value(std::move(list)), nullptr);
    }
  }

  void Accept(const blink::mojom::StreamDevicesSet& stream_devices_set,
              blink::mojom::MediaStreamRequestResult result,
              bool blocked_by_permissions_policy,
              ContentSetting audio_setting,
              ContentSetting video_setting) {
    if (result == blink::mojom::MediaStreamRequestResult::OK) {
      ASSERT_EQ(stream_devices_set.stream_devices.size(), 1u);
      const blink::mojom::StreamDevices& devices =
          *stream_devices_set.stream_devices[0];
      if (policy_value_ || request_url_allowed_via_allowlist_) {
        ASSERT_NE(devices.audio_device.has_value(),
                  devices.video_device.has_value());
        if (devices.audio_device.has_value()) {
          ASSERT_EQ("fake_dev", devices.audio_device.value().id);
        } else if (devices.video_device.has_value()) {
          ASSERT_EQ("fake_dev", devices.video_device.value().id);
        }
      } else {
        ASSERT_FALSE(devices.audio_device.has_value());
        ASSERT_FALSE(devices.video_device.has_value());
      }
    } else {
      ASSERT_EQ(0u, stream_devices_set.stream_devices.size());
    }
  }

  void FinishAudioTest(std::string requested_device_id) {
    content::MediaStreamRequest request(
        CreateRequest(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                      blink::mojom::MediaStreamType::NO_SERVICE));
    request.requested_audio_device_ids = {requested_device_id};
    // TODO(raymes): Test MEDIA_DEVICE_OPEN (Pepper) which grants both webcam
    // and microphone permissions at the same time.
    webrtc::MediaStreamDevicesController::RequestPermissions(
        request, MediaCaptureDevicesDispatcher::GetInstance(),
        base::BindOnce(&MediaStreamDevicesControllerBrowserTest::Accept,
                       base::Unretained(this)));
    quit_closure_.Run();
  }

  void FinishVideoTest(std::string requested_device_id) {
    content::MediaStreamRequest request(
        CreateRequest(blink::mojom::MediaStreamType::NO_SERVICE,
                      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
    request.requested_video_device_ids = {requested_device_id};
    // TODO(raymes): Test MEDIA_DEVICE_OPEN (Pepper) which grants both webcam
    // and microphone permissions at the same time.
    webrtc::MediaStreamDevicesController::RequestPermissions(
        request, MediaCaptureDevicesDispatcher::GetInstance(),
        base::BindOnce(&MediaStreamDevicesControllerBrowserTest::Accept,
                       base::Unretained(this)));
    quit_closure_.Run();
  }

  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
  bool policy_value_;
  bool request_url_allowed_via_allowlist_;
  GURL request_url_;
  std::string request_pattern_;
  base::RepeatingClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_P(MediaStreamDevicesControllerBrowserTest,
                       AudioCaptureAllowed) {
  blink::MediaStreamDevices audio_devices;
  blink::MediaStreamDevice fake_audio_device(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "fake_dev",
      "Fake Audio Device");
  audio_devices.push_back(fake_audio_device);

  PolicyMap policies;
  ConfigurePolicyMap(&policies, key::kAudioCaptureAllowed, nullptr, nullptr);
  UpdateProviderPolicy(policies);

  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &MediaCaptureDevicesDispatcher::SetTestAudioCaptureDevices,
          base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
          audio_devices),
      base::BindOnce(&MediaStreamDevicesControllerBrowserTest::FinishAudioTest,
                     base::Unretained(this), fake_audio_device.id));

  base::RunLoop loop;
  quit_closure_ = loop.QuitWhenIdleClosure();
  loop.Run();
}

IN_PROC_BROWSER_TEST_P(MediaStreamDevicesControllerBrowserTest,
                       AudioCaptureAllowedUrls) {
  blink::MediaStreamDevices audio_devices;
  blink::MediaStreamDevice fake_audio_device(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "fake_dev",
      "Fake Audio Device");
  audio_devices.push_back(fake_audio_device);

  const char* allow_pattern[] = {
      request_pattern_.c_str(),
      // This will set an allow-all policy allowlist.  Since we do not allow
      // setting an allow-all entry in the allowlist, this entry should be
      // ignored and therefore the request should be denied.
      nullptr,
  };

  for (size_t i = 0; i < std::size(allow_pattern); ++i) {
    PolicyMap policies;
    ConfigurePolicyMap(&policies, key::kAudioCaptureAllowed,
                       key::kAudioCaptureAllowedUrls, allow_pattern[i]);
    UpdateProviderPolicy(policies);

    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &MediaCaptureDevicesDispatcher::SetTestAudioCaptureDevices,
            base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
            audio_devices),
        base::BindOnce(
            &MediaStreamDevicesControllerBrowserTest::FinishAudioTest,
            base::Unretained(this), fake_audio_device.id));

    base::RunLoop loop;
    quit_closure_ = loop.QuitWhenIdleClosure();
    loop.Run();
  }
}

IN_PROC_BROWSER_TEST_P(MediaStreamDevicesControllerBrowserTest,
                       VideoCaptureAllowed) {
  blink::MediaStreamDevices video_devices;
  blink::MediaStreamDevice fake_video_device(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "fake_dev",
      "Fake Video Device");
  video_devices.push_back(fake_video_device);

  PolicyMap policies;
  ConfigurePolicyMap(&policies, key::kVideoCaptureAllowed, nullptr, nullptr);
  UpdateProviderPolicy(policies);

  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &MediaCaptureDevicesDispatcher::SetTestVideoCaptureDevices,
          base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
          video_devices),
      base::BindOnce(&MediaStreamDevicesControllerBrowserTest::FinishVideoTest,
                     base::Unretained(this), std::move(fake_video_device.id)));

  base::RunLoop loop;
  quit_closure_ = loop.QuitWhenIdleClosure();
  loop.Run();
}

IN_PROC_BROWSER_TEST_P(MediaStreamDevicesControllerBrowserTest,
                       VideoCaptureAllowedUrls) {
  blink::MediaStreamDevices video_devices;
  blink::MediaStreamDevice fake_video_device(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "fake_dev",
      "Fake Video Device");
  video_devices.push_back(fake_video_device);

  const char* allow_pattern[] = {
      request_pattern_.c_str(),
      // This will set an allow-all policy allowlist.  Since we do not allow
      // setting an allow-all entry in the allowlist, this entry should be
      // ignored and therefore the request should be denied.
      nullptr,
  };

  for (size_t i = 0; i < std::size(allow_pattern); ++i) {
    PolicyMap policies;
    ConfigurePolicyMap(&policies, key::kVideoCaptureAllowed,
                       key::kVideoCaptureAllowedUrls, allow_pattern[i]);
    UpdateProviderPolicy(policies);

    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &MediaCaptureDevicesDispatcher::SetTestVideoCaptureDevices,
            base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
            video_devices),
        base::BindOnce(
            &MediaStreamDevicesControllerBrowserTest::FinishVideoTest,
            base::Unretained(this), fake_video_device.id));

    base::RunLoop loop;
    quit_closure_ = loop.QuitWhenIdleClosure();
    loop.Run();
  }
}

INSTANTIATE_TEST_SUITE_P(MediaStreamDevicesControllerBrowserTestInstance,
                         MediaStreamDevicesControllerBrowserTest,
                         testing::Bool());

}  // namespace policy
