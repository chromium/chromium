// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {
static const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";
}  // namespace

// Top-level integration test for WebRTC. Uses an actual desktop capture
// extension to capture whole screen.
class WebRtcDesktopCaptureBrowserTest : public WebRtcTestBase {
 public:
  WebRtcDesktopCaptureBrowserTest() : left_tab_(nullptr), right_tab_(nullptr) {}

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // Always use fake devices.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    // Flags use to automatically select the right dekstop source and get
    // around security restrictions.
    command_line->AppendSwitchASCII(switches::kAutoSelectDesktopCaptureSource,
                                    "Entire screen");
    command_line->AppendSwitch(switches::kEnableUserMediaScreenCapturing);
  }

 protected:
  void DetectVideoAndHangUp() {
    StartDetectingVideo(left_tab_, "remote-view");
    StartDetectingVideo(right_tab_, "remote-view");
#if !defined(OS_MACOSX)
    // Video is choppy on Mac OS X. http://crbug.com/443542.
    WaitForVideoToPlay(left_tab_);
    WaitForVideoToPlay(right_tab_);
#endif
    HangUp(left_tab_);
    HangUp(right_tab_);
  }

  content::WebContents* left_tab_;
  content::WebContents* right_tab_;
};

// TODO(crbug.com/796889): Enable on Mac when thread check crash is fixed.
// TODO(sprang): Figure out why test times out on Win 10 and ChromeOS.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_RunsScreenshareFromOneTabToAnother \
  RunsScreenshareFromOneTabToAnother
#else
#define MAYBE_RunsScreenshareFromOneTabToAnother \
  DISABLED_RunsScreenshareFromOneTabToAnother
#endif
IN_PROC_BROWSER_TEST_F(WebRtcDesktopCaptureBrowserTest,
                       MAYBE_RunsScreenshareFromOneTabToAnother) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadDesktopCaptureExtension();
  left_tab_ = OpenTestPageInNewTab(kMainWebrtcTestHtmlPage);
  std::string stream_id = GetDesktopMediaStream(left_tab_);
  EXPECT_NE(stream_id, "");

  LOG(INFO) << "Opened desktop media stream, got id " << stream_id;

  const std::string constraints =
      "{audio: false, video: {mandatory: {chromeMediaSource: 'desktop',"
      "chromeMediaSourceId: '" +
      stream_id + "'}}}";
  EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAcceptIfPrompted(
      left_tab_, constraints));
  right_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  SetupPeerconnectionWithLocalStream(left_tab_);
  SetupPeerconnectionWithLocalStream(right_tab_);
  NegotiateCall(left_tab_, right_tab_);
  VerifyStatsGeneratedCallback(right_tab_);
  DetectVideoAndHangUp();
}
