// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

static const char kMainHtmlPage[] = "/webrtc/webrtc_getdisplaymedia_test.html";

struct TestConfig {
  const char* display_surface;
  const char* logical_surface;
  const char* cursor;
};

}  // namespace

// Base class for top level tests for getDisplayMedia().
class WebRtcGetDisplayMediaBrowserTest : public WebRtcTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }

  void RunGetDisplayMedia(content::WebContents* tab,
                          const std::string& constraints,
                          bool is_fake_ui = false) {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        tab->GetMainFrame(),
        base::StringPrintf("runGetDisplayMedia(%s);", constraints.c_str()),
        &result));
#if defined(OS_MAC)
    // Starting from macOS 10.15, screen capture requires system permissions
    // that are disabled by default. The permission is reported as granted
    // if the fake UI is used.
    EXPECT_EQ(result, base::mac::IsAtMostOS10_14() || is_fake_ui
                          ? "getdisplaymedia-success"
                          : "getdisplaymedia-failure");
#else
    EXPECT_EQ(result, "getdisplaymedia-success");
#endif
  }
};

// Top level test for getDisplayMedia(). Pops picker Ui and selects desktop
// capture by default.
class WebRtcGetDisplayMediaBrowserTestWithPicker
    : public WebRtcGetDisplayMediaBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(switches::kAutoSelectDesktopCaptureSource,
                                    "Entire screen");
  }
};

// Real desktop capture is flaky on below platforms.
#if defined(OS_CHROMEOS) || defined(OS_WIN)
#define MAYBE_GetDisplayMediaVideo DISABLED_GetDisplayMediaVideo
#else
#define MAYBE_GetDisplayMediaVideo GetDisplayMediaVideo
#endif  // defined(OS_CHROMEOS) || defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(WebRtcGetDisplayMediaBrowserTestWithPicker,
                       MAYBE_GetDisplayMediaVideo) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  std::string constraints("{video:true}");
  RunGetDisplayMedia(tab, constraints);
}

// Real desktop capture is flaky on below platforms.
#if defined(OS_CHROMEOS) || defined(OS_WIN)
#define MAYBE_GetDisplayMediaVideoAndAudio DISABLED_GetDisplayMediaVideoAndAudio
// On linux debug bots, it's flaky as well.
#elif (defined(OS_LINUX) && !defined(NDEBUG))
#define MAYBE_GetDisplayMediaVideoAndAudio DISABLED_GetDisplayMediaVideoAndAudio
// On linux asan bots, it's flaky as well - msan and other rel bot are fine.
#elif (defined(OS_LINUX) && defined(ADDRESS_SANITIZER))
#define MAYBE_GetDisplayMediaVideoAndAudio DISABLED_GetDisplayMediaVideoAndAudio
#else
#define MAYBE_GetDisplayMediaVideoAndAudio GetDisplayMediaVideoAndAudio
#endif  // defined(OS_CHROMEOS) || defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(WebRtcGetDisplayMediaBrowserTestWithPicker,
                       MAYBE_GetDisplayMediaVideoAndAudio) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  std::string constraints("{video:true, audio:true}");
  RunGetDisplayMedia(tab, constraints);
}

// Top level test for getDisplayMedia(). Skips picker UI and uses fake device
// with specified type.
class WebRtcGetDisplayMediaBrowserTestWithFakeUI
    : public WebRtcGetDisplayMediaBrowserTest,
      public testing::WithParamInterface<TestConfig> {
 public:
  WebRtcGetDisplayMediaBrowserTestWithFakeUI() {
    test_config_ = GetParam();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    command_line->RemoveSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        base::StringPrintf("display-media-type=%s",
                           test_config_.display_surface));
  }

 protected:
  TestConfig test_config_;
};

IN_PROC_BROWSER_TEST_P(WebRtcGetDisplayMediaBrowserTestWithFakeUI,
                       GetDisplayMediaVideo) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  std::string constraints("{video:true}");
  RunGetDisplayMedia(tab, constraints, /*is_fake_ui=*/true);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getDisplaySurfaceSetting();", &result));
  EXPECT_EQ(result, test_config_.display_surface);

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getLogicalSurfaceSetting();", &result));
  EXPECT_EQ(result, test_config_.logical_surface);

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getCursorSetting();", &result));
  EXPECT_EQ(result, test_config_.cursor);
}

IN_PROC_BROWSER_TEST_P(WebRtcGetDisplayMediaBrowserTestWithFakeUI,
                       GetDisplayMediaVideoAndAudio) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  std::string constraints("{video:true, audio:true}");
  RunGetDisplayMedia(tab, constraints, /*is_fake_ui=*/true);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "hasAudioTrack();", &result));
  EXPECT_EQ(result, "true");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetDisplayMediaBrowserTestWithFakeUI,
                       GetDisplayMediaWithConstraints) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  const int kMaxWidth = 200;
  const int kMaxFrameRate = 6;
  const std::string& constraints =
      base::StringPrintf("{video: {width: {max: %d}, frameRate: {max: %d}}}",
                         kMaxWidth, kMaxFrameRate);
  RunGetDisplayMedia(tab, constraints, /*is_fake_ui=*/true);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getWidthSetting();", &result));
  EXPECT_EQ(result, base::StringPrintf("%d", kMaxWidth));

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getFrameRateSetting();", &result));
  EXPECT_EQ(result, base::StringPrintf("%d", kMaxFrameRate));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcGetDisplayMediaBrowserTestWithFakeUI,
                         testing::Values(TestConfig{"monitor", "true", "never"},
                                         TestConfig{"window", "true", "never"},
                                         TestConfig{"browser", "true",
                                                    "never"}));
