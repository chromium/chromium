// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/features.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#endif

namespace {

// Note that this is used for both getDisplayMedia as well as for
// getCurrentBrowsingContextMedia.
static const char kMainHtmlPage[] = "/webrtc/webrtc_getdisplaymedia_test.html";

enum class TestedFunction {
  kGetDisplayMedia,
  kGetCurrentBrowsingContextMedia,
};

struct TestConfigForPicker {
  TestedFunction tested_function;
  // |accept_this_tab_capture| is only applicable for
  // getCurrentBrowsingContextMedia API, where setting this bool false implies
  // no tab-capture by canceling the confirmation box.
  bool accept_this_tab_capture;
};

struct TestConfigForFakeUI {
  TestedFunction tested_function;
  const char* display_surface;
};

const char* GetTestedFunctionName(TestedFunction tested_function) {
  switch (tested_function) {
    case TestedFunction::kGetDisplayMedia:
      return "getDisplayMedia";
    case TestedFunction::kGetCurrentBrowsingContextMedia:
      return "getCurrentBrowsingContextMedia";
  }
  CHECK(false) << "Invalid function to test.";
  return "";
}

}  // namespace

// Base class for top level tests for getDisplayMedia() and
// getCurrentBrowsingContextMedia().
class WebRtcScreenCaptureBrowserTest : public WebRtcTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }

  void RunTestedFunction(const TestedFunction tested_function,
                         content::WebContents* tab,
                         const std::string& constraints,
                         bool is_fake_ui,
                         bool accept_this_tab_capture) {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        tab->GetMainFrame(),
        base::StringPrintf("runTestedFunction(\"%s\", %s);",
                           GetTestedFunctionName(tested_function),
                           constraints.c_str()),
        &result));
#if defined(OS_MAC)
    // Starting from macOS 10.15, screen capture requires system permissions
    // that are disabled by default. The permission is reported as granted
    // if the fake UI is used.
    EXPECT_EQ(result, base::mac::IsAtMostOS10_14() || is_fake_ui
                          ? "capture-success"
                          : "capture-failure");
#else
    EXPECT_EQ(result,
              accept_this_tab_capture ? "capture-success" : "capture-failure");
#endif
  }
};

// Top level test for getDisplayMedia() and getCurrentBrowsingContextMedia().
// Pops picker UI and shares by default.
class WebRtcScreenCaptureBrowserTestWithPicker
    : public WebRtcScreenCaptureBrowserTest,
      public testing::WithParamInterface<TestConfigForPicker> {
 public:
  WebRtcScreenCaptureBrowserTestWithPicker() : test_config_(GetParam()) {
    if (test_config_.tested_function ==
        TestedFunction::kGetCurrentBrowsingContextMedia) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kRTCGetCurrentBrowsingContextMedia);
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    switch (test_config_.tested_function) {
      case TestedFunction::kGetDisplayMedia: {
#if BUILDFLAG(IS_CHROMEOS_ASH)
        command_line->AppendSwitchASCII(
            switches::kAutoSelectDesktopCaptureSource, "Display");
#else
        command_line->AppendSwitchASCII(
            switches::kAutoSelectDesktopCaptureSource, "Entire screen");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
        break;
      }
      case TestedFunction::kGetCurrentBrowsingContextMedia: {
        command_line->AppendSwitch(test_config_.accept_this_tab_capture
                                       ? switches::kThisTabCaptureAutoAccept
                                       : switches::kThisTabCaptureAutoReject);
        break;
      }
    }
  }

  const TestConfigForPicker test_config_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(1170479): Real desktop capture is flaky on below platforms.
#if defined(OS_WIN) || defined(OS_MAC)
#define MAYBE_ScreenCaptureVideo DISABLED_ScreenCaptureVideo
#else
#define MAYBE_ScreenCaptureVideo ScreenCaptureVideo
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithPicker,
                       MAYBE_ScreenCaptureVideo) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  std::string constraints("{video:true}");
  RunTestedFunction(test_config_.tested_function, tab, constraints,
                    /*is_fake_ui=*/false, test_config_.accept_this_tab_capture);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithPicker,
                       ScreenCaptureVideoWithDlp) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  std::string constraints("{video:true}");
  RunTestedFunction(test_config_.tested_function, tab, constraints,
                    /*is_fake_ui=*/false, test_config_.accept_this_tab_capture);

  if (!test_config_.accept_this_tab_capture) {
    // This test is not relevant for this parameterized test case because it
    // does not capture the tab/display surface.
    return;
  }

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "waitVideoUnmuted();", &result));
  EXPECT_EQ(result, "unmuted");

  const policy::DlpContentRestrictionSet kScreenShareRestricted(
      policy::DlpContentRestriction::kScreenShare);

  policy::DlpContentManagerTestHelper helper_;
  helper_.ChangeConfidentiality(tab, kScreenShareRestricted);
  content::WaitForLoadStop(tab);

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "waitVideoMuted();", &result));
  EXPECT_EQ(result, "muted");

  const policy::DlpContentRestrictionSet kEmptyRestrictionSet;
  helper_.ChangeConfidentiality(tab, kEmptyRestrictionSet);

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "waitVideoUnmuted();", &result));
  EXPECT_EQ(result, "unmuted");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(1170479): Real desktop capture is flaky on below platforms.
#if defined(OS_WIN) || defined(OS_MAC)
#define MAYBE_ScreenCaptureVideoAndAudio DISABLED_ScreenCaptureVideoAndAudio
// On linux debug bots, it's flaky as well.
#elif ((defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && !defined(NDEBUG))
#define MAYBE_ScreenCaptureVideoAndAudio DISABLED_ScreenCaptureVideoAndAudio
// On linux asan bots, it's flaky as well - msan and other rel bot are fine.
#elif ((defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
       defined(ADDRESS_SANITIZER))
#define MAYBE_ScreenCaptureVideoAndAudio DISABLED_ScreenCaptureVideoAndAudio
#else
#define MAYBE_ScreenCaptureVideoAndAudio ScreenCaptureVideoAndAudio
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithPicker,
                       MAYBE_ScreenCaptureVideoAndAudio) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  std::string constraints("{video:true, audio:true}");
  RunTestedFunction(test_config_.tested_function, tab, constraints,
                    /*is_fake_ui=*/false, test_config_.accept_this_tab_capture);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebRtcScreenCaptureBrowserTestWithPicker,
    testing::Values(
        TestConfigForPicker{TestedFunction::kGetDisplayMedia,
                            /*accept_this_tab_capture=*/true},
        TestConfigForPicker{TestedFunction::kGetCurrentBrowsingContextMedia,
                            /*accept_this_tab_capture=*/true},
        TestConfigForPicker{TestedFunction::kGetCurrentBrowsingContextMedia,
                            /*accept_this_tab_capture=*/false}));

// Top level test for getDisplayMedia() and getCurrentBrowsingContextMedia().
// Skips picker UI and uses fake device with specified type.
class WebRtcScreenCaptureBrowserTestWithFakeUI
    : public WebRtcScreenCaptureBrowserTest,
      public testing::WithParamInterface<TestConfigForFakeUI> {
 public:
  WebRtcScreenCaptureBrowserTestWithFakeUI() : test_config_(GetParam()) {
    if (test_config_.tested_function ==
        TestedFunction::kGetCurrentBrowsingContextMedia) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kRTCGetCurrentBrowsingContextMedia);
    }
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
  const TestConfigForFakeUI test_config_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithFakeUI,
                       ScreenCaptureVideo) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  std::string constraints("{video:true}");
  RunTestedFunction(test_config_.tested_function, tab, constraints,
                    /*is_fake_ui=*/true, /*accept_this_tab_capture=*/true);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getDisplaySurfaceSetting();", &result));
  EXPECT_EQ(result, test_config_.display_surface);

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getLogicalSurfaceSetting();", &result));
  EXPECT_EQ(result, "true");

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getCursorSetting();", &result));
  EXPECT_EQ(result, "never");
}

IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithFakeUI,
                       ScreenCaptureVideoAndAudio) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  std::string constraints("{video:true, audio:true}");
  RunTestedFunction(test_config_.tested_function, tab, constraints,
                    /*is_fake_ui=*/true, /*accept_this_tab_capture=*/true);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "hasAudioTrack();", &result));
  EXPECT_EQ(result, "true");
}

IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithFakeUI,
                       ScreenCaptureWithConstraints) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  const int kMaxWidth = 200;
  const int kMaxFrameRate = 6;
  const std::string& constraints =
      base::StringPrintf("{video: {width: {max: %d}, frameRate: {max: %d}}}",
                         kMaxWidth, kMaxFrameRate);
  RunTestedFunction(test_config_.tested_function, tab, constraints,
                    /*is_fake_ui=*/true, /*accept_this_tab_capture=*/true);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getWidthSetting();", &result));
  EXPECT_EQ(result, base::StringPrintf("%d", kMaxWidth));

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getFrameRateSetting();", &result));
  EXPECT_EQ(result, base::StringPrintf("%d", kMaxFrameRate));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebRtcScreenCaptureBrowserTestWithFakeUI,
    testing::Values(TestConfigForFakeUI{TestedFunction::kGetDisplayMedia,
                                        /*display_surface=*/"monitor"},
                    TestConfigForFakeUI{TestedFunction::kGetDisplayMedia,
                                        /*display_surface=*/"window"},
                    TestConfigForFakeUI{TestedFunction::kGetDisplayMedia,
                                        /*display_surface=*/"browser"},
                    TestConfigForFakeUI{
                        TestedFunction::kGetCurrentBrowsingContextMedia,
                        /*display_surface=*/"browser"}));
