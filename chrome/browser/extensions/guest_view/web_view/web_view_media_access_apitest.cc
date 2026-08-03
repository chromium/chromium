// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/guest_view/web_view/web_view_apitest.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"

namespace extensions {
namespace {

// This class intercepts media access request from the embedder. The request
// should be triggered only if the embedder API (from tests) allows the request
// in Javascript.
// We do not issue the actual media request; the fact that the request reached
// embedder's WebContents is good enough for our tests. This is also to make
// the test run successfully on trybots.
class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;

  MockWebContentsDelegate(const MockWebContentsDelegate&) = delete;
  MockWebContentsDelegate& operator=(const MockWebContentsDelegate&) = delete;

  ~MockWebContentsDelegate() override = default;

  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override {
    requested_ = true;
    if (request_message_loop_runner_.get()) {
      request_message_loop_runner_->Quit();
    }
  }

  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override {
    checked_ = true;
    if (check_message_loop_runner_.get()) {
      check_message_loop_runner_->Quit();
    }
    return true;
  }

  void WaitForRequestMediaPermission() {
    if (requested_) {
      return;
    }
    request_message_loop_runner_ =
        base::MakeRefCounted<content::MessageLoopRunner>();
    request_message_loop_runner_->Run();
  }

  void WaitForCheckMediaPermission() {
    if (checked_) {
      return;
    }
    check_message_loop_runner_ =
        base::MakeRefCounted<content::MessageLoopRunner>();
    check_message_loop_runner_->Run();
  }

 private:
  bool requested_ = false;
  bool checked_ = false;
  scoped_refptr<content::MessageLoopRunner> request_message_loop_runner_;
  scoped_refptr<content::MessageLoopRunner> check_message_loop_runner_;
};

}  // namespace

class WebViewMediaAccessAPITest : public WebViewAPITest {
 public:
  WebViewMediaAccessAPITest() = default;
  WebViewMediaAccessAPITest(const WebViewMediaAccessAPITest&) = delete;
  WebViewMediaAccessAPITest& operator=(const WebViewMediaAccessAPITest&) =
      delete;
  ~WebViewMediaAccessAPITest() override = default;

  // Runs media_access tests.
  void RunTest(const std::string& test_name) {
    ExtensionTestMessageListener test_run_listener("TEST_PASSED");
    test_run_listener.set_failure_message("TEST_FAILED");
    EXPECT_TRUE(content::ExecJs(
        embedder_web_contents_.get(),
        base::StringPrintf("runTest('%s');", test_name.c_str())));
    ASSERT_TRUE(test_run_listener.WaitUntilSatisfied());
  }

  void SetUp() override {
    WebViewAPITest::SetUp();
    // Verify fake devices are enabled. This is necessary to make sure there is
    // at least one device in the system. Otherwise, this test would fail on
    // machines without physical media devices since getUserMedia fails early in
    // those cases.
    EXPECT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
  }
};

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllow) {
  std::string app_location = "web_view/media_access/allow";
  StartTestServer(app_location);
  LaunchApp(app_location);

  auto mock = std::make_unique<MockWebContentsDelegate>();
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllow");

  mock->WaitForRequestMediaPermission();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllowAndThenDeny) {
  std::string app_location = "web_view/media_access/allow";
  StartTestServer(app_location);
  LaunchApp(app_location);

  auto mock = std::make_unique<MockWebContentsDelegate>();
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllowAndThenDeny");

  mock->WaitForRequestMediaPermission();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllowAsync) {
  std::string app_location = "web_view/media_access/allow";
  StartTestServer(app_location);
  LaunchApp(app_location);

  auto mock = std::make_unique<MockWebContentsDelegate>();
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllowAsync");

  mock->WaitForRequestMediaPermission();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllowTwice) {
  std::string app_location = "web_view/media_access/allow";
  StartTestServer(app_location);
  LaunchApp(app_location);

  auto mock = std::make_unique<MockWebContentsDelegate>();
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllowTwice");

  mock->WaitForRequestMediaPermission();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestCheck) {
  std::string app_location = "web_view/media_access/check";
  StartTestServer(app_location);
  LaunchApp(app_location);

  auto mock = std::make_unique<MockWebContentsDelegate>();
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testCheck");

  mock->WaitForCheckMediaPermission();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestDeny) {
  std::string app_location = "web_view/media_access/deny";
  StartTestServer(app_location);
  LaunchApp(app_location);

  RunTest("testDeny");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestDenyThenAllowThrows) {
  std::string app_location = "web_view/media_access/deny";
  StartTestServer(app_location);
  LaunchApp(app_location);

  RunTest("testDenyThenAllowThrows");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestDenyWithPreventDefault) {
  std::string app_location = "web_view/media_access/deny";
  StartTestServer(app_location);
  LaunchApp(app_location);

  RunTest("testDenyWithPreventDefault");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestNoListenersImplyDeny) {
  std::string app_location = "web_view/media_access/deny";
  StartTestServer(app_location);
  LaunchApp(app_location);

  RunTest("testNoListenersImplyDeny");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest,
                       TestNoPreventDefaultImpliesDeny) {
  std::string app_location = "web_view/media_access/deny";
  StartTestServer(app_location);
  LaunchApp(app_location);

  RunTest("testNoPreventDefaultImpliesDeny");
  StopTestServer();
}

class WebViewMediaAccessPEPCAPITest : public WebViewMediaAccessAPITest {
 public:
  WebViewMediaAccessPEPCAPITest() {
    feature_list_.InitWithFeatures(
        {blink::features::kUserMediaElement,
         blink::features::kUserMediaElementLegacy,
         blink::features::kBypassPepcSecurityForTesting},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessPEPCAPITest, TestAllowPEPC) {
  std::string app_location = "web_view/media_access/allow_pepc";
  StartTestServer(app_location);
  LaunchApp(app_location);

  RunTest("testAllowPEPC");

  StopTestServer();
}

struct PEPCParam {
  bool legacy_enabled;
  bool app_has_video_capture;

  friend std::ostream& operator<<(std::ostream& os, const PEPCParam& param) {
    return os << "Legacy_" << (param.legacy_enabled ? "On" : "Off")
              << "_AppHasVideo_"
              << (param.app_has_video_capture ? "Yes" : "No");
  }
};

class WebViewMediaAccessPEPCParameterizedTest
    : public WebViewMediaAccessAPITest,
      public testing::WithParamInterface<PEPCParam> {
 public:
  WebViewMediaAccessPEPCParameterizedTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        blink::features::kUserMediaElement,
        blink::features::kBypassPepcSecurityForTesting};
    std::vector<base::test::FeatureRef> disabled_features;

    if (GetParam().legacy_enabled) {
      enabled_features.push_back(blink::features::kUserMediaElementLegacy);
    } else {
      disabled_features.push_back(blink::features::kUserMediaElementLegacy);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebViewMediaAccessPEPCParameterizedTest, TestPEPC) {
  PEPCParam param = GetParam();
  std::string app_location = param.app_has_video_capture
                                 ? "web_view/media_access/allow_pepc_both"
                                 : "web_view/media_access/allow_pepc";
  StartTestServer(app_location);
  LaunchApp(app_location);

  auto mock = std::make_unique<MockWebContentsDelegate>();
  embedder_web_contents_->SetDelegate(mock.get());

  ExtensionTestMessageListener test_run_listener("TEST_PASSED");

  EXPECT_TRUE(content::ExecJs(embedder_web_contents_.get(),
                              "runTest('testAllowPEPC');"));

  ASSERT_TRUE(test_run_listener.WaitUntilSatisfied());

  // Verify backend permission statuses are ASK (not persisted).
  content::RenderFrameHost* guest_rfh =
      GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated();
  ASSERT_TRUE(guest_rfh);

  auto* permission_controller =
      embedder_web_contents_->GetBrowserContext()->GetPermissionController();

  auto mic_descriptor = blink::mojom::PermissionDescriptor::New();
  mic_descriptor->name = blink::mojom::PermissionName::AUDIO_CAPTURE;
  EXPECT_EQ(permission_controller->GetPermissionStatusForCurrentDocument(
                mic_descriptor, guest_rfh),
            blink::mojom::PermissionStatus::ASK);

  auto camera_descriptor = blink::mojom::PermissionDescriptor::New();
  camera_descriptor->name = blink::mojom::PermissionName::VIDEO_CAPTURE;
  EXPECT_EQ(permission_controller->GetPermissionStatusForCurrentDocument(
                camera_descriptor, guest_rfh),
            blink::mojom::PermissionStatus::ASK);

  StopTestServer();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebViewMediaAccessPEPCParameterizedTest,
    testing::Values(
        PEPCParam{/*legacy_enabled=*/true, /*app_has_video_capture=*/false},
        PEPCParam{/*legacy_enabled=*/true, /*app_has_video_capture=*/true},
        PEPCParam{/*legacy_enabled=*/false, /*app_has_video_capture=*/true}),
    testing::PrintToStringParamName());

class WebViewMediaAccessPEPCNoLegacyAPITest : public WebViewMediaAccessAPITest {
 public:
  WebViewMediaAccessPEPCNoLegacyAPITest() {
    feature_list_.InitWithFeatures(
        {blink::features::kUserMediaElement,
         blink::features::kBypassPepcSecurityForTesting},
        {blink::features::kUserMediaElementLegacy});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies what happens when one permission is allowed by the app manifest
// (Mic) and the other is denied (Camera). The combined PEPC request should
// fail, and the backend status for both should remain ASK (not persisted).
IN_PROC_BROWSER_TEST_F(WebViewMediaAccessPEPCNoLegacyAPITest,
                       TestPEPC_PartialGrant_MicAllowedCameraDenied) {
  // App only has mic permission.
  std::string app_location = "web_view/media_access/allow_pepc";
  StartTestServer(app_location);
  LaunchApp(app_location);

  auto mock = std::make_unique<MockWebContentsDelegate>();
  embedder_web_contents_->SetDelegate(mock.get());

  // Listen for any message to verify the specific failure reason.
  ExtensionTestMessageListener test_run_listener;

  EXPECT_TRUE(content::ExecJs(embedder_web_contents_.get(),
                              "runTest('testAllowPEPC');"));

  ASSERT_TRUE(test_run_listener.WaitUntilSatisfied());

  // We expect it to fail because Camera is not in manifest.
  EXPECT_TRUE(base::StartsWith(test_run_listener.message(),
                               "TEST_FAILED: Guest denied access",
                               base::CompareCase::SENSITIVE))
      << "Actual message: " << test_run_listener.message();

  // Verify backend permission statuses are ASK (not persisted).
  content::RenderFrameHost* guest_rfh =
      GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated();
  ASSERT_TRUE(guest_rfh);

  auto* permission_controller =
      embedder_web_contents_->GetBrowserContext()->GetPermissionController();

  auto mic_descriptor = blink::mojom::PermissionDescriptor::New();
  mic_descriptor->name = blink::mojom::PermissionName::AUDIO_CAPTURE;
  EXPECT_EQ(permission_controller->GetPermissionStatusForCurrentDocument(
                mic_descriptor, guest_rfh),
            blink::mojom::PermissionStatus::ASK);

  auto camera_descriptor = blink::mojom::PermissionDescriptor::New();
  camera_descriptor->name = blink::mojom::PermissionName::VIDEO_CAPTURE;
  EXPECT_EQ(permission_controller->GetPermissionStatusForCurrentDocument(
                camera_descriptor, guest_rfh),
            blink::mojom::PermissionStatus::ASK);

  StopTestServer();
}

}  // namespace extensions
