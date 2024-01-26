// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"

namespace {

struct PermissionTestConfig {
  const char* constraints;
  const char* expected_microphone;
  const char* expected_camera;
  const char* expected_pan_tilt_zoom;
};

struct TrackTestConfig {
  const char* constraints;
  const double expected_pan;
  const double expected_tilt;
  const double expected_zoom;
  const char* expected_constraints;
};

static const char kMainHtmlPage[] = "/webrtc/webrtc_pan_tilt_zoom_test.html";

}  // namespace

class WebRtcPanTiltZoomPermissionBrowserTest
    : public WebRtcTestBase,
      public testing::WithParamInterface<PermissionTestConfig> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcPanTiltZoomPermissionBrowserTest,
                       TestRequestPanTiltZoomPermission) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            base::StringPrintf("runGetUserMedia(%s);",
                                               GetParam().constraints)),
            "runGetUserMedia-success");

  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "getMicrophonePermission();"),
      GetParam().expected_microphone);

  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "getCameraPermission();"),
      GetParam().expected_camera);

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "getPanTiltZoomPermission();"),
            GetParam().expected_pan_tilt_zoom);
}

INSTANTIATE_TEST_SUITE_P(
    RequestPanTiltZoomPermission,
    WebRtcPanTiltZoomPermissionBrowserTest,
    testing::Values(
        // no pan, tilt, zoom in audio and video constraints
        PermissionTestConfig{"{ video: true }", "prompt", "granted", "prompt"},
        PermissionTestConfig{"{ audio: true }", "granted", "prompt", "prompt"},
        PermissionTestConfig{"{ audio: true, video: true }", "granted",
                             "granted", "prompt"},
        // pan, tilt, zoom in audio constraints
        PermissionTestConfig{"{ audio: { pan : false } }", "granted", "prompt",
                             "prompt"},
        PermissionTestConfig{"{ audio: { tilt : false } }", "granted", "prompt",
                             "prompt"},
        PermissionTestConfig{"{ audio: { zoom : false } }", "granted", "prompt",
                             "prompt"},
        PermissionTestConfig{"{ audio: { pan : {} } }", "granted", "prompt",
                             "prompt"},
        PermissionTestConfig{"{ audio: { tilt : {} } }", "granted", "prompt",
                             "prompt"},
        PermissionTestConfig{"{ audio: { zoom : {} } }", "granted", "prompt",
                             "prompt"},
        PermissionTestConfig{"{ audio: { pan : 1 } }", "granted", "prompt",
                             "prompt"},
        PermissionTestConfig{"{ audio: { tilt : 1 } }", "granted", "prompt",
                             "prompt"},
        PermissionTestConfig{"{ audio: { zoom : 1 } }", "granted", "prompt",
                             "prompt"},
        PermissionTestConfig{"{ audio: { pan : true } }", "granted", "prompt",
                             "prompt"},
        PermissionTestConfig{"{ audio: { tilt : true } }", "granted", "prompt",
                             "prompt"},
        PermissionTestConfig{"{ audio: { zoom : true } }", "granted", "prompt",
                             "prompt"},
        // pan, tilt, zoom in basic video constraints if no audio
        PermissionTestConfig{"{ video: { pan : false } }", "prompt", "granted",
                             "prompt"},
        PermissionTestConfig{"{ video: { tilt : false } }", "prompt", "granted",
                             "prompt"},
        PermissionTestConfig{"{ video: { zoom : false } }", "prompt", "granted",
                             "prompt"},
        PermissionTestConfig{"{ video: { pan : {} } }", "prompt", "granted",
                             "granted"},
        PermissionTestConfig{"{ video: { tilt : {} } }", "prompt", "granted",
                             "granted"},
        PermissionTestConfig{"{ video: { zoom : {} } }", "prompt", "granted",
                             "granted"},
        PermissionTestConfig{"{ video: { pan : 1 } }", "prompt", "granted",
                             "granted"},
        PermissionTestConfig{"{ video: { tilt : 1 } }", "prompt", "granted",
                             "granted"},
        PermissionTestConfig{"{ video: { zoom : 1 } }", "prompt", "granted",
                             "granted"},
        PermissionTestConfig{"{ video: { pan : true } }", "prompt", "granted",
                             "granted"},
        PermissionTestConfig{"{ video: { tilt : true } }", "prompt", "granted",
                             "granted"},
        PermissionTestConfig{"{ video: { zoom : true } }", "prompt", "granted",
                             "granted"},
        // pan, tilt, zoom in advanced video constraints if no audio
        PermissionTestConfig{"{ video: { advanced: [{ pan : false }] } }",
                             "prompt", "granted", "prompt"},
        PermissionTestConfig{"{ video: { advanced: [{ tilt : false }] } }",
                             "prompt", "granted", "prompt"},
        PermissionTestConfig{"{ video: { advanced: [{ zoom : false }] } }",
                             "prompt", "granted", "prompt"},
        PermissionTestConfig{"{ video: { advanced: [{ pan : {} }] } }",
                             "prompt", "granted", "granted"},
        PermissionTestConfig{"{ video: { advanced: [{ tilt : {} }] } }",
                             "prompt", "granted", "granted"},
        PermissionTestConfig{"{ video: { advanced: [{ zoom : {} }] } }",
                             "prompt", "granted", "granted"},
        PermissionTestConfig{"{ video: { advanced: [{ pan : 1 }] } }", "prompt",
                             "granted", "granted"},
        PermissionTestConfig{"{ video: { advanced: [{ tilt : 1 }] } }",
                             "prompt", "granted", "granted"},
        PermissionTestConfig{"{ video: { advanced: [{ zoom : 1 }] } }",
                             "prompt", "granted", "granted"},
        PermissionTestConfig{"{ video: { advanced: [{ pan : true }] } }",
                             "prompt", "granted", "granted"},
        PermissionTestConfig{"{ video: { advanced: [{ tilt : true }] } }",
                             "prompt", "granted", "granted"},
        PermissionTestConfig{"{ video: { advanced: [{ zoom : true }] } }",
                             "prompt", "granted", "granted"},
        // pan, tilt, zoom in basic video constraints if audio
        PermissionTestConfig{"{ audio: true, video: { pan : false } }",
                             "granted", "granted", "prompt"},
        PermissionTestConfig{"{ audio: true, video: { tilt : false } }",
                             "granted", "granted", "prompt"},
        PermissionTestConfig{"{ audio: true, video: { zoom : false } }",
                             "granted", "granted", "prompt"},
        PermissionTestConfig{"{ audio: true, video: { pan : {} } }", "granted",
                             "granted", "granted"},
        PermissionTestConfig{"{ audio: true, video: { tilt : {} } }", "granted",
                             "granted", "granted"},
        PermissionTestConfig{"{ audio: true, video: { zoom : {} } }", "granted",
                             "granted", "granted"},
        PermissionTestConfig{"{ audio: true, video: { pan : 1 } }", "granted",
                             "granted", "granted"},
        PermissionTestConfig{"{ audio: true, video: { tilt : 1 } }", "granted",
                             "granted", "granted"},
        PermissionTestConfig{"{ audio: true, video: { zoom : 1 } }", "granted",
                             "granted", "granted"},
        PermissionTestConfig{"{ audio: true, video: { pan : true } }",
                             "granted", "granted", "granted"},
        PermissionTestConfig{"{ audio: true, video: { tilt : true } }",
                             "granted", "granted", "granted"},
        PermissionTestConfig{"{ audio: true, video: { zoom : true } }",
                             "granted", "granted", "granted"},
        // pan, tilt, zoom in advanced video constraints if audio
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ pan : false }] } }",
            "granted", "granted", "prompt"},
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ tilt : false }] } }",
            "granted", "granted", "prompt"},
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ zoom : false }] } }",
            "granted", "granted", "prompt"},
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ pan : {} }] } }", "granted",
            "granted", "granted"},
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ tilt : {} }] } }", "granted",
            "granted", "granted"},
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ zoom : {} }] } }", "granted",
            "granted", "granted"},
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ pan : 1 }] } }", "granted",
            "granted", "granted"},
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ tilt : 1 }] } }", "granted",
            "granted", "granted"},
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ zoom : 1 }] } }", "granted",
            "granted", "granted"},
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ pan : true }] } }", "granted",
            "granted", "granted"},
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ tilt : true }] } }",
            "granted", "granted", "granted"},
        PermissionTestConfig{
            "{ audio: true, video: { advanced: [{ zoom : true }] } }",
            "granted", "granted", "granted"}));

class WebRtcPanTiltZoomTrackBrowserTest
    : public WebRtcTestBase,
      public testing::WithParamInterface<TrackTestConfig> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcPanTiltZoomTrackBrowserTest,
                       TestTrackFromGetUserMedia) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            base::StringPrintf("runGetUserMedia(%s);",
                                               GetParam().constraints)),
            "runGetUserMedia-success");

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "getPanTiltZoomPermission();"),
            "granted");

  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "getTrackSetting('pan');")
          .ExtractDouble(),
      GetParam().expected_pan);

  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "getTrackSetting('tilt');")
          .ExtractDouble(),
      GetParam().expected_tilt);

  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "getTrackSetting('zoom');")
          .ExtractDouble(),
      GetParam().expected_zoom);

  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(),
                      base::StringPrintf("checkConstraints(%s);",
                                         GetParam().expected_constraints)),
      "checkConstraints-success");
}

// Default PTZ value is 100, min is 100, max is 400 as defined in fake video
// capture config at media/capture/video/fake_video_capture_device.cc and
// media/capture/video/fake_video_capture_device_factory.cc
INSTANTIATE_TEST_SUITE_P(
    TrackFromGetUserMedia,
    WebRtcPanTiltZoomTrackBrowserTest,
    testing::Values(
        // pan, tilt, zoom in basic video constraints with valid values
        TrackTestConfig{"{ video: { pan : 101 } }", 101, 100, 100,
                        "{ pan : 101 }"},
        TrackTestConfig{"{ video: { tilt : 102 } }", 100, 102, 100,
                        "{ tilt : 102 }"},
        TrackTestConfig{"{ video: { zoom : 103 } }", 100, 100, 103,
                        "{ zoom : 103 }"},
        TrackTestConfig{"{ video: { pan: 101, tilt: 102, zoom: 103 } }", 101,
                        102, 103, "{ pan: 101, tilt: 102, zoom: 103 }"},
        // pan, tilt, zoom in advanced video constraints with valid values
        TrackTestConfig{"{ video: { advanced: [{ pan : 101 }] } }", 101, 100,
                        100, "{ advanced: [{ pan : 101 }] }"},
        TrackTestConfig{"{ video: { advanced: [{ tilt : 102 }] } }", 100, 102,
                        100, "{ advanced: [{ tilt : 102 }] }"},
        TrackTestConfig{"{ video: { advanced: [{ zoom : 103 }] } }", 100, 100,
                        103, "{ advanced: [{ zoom : 103 }] }"},
        TrackTestConfig{
            "{ video: { advanced: [{ pan : 101, tilt : 102, zoom : 103 }] } }",
            101, 102, 103,
            "{ advanced: [{ pan: 101, tilt: 102, zoom: 103 }] }"},
        // pan, tilt, zoom in basic video constraints with invalid values
        TrackTestConfig{"{ video: { pan : 99 } }", 100, 100, 100,
                        "{ pan: 99 }"},
        TrackTestConfig{"{ video: { tilt : 99 } }", 100, 100, 100,
                        "{ tilt: 99 }"},
        TrackTestConfig{"{ video: { zoom : 99 } }", 100, 100, 100,
                        "{ zoom: 99 }"},
        TrackTestConfig{"{ video: { pan : 401 } }", 100, 100, 100,
                        "{ pan: 401 }"},
        TrackTestConfig{"{ video: { tilt : 401 } }", 100, 100, 100,
                        "{ tilt: 401 }"},
        TrackTestConfig{"{ video: { zoom : 401 } }", 100, 100, 100,
                        "{ zoom: 401 }"},
        // pan, tilt, zoom in advanced video constraints with invalid values
        TrackTestConfig{"{ video: { advanced: [{ pan : 99 }] } }", 100, 100,
                        100, "{ advanced: [{ pan : 99 }] }"},
        TrackTestConfig{"{ video: { advanced: [{ tilt : 99 }] } }", 100, 100,
                        100, "{ advanced: [{ tilt : 99 }] }"},
        TrackTestConfig{"{ video: { advanced: [{ zoom : 99 }] } }", 100, 100,
                        100, "{ advanced: [{ zoom : 99 }] }"},
        TrackTestConfig{"{ video: { advanced: [{ pan : 401 }] } }", 100, 100,
                        100, "{ advanced: [{ pan : 401 }] }"},
        TrackTestConfig{"{ video: { advanced: [{ tilt : 401 }] } }", 100, 100,
                        100, "{ advanced: [{ tilt : 401 }] }"},
        TrackTestConfig{"{ video: { advanced: [{ zoom : 401 }] } }", 100, 100,
                        100, "{ advanced: [{ zoom : 401 }] }"}));

class WebRtcPanTiltZoomConstraintsBrowserTest
    : public WebRtcTestBase,
      public ::testing::WithParamInterface<std::string> {
 public:
  const char* Constraint() { return GetParam().c_str(); }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcPanTiltZoomConstraintsBrowserTest,
                       TestConstraintsFromGetUserMedia) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  EXPECT_EQ(content::EvalJs(
                tab->GetPrimaryMainFrame(),
                base::StringPrintf(
                    "runGetUserMedia({ video: { width: 640, %s: 101 } });",
                    Constraint())),
            "runGetUserMedia-success");

  EXPECT_EQ(content::EvalJs(
                tab->GetPrimaryMainFrame(),
                base::StringPrintf("checkConstraints({ width: 640, %s: 101 });",
                                   Constraint())),
            "checkConstraints-success");

  EXPECT_EQ(
      content::EvalJs(
          tab->GetPrimaryMainFrame(),
          base::StringPrintf("applyConstraints({ advanced: [{ %s: 102 }] });",
                             Constraint())),
      "applyConstraints-success");

  EXPECT_EQ(
      content::EvalJs(
          tab->GetPrimaryMainFrame(),
          base::StringPrintf("checkConstraints({ advanced: [{ %s: 102 }] });",
                             Constraint())),
      "checkConstraints-success");
}

IN_PROC_BROWSER_TEST_P(WebRtcPanTiltZoomConstraintsBrowserTest,
                       TestUnconstrainedConstraintsFromGetUserMedia) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  EXPECT_EQ(content::EvalJs(
                tab->GetPrimaryMainFrame(),
                base::StringPrintf(
                    "runGetUserMedia({ video: { width: 640, %s: 101 } });",
                    Constraint())),
            "runGetUserMedia-success");

  EXPECT_EQ(content::EvalJs(
                tab->GetPrimaryMainFrame(),
                base::StringPrintf("checkConstraints({ width: 640, %s: 101 });",
                                   Constraint())),
            "checkConstraints-success");

  EXPECT_EQ(content::EvalJs(
                tab->GetPrimaryMainFrame(),
                base::StringPrintf("runGetUserMedia({ video: { %s: true } });",
                                   Constraint())),
            "runGetUserMedia-success");

  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "checkConstraints({});"),
      "checkConstraints-success");
}

INSTANTIATE_TEST_SUITE_P(ConstraintsFromGetUserMedia,
                         WebRtcPanTiltZoomConstraintsBrowserTest,
                         testing::Values("pan", "tilt", "zoom"));

class WebRtcPanTiltZoomPermissionRequestBrowserTest
    : public WebRtcTestBase,
      public ::testing::WithParamInterface<
          bool /* IsPanTiltZoomSupported() */> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "PermissionsRequestRevoke");
  }

  bool IsPanTiltZoomSupported() const { return GetParam(); }

  void SetUpOnMainThread() override {
    WebRtcTestBase::SetUpOnMainThread();

    media::VideoCaptureControlSupport control_support;
    control_support.pan = IsPanTiltZoomSupported();
    control_support.tilt = IsPanTiltZoomSupported();
    control_support.zoom = IsPanTiltZoomSupported();
    blink::MediaStreamDevices video_devices;
    blink::MediaStreamDevice fake_video_device(
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "fake_video_dev",
        "Fake Video Device", control_support, media::MEDIA_VIDEO_FACING_NONE,
        std::nullopt);
    video_devices.push_back(fake_video_device);
    MediaCaptureDevicesDispatcher::GetInstance()->SetTestVideoCaptureDevices(
        video_devices);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcPanTiltZoomPermissionRequestBrowserTest,
                       TestRequestPanTiltZoomPermission) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "runRequestPanTiltZoom();"),
      "runRequestPanTiltZoom-success");

  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "getCameraPermission();"),
      "granted");

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "getPanTiltZoomPermission();"),
            IsPanTiltZoomSupported() ? "granted" : "prompt");
}

INSTANTIATE_TEST_SUITE_P(RequestPanTiltZoomPermission,
                         WebRtcPanTiltZoomPermissionRequestBrowserTest,
                         ::testing::Bool() /* IsPanTiltZoomSupported() */);

class WebRtcPanTiltZoomCameraDevicesBrowserTest : public WebRtcTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "PermissionsRequestRevoke");
  }

  void SetVideoCaptureDevice(bool pan_supported,
                             bool tilt_supported,
                             bool zoom_supported) {
    blink::MediaStreamDevices video_devices;
    blink::MediaStreamDevice fake_video_device(
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "fake_video_dev",
        "Fake Video Device", {pan_supported, tilt_supported, zoom_supported},
        media::MEDIA_VIDEO_FACING_NONE, std::nullopt);
    video_devices.push_back(fake_video_device);
    MediaCaptureDevicesDispatcher::GetInstance()->SetTestVideoCaptureDevices(
        video_devices);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcPanTiltZoomCameraDevicesBrowserTest,
                       TestCameraPanTiltZoomPermissionIsNotGrantedAfterCamera) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  // Simulate camera device with no PTZ support and request PTZ camera
  // permission.
  SetVideoCaptureDevice(/*pan_supported=*/false, /*tilt_supported=*/false,
                        /*zoom_supported=*/false);
  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "runRequestPanTiltZoom();"),
      "runRequestPanTiltZoom-success");

  // Camera permission should be granted.
  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "getCameraPermission();"),
      "granted");

  // Camera PTZ permission should not be granted.
  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "getPanTiltZoomPermission();"),
            "prompt");

  // Simulate camera device with PTZ support.
  SetVideoCaptureDevice(/*pan_supported=*/true, /*tilt_supported=*/true,
                        /*zoom_supported=*/true);

  // Camera PTZ permission should still not be granted.
  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "getPanTiltZoomPermission();"),
            "prompt");
}

IN_PROC_BROWSER_TEST_F(WebRtcPanTiltZoomCameraDevicesBrowserTest,
                       TestCameraPanTiltZoomPermissionPersists) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  // Simulate camera device with PTZ support and request PTZ camera permission.
  SetVideoCaptureDevice(/*pan_supported=*/true, /*tilt_supported=*/true,
                        /*zoom_supported=*/true);
  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "runRequestPanTiltZoom();"),
      "runRequestPanTiltZoom-success");

  // Camera permission should be granted.
  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "getCameraPermission();"),
      "granted");

  // Camera PTZ permission should be granted.
  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "getPanTiltZoomPermission();"),
            "granted");

  // Simulate camera device with no PTZ support.
  SetVideoCaptureDevice(/*pan_supported=*/false, /*tilt_supported=*/false,
                        /*zoom_supported=*/false);

  // Camera PTZ permission should still be granted.
  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "getPanTiltZoomPermission();"),
            "granted");
}

class WebRtcPanTiltZoomFakeCameraDevicesBrowserTest : public WebRtcTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcPanTiltZoomFakeCameraDevicesBrowserTest,
                       TestPageVisible) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  // Access PTZ camera.
  EXPECT_EQ(
      content::EvalJs(
          tab->GetPrimaryMainFrame(),
          "runGetUserMedia({ video: { pan: true, tilt: true, zoom: true } });"),
      "runGetUserMedia-success");

  // Hide page.
  tab->WasHidden();
  std::u16string expected_title = u"hidden";
  EXPECT_EQ(expected_title,
            content::TitleWatcher(tab, expected_title).WaitAndGetTitle());

  // Pan can't be set when page is hidden.
  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "applyConstraints({ advanced: [{ pan: 102 }] });"),
            "applyConstraints-failure-SecurityError");

  // Tilt can't be set when page is hidden.
  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "applyConstraints({ advanced: [{ tilt: 102 }] });"),
            "applyConstraints-failure-SecurityError");

  // Zoom can't be set when page is hidden.
  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "applyConstraints({ advanced: [{ zoom: 102 }] });"),
            "applyConstraints-failure-SecurityError");

  // Show page.
  tab->WasShown();
  expected_title = u"visible";
  EXPECT_EQ(expected_title,
            content::TitleWatcher(tab, expected_title).WaitAndGetTitle());

  // Pan can be set when page is shown again.
  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "applyConstraints({ advanced: [{ pan: 102 }] });"),
            "applyConstraints-success");

  // Tilt can be set when page is shown again.
  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "applyConstraints({ advanced: [{ tilt: 102 }] });"),
            "applyConstraints-success");

  // Zoom can be set when page is shown again.
  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "applyConstraints({ advanced: [{ zoom: 102 }] });"),
            "applyConstraints-success");
}
