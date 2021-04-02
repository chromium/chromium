// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

using testing::_;

namespace policy {

class MediaStreamDevicesControllerBrowserTest
    : public PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  MediaStreamDevicesControllerBrowserTest()
      : request_url_allowed_via_whitelist_(false) {
    policy_value_ = GetParam();
  }
  virtual ~MediaStreamDevicesControllerBrowserTest() {}

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
    request_url_ = embedded_test_server()->GetURL("/simple.html");
    request_pattern_ = request_url_.GetOrigin().spec();
    ui_test_utils::NavigateToURL(browser(), request_url_);

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
              web_contents->GetMainFrame()->GetLastCommittedURL());
    int render_process_id = web_contents->GetMainFrame()->GetProcess()->GetID();
    int render_frame_id = web_contents->GetMainFrame()->GetRoutingID();
    return content::MediaStreamRequest(
        render_process_id, render_frame_id, 0, request_url_.GetOrigin(), false,
        blink::MEDIA_DEVICE_ACCESS, std::string(), std::string(),
        audio_request_type, video_request_type, /*disable_local_echo=*/false,
        /*request_pan_tilt_zoom_permission=*/false);
  }

  // Configure a given policy map. The |policy_name| is the name of either the
  // audio or video capture allow policy and must never be nullptr.
  // |whitelist_policy| and |allow_rule| are optional.  If nullptr, no whitelist
  // policy is set.  If non-nullptr, the whitelist policy is set to contain
  // either the |allow_rule| (if non-nullptr) or an "allow all" wildcard.
  void ConfigurePolicyMap(PolicyMap* policies,
                          const char* policy_name,
                          const char* whitelist_policy,
                          const char* allow_rule) {
    policies->Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, base::Value(policy_value_), nullptr);

    if (whitelist_policy) {
      // Add an entry to the whitelist that allows the specified URL regardless
      // of the setting of kAudioCapturedAllowed.
      base::Value list(base::Value::Type::LIST);
      if (allow_rule) {
        list.Append(allow_rule);
        request_url_allowed_via_whitelist_ = true;
      } else {
        list.Append(ContentSettingsPattern::Wildcard().ToString());
        // We should ignore all wildcard entries in the whitelist, so even
        // though we've added an entry, it should be ignored and our expectation
        // is that the request has not been allowed via the whitelist.
        request_url_allowed_via_whitelist_ = false;
      }
      policies->Set(whitelist_policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    POLICY_SOURCE_CLOUD, std::move(list), nullptr);
    }
  }

  void Accept(const blink::MediaStreamDevices& devices,
              blink::mojom::MediaStreamRequestResult result,
              bool blocked_by_permissions_policy,
              ContentSetting audio_setting,
              ContentSetting video_setting) {
    if (policy_value_ || request_url_allowed_via_whitelist_) {
      ASSERT_EQ(1U, devices.size());
      ASSERT_EQ("fake_dev", devices[0].id);
    } else {
      ASSERT_EQ(0U, devices.size());
    }
  }

  void FinishAudioTest() {
    content::MediaStreamRequest request(
        CreateRequest(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                      blink::mojom::MediaStreamType::NO_SERVICE));
    // TODO(raymes): Test MEDIA_DEVICE_OPEN (Pepper) which grants both webcam
    // and microphone permissions at the same time.
    webrtc::MediaStreamDevicesController::RequestPermissions(
        request, MediaCaptureDevicesDispatcher::GetInstance(),
        base::BindOnce(&MediaStreamDevicesControllerBrowserTest::Accept,
                       base::Unretained(this)));
    quit_closure_.Run();
  }

  void FinishVideoTest() {
    content::MediaStreamRequest request(
        CreateRequest(blink::mojom::MediaStreamType::NO_SERVICE,
                      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
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
  bool request_url_allowed_via_whitelist_;
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
                     base::Unretained(this)));

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
      // This will set an allow-all policy whitelist.  Since we do not allow
      // setting an allow-all entry in the whitelist, this entry should be
      // ignored and therefore the request should be denied.
      nullptr,
  };

  for (size_t i = 0; i < base::size(allow_pattern); ++i) {
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
            base::Unretained(this)));

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
                     base::Unretained(this)));

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
      // This will set an allow-all policy whitelist.  Since we do not allow
      // setting an allow-all entry in the whitelist, this entry should be
      // ignored and therefore the request should be denied.
      nullptr,
  };

  for (size_t i = 0; i < base::size(allow_pattern); ++i) {
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
            base::Unretained(this)));

    base::RunLoop loop;
    quit_closure_ = loop.QuitWhenIdleClosure();
    loop.Run();
  }
}

INSTANTIATE_TEST_SUITE_P(MediaStreamDevicesControllerBrowserTestInstance,
                         MediaStreamDevicesControllerBrowserTest,
                         testing::Bool());

}  // namespace policy
