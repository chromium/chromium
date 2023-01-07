// Copyright 2014 The Chromium Authors
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
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

enum class TargetVideoCaptureImplementation {
  DEFAULT,
#if BUILDFLAG(IS_WIN)
  WIN_MEDIA_FOUNDATION
#endif
};

const TargetVideoCaptureImplementation kTargetVideoCaptureImplementations[] = {
    TargetVideoCaptureImplementation::DEFAULT,
#if BUILDFLAG(IS_WIN)
    TargetVideoCaptureImplementation::WIN_MEDIA_FOUNDATION
#endif
};

// These tests runs on real webcams and ensure WebRTC can acquire webcams
// correctly. They will do nothing if there are no webcams on the system.
// The webcam on the system must support up to 1080p, or the test will fail.
// This test is excellent for testing the various capture paths of WebRTC
// on all desktop platforms.
class WebRtcWebcamBrowserTest
    : public WebRtcTestBase,
      public testing::WithParamInterface<TargetVideoCaptureImplementation> {
 public:
  WebRtcWebcamBrowserTest() {
#if BUILDFLAG(IS_WIN)
    if (GetParam() == TargetVideoCaptureImplementation::WIN_MEDIA_FOUNDATION) {
      scoped_feature_list_.InitAndEnableFeature(
          media::kMediaFoundationVideoCapture);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          media::kMediaFoundationVideoCapture);
    }
#endif
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->RemoveSwitch(switches::kUseFakeDeviceForMediaStream);
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeUIForMediaStream));
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  std::string GetUserMediaAndGetStreamSize(content::WebContents* tab,
                                           const std::string& constraints) {
    std::string actual_stream_size;
    if (GetUserMediaWithSpecificConstraintsAndAcceptIfPrompted(tab,
                                                               constraints)) {
      StartDetectingVideo(tab, "local-view");
      if (WaitForVideoToPlay(tab))
        actual_stream_size = GetStreamSize(tab, "local-view");
      CloseLastLocalStream(tab);
    }
    return actual_stream_size;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test is manual because the test results can vary heavily depending on
// which webcam or drivers you have on the system.
IN_PROC_BROWSER_TEST_P(WebRtcWebcamBrowserTest,
                       MANUAL_TestAcquiringAndReacquiringWebcam) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  if (!content::IsWebcamAvailableOnSystem(tab)) {
    DVLOG(0) << "No webcam found on bot: skipping...";
    return;
  }

  EXPECT_EQ("320x240",
            GetUserMediaAndGetStreamSize(tab, kVideoCallConstraintsQVGA));
  EXPECT_EQ("640x480",
            GetUserMediaAndGetStreamSize(tab, kVideoCallConstraintsVGA));
  EXPECT_EQ("640x360",
            GetUserMediaAndGetStreamSize(tab, kVideoCallConstraints360p));
  EXPECT_EQ("1280x720",
            GetUserMediaAndGetStreamSize(tab, kVideoCallConstraints720p));
  EXPECT_EQ("1920x1080",
            GetUserMediaAndGetStreamSize(tab, kVideoCallConstraints1080p));
}

INSTANTIATE_TEST_SUITE_P(WebRtcWebcamBrowserTests,
                         WebRtcWebcamBrowserTest,
                         testing::ValuesIn(kTargetVideoCaptureImplementations));
