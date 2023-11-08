// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"  // PLATFORM_CFM
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/base/filename_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_content_manager_test_helper.h"
#endif

namespace {

using base::test::FeatureRef;

static const char kMainHtmlPage[] = "/webrtc/webrtc_getdisplaymedia_test.html";
static const char kMainHtmlFileName[] = "webrtc_getdisplaymedia_test.html";
static const char kSameOriginRenamedTitle[] = "Renamed Same Origin Tab";
static const char kMainHtmlTitle[] = "WebRTC Automated Test";
// The captured tab is identified by its title.
static const char kCapturedTabTitle[] = "totally-unique-captured-page-title";
static const char kCapturedPageMain[] = "/webrtc/captured_page_main.html";
static const std::u16string kShareThisTabInsteadMessage =
    u"Share this tab instead";

enum class DisplaySurfaceType { kTab, kWindow, kScreen };

enum class GetDisplayMediaVariant : int {
  kStandard = 0,
  kPreferCurrentTab = 1
};

struct TestConfigForPicker {
  TestConfigForPicker(bool should_prefer_current_tab,
                      bool accept_this_tab_capture)
      : should_prefer_current_tab(should_prefer_current_tab),
        accept_this_tab_capture(accept_this_tab_capture) {}

  explicit TestConfigForPicker(std::tuple<bool, bool> input_tuple)
      : TestConfigForPicker(std::get<0>(input_tuple),
                            std::get<1>(input_tuple)) {}

  // If true, specify {preferCurrentTab: true}.
  // Otherwise, either don't specify it, or set it to false.
  bool should_prefer_current_tab;

  // |accept_this_tab_capture| is only applicable if
  // |should_prefer_current_tab| is set to true. Then, setting
  // |accept_this_tab_capture| to true accepts the current tab, and
  // |accept_this_tab_capture| set to false implies dismissing the media picker.
  bool accept_this_tab_capture;
};

struct TestConfigForFakeUI {
  bool should_prefer_current_tab;
  const char* display_surface;
};

struct TestConfigForSelectAllScreens {
  const char* display_surface;
  bool enable_select_all_screens;
};

struct TestConfigForHiDpi {
  bool enable_hidpi;
  int constraint_width;
  int constraint_height;
};

constexpr char kAppWindowTitle[] = "AppWindow Display Capture Test";

constexpr char kEmbeddedTestServerOrigin[] = "http://127.0.0.1";
constexpr char kOtherOrigin[] = "https://other-origin.com";

std::string DisplaySurfaceTypeAsString(
    DisplaySurfaceType display_surface_type) {
  switch (display_surface_type) {
    case DisplaySurfaceType::kTab:
      return "browser";
    case DisplaySurfaceType::kWindow:
      return "window";
    case DisplaySurfaceType::kScreen:
      return "screen";
  }
  NOTREACHED_NORETURN();
}

void RunGetDisplayMedia(content::WebContents* tab,
                        const std::string& constraints,
                        bool is_fake_ui,
                        bool expect_success,
                        bool is_tab_capture,
                        const std::string& expected_error = "",
                        bool with_user_gesture = true) {
  DCHECK(!expect_success || expected_error.empty());

  const content::ToRenderFrameHost& adapter = tab->GetPrimaryMainFrame();
  const std::string script = base::StringPrintf(
      "runGetDisplayMedia(%s, \"top-level-document\", \"%s\");",
      constraints.c_str(), expected_error.c_str());
  std::string result =
      content::EvalJs(adapter, script,
                      with_user_gesture
                          ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                          : content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractString();

#if BUILDFLAG(IS_MAC)
  if (!is_fake_ui && !is_tab_capture &&
      system_media_permissions::CheckSystemScreenCapturePermission() !=
          system_media_permissions::SystemPermission::kAllowed) {
    expect_success = false;
  }
#endif

  EXPECT_EQ(result, expect_success           ? "capture-success"
                    : expected_error.empty() ? "capture-failure"
                                             : "expected-error");
}

void StopAllTracks(content::WebContents* tab) {
  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(), "stopAllTracks();"),
            "stopped");
}

void UpdateWebContentsTitle(content::WebContents* contents,
                            const std::u16string& title) {
  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  contents->UpdateTitleForEntry(entry, title);
}

GURL GetFileURL(const char* filename) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("webrtc").AppendASCII(filename);
  CHECK(base::PathExists(path));
  return net::FilePathToFileURL(path);
}

infobars::ContentInfoBarManager* GetInfoBarManager(
    content::WebContents* web_contents) {
  return infobars::ContentInfoBarManager::FromWebContents(web_contents);
}

ConfirmInfoBarDelegate* GetDelegate(content::WebContents* web_contents) {
  return static_cast<ConfirmInfoBarDelegate*>(
      GetInfoBarManager(web_contents)->infobars()[0]->delegate());
}

bool HasSecondaryButton(content::WebContents* web_contents) {
  return GetDelegate(web_contents)->GetButtons() &
         ConfirmInfoBarDelegate::InfoBarButton::BUTTON_CANCEL;
}

std::u16string GetSecondaryButtonLabel(content::WebContents* web_contents) {
  DCHECK(HasSecondaryButton(web_contents));  // Test error otherwise.
  return GetDelegate(web_contents)
      ->GetButtonLabel(ConfirmInfoBarDelegate::InfoBarButton::BUTTON_CANCEL);
}

void AdjustCommandLineForZeroCopyCapture(base::CommandLine* command_line) {
  CHECK(command_line);

  // TODO(https://crbug.com/1424557): Remove this after fixing feature
  // detection in 0c tab capture path as it'll no longer be needed.
  if constexpr (!BUILDFLAG(IS_CHROMEOS)) {
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }
}

}  // namespace

// Base class for top level tests for getDisplayMedia().
class WebRtcScreenCaptureBrowserTest : public WebRtcTestBase {
 public:
  ~WebRtcScreenCaptureBrowserTest() override = default;

  enum class SelectAllScreens { kUndefined = 0, kTrue = 1, kFalse = 2 };

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }

  virtual bool PreferCurrentTab() const = 0;

  std::string GetConstraints(bool video,
                             bool audio,
                             SelectAllScreens select_all_screens) const {
    std::string select_all_screens_property =
        (select_all_screens == SelectAllScreens::kUndefined)
            ? ""
            : base::StringPrintf(
                  "autoSelectAllScreens: %s",
                  (select_all_screens == SelectAllScreens::kFalse) ? "false"
                                                                   : "true");
    return base::StringPrintf(
        "{video: %s, audio: %s, preferCurrentTab: %s, %s}",
        video ? "true" : "false", audio ? "true" : "false",
        PreferCurrentTab() ? "true" : "false",
        select_all_screens_property.c_str());
  }
};

// Top level test for getDisplayMedia().
// Pops picker UI and shares by default.
class WebRtcScreenCaptureBrowserTestWithPicker
    : public WebRtcScreenCaptureBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  WebRtcScreenCaptureBrowserTestWithPicker() : test_config_(GetParam()) {}

  void SetUpOnMainThread() override {
    WebRtcScreenCaptureBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(PLATFORM_CFM)
    if (test_config_.should_prefer_current_tab &&
        !test_config_.accept_this_tab_capture) {
      GTEST_SKIP();  // CFMs always automatically accept current-tab captures.
    }
#endif
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    if (test_config_.should_prefer_current_tab) {
      command_line->AppendSwitch(test_config_.accept_this_tab_capture
                                     ? switches::kThisTabCaptureAutoAccept
                                     : switches::kThisTabCaptureAutoReject);
    } else {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      command_line->AppendSwitchASCII(switches::kAutoSelectDesktopCaptureSource,
                                      "Display");
#else
      command_line->AppendSwitchASCII(switches::kAutoSelectDesktopCaptureSource,
                                      "Entire screen");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    }
  }

  bool PreferCurrentTab() const override {
    return test_config_.should_prefer_current_tab;
  }

  const TestConfigForPicker test_config_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcScreenCaptureBrowserTestWithPicker,
                         testing::Combine(
                             /*should_prefer_current_tab=*/testing::Bool(),
                             /*accept_this_tab_capture=*/testing::Bool()));

// TODO(1170479): Real desktop capture is flaky on below platforms.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ScreenCaptureVideo DISABLED_ScreenCaptureVideo
#else
#define MAYBE_ScreenCaptureVideo ScreenCaptureVideo
#endif  // BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithPicker,
                       MAYBE_ScreenCaptureVideo) {
  if (!test_config_.should_prefer_current_tab &&
      !test_config_.accept_this_tab_capture) {
    GTEST_SKIP();
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  RunGetDisplayMedia(tab,
                     GetConstraints(
                         /*video=*/true, /*audio=*/false,
                         /*select_all_screens=*/
                         SelectAllScreens::kUndefined),
                     /*is_fake_ui=*/false,
                     /*expect_success=*/test_config_.accept_this_tab_capture,
                     /*is_tab_capture=*/PreferCurrentTab());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithPicker,
                       ScreenCaptureVideoWithDlp) {
  if (!test_config_.should_prefer_current_tab &&
      !test_config_.accept_this_tab_capture) {
    GTEST_SKIP();
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  policy::DlpContentManagerTestHelper helper;
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  RunGetDisplayMedia(tab,
                     GetConstraints(
                         /*video=*/true, /*audio=*/false,
                         /*select_all_screens=*/
                         SelectAllScreens::kUndefined),
                     /*is_fake_ui=*/false,
                     /*expect_success=*/test_config_.accept_this_tab_capture,
                     /*is_tab_capture=*/PreferCurrentTab());

  if (!test_config_.accept_this_tab_capture) {
    // This test is not relevant for this parameterized test case because it
    // does not capture the tab/display surface.
    return;
  }

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(), "waitVideoUnmuted();"),
            "unmuted");

  const policy::DlpContentRestrictionSet kScreenShareRestricted(
      policy::DlpContentRestriction::kScreenShare,
      policy::DlpRulesManager::Level::kBlock);

  helper.ChangeConfidentiality(tab, kScreenShareRestricted);
  content::WaitForLoadStop(tab);

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(), "waitVideoMuted();"),
            "muted");

  const policy::DlpContentRestrictionSet kEmptyRestrictionSet;
  helper.ChangeConfidentiality(tab, kEmptyRestrictionSet);

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(), "waitVideoUnmuted();"),
            "unmuted");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(1170479): Real desktop capture is flaky on below platforms.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ScreenCaptureVideoAndAudio DISABLED_ScreenCaptureVideoAndAudio
// On linux debug bots, it's flaky as well.
#elif ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
       !defined(NDEBUG))
#define MAYBE_ScreenCaptureVideoAndAudio DISABLED_ScreenCaptureVideoAndAudio
// On linux asan bots, it's flaky as well - msan and other rel bot are fine.
#elif ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
       defined(ADDRESS_SANITIZER))
#define MAYBE_ScreenCaptureVideoAndAudio DISABLED_ScreenCaptureVideoAndAudio
#else
#define MAYBE_ScreenCaptureVideoAndAudio ScreenCaptureVideoAndAudio
#endif  // BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithPicker,
                       MAYBE_ScreenCaptureVideoAndAudio) {
  if (!test_config_.should_prefer_current_tab &&
      !test_config_.accept_this_tab_capture) {
    GTEST_SKIP();
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  RunGetDisplayMedia(tab,
                     GetConstraints(
                         /*video=*/true, /*audio=*/true,
                         /*select_all_screens=*/
                         SelectAllScreens::kUndefined),
                     /*is_fake_ui=*/false,
                     /*expect_success=*/test_config_.accept_this_tab_capture,
                     /*is_tab_capture=*/PreferCurrentTab());
}

// Top level test for getDisplayMedia().
// Skips picker UI and uses fake device with specified type.
class WebRtcScreenCaptureBrowserTestWithFakeUI
    : public WebRtcScreenCaptureBrowserTest,
      public testing::WithParamInterface<TestConfigForFakeUI> {
 public:
  WebRtcScreenCaptureBrowserTestWithFakeUI() : test_config_(GetParam()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    command_line->RemoveSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        base::StringPrintf("display-media-type=%s",
                           test_config_.display_surface));

    AdjustCommandLineForZeroCopyCapture(command_line);
  }

  bool PreferCurrentTab() const override {
    return test_config_.should_prefer_current_tab;
  }

 protected:
  const TestConfigForFakeUI test_config_;
};

IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithFakeUI,
                       ScreenCaptureVideo) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  RunGetDisplayMedia(tab,
                     GetConstraints(
                         /*video=*/true, /*audio=*/false,
                         /*select_all_screens=*/
                         SelectAllScreens::kUndefined),
                     /*is_fake_ui=*/true, /*expect_success=*/true,
                     /*is_tab_capture=*/PreferCurrentTab());

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "getDisplaySurfaceSetting();"),
            test_config_.display_surface);

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(),
                            "getLogicalSurfaceSetting();"),
            "true");

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(), "getCursorSetting();"),
            "never");
}

IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithFakeUI,
                       ScreenCaptureVideoAndAudio) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  RunGetDisplayMedia(tab,
                     GetConstraints(
                         /*video=*/true, /*audio=*/true,
                         /*select_all_screens=*/
                         SelectAllScreens::kUndefined),
                     /*is_fake_ui=*/true, /*expect_success=*/true,
                     /*is_tab_capture=*/PreferCurrentTab());

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(), "hasAudioTrack();"),
            "true");
}

IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureBrowserTestWithFakeUI,
                       ScreenCaptureWithConstraints) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  const int kMaxWidth = 200;
  const int kMaxFrameRate = 6;
  const std::string constraints = base::StringPrintf(
      "{video: {width: {max: %d}, frameRate: {max: %d}}, "
      "should_prefer_current_tab: "
      "%s}",
      kMaxWidth, kMaxFrameRate,
      test_config_.should_prefer_current_tab ? "true" : "false");
  RunGetDisplayMedia(tab, constraints,
                     /*is_fake_ui=*/true, /*expect_success=*/true,
                     /*is_tab_capture=*/PreferCurrentTab());

  EXPECT_EQ(content::EvalJs(tab->GetPrimaryMainFrame(), "getWidthSetting();"),
            base::StringPrintf("%d", kMaxWidth));

  EXPECT_EQ(
      content::EvalJs(tab->GetPrimaryMainFrame(), "getFrameRateSetting();"),
      base::StringPrintf("%d", kMaxFrameRate));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebRtcScreenCaptureBrowserTestWithFakeUI,
    testing::Values(TestConfigForFakeUI{/*should_prefer_current_tab=*/false,
                                        /*display_surface=*/"monitor"},
                    TestConfigForFakeUI{/*should_prefer_current_tab=*/false,
                                        /*display_surface=*/"window"},
                    TestConfigForFakeUI{/*should_prefer_current_tab=*/false,
                                        /*display_surface=*/"browser"},
                    TestConfigForFakeUI{/*should_prefer_current_tab=*/true,
                                        /*display_surface=*/"browser"}));

class WebRtcScreenCapturePermissionPolicyBrowserTest
    : public WebRtcScreenCaptureBrowserTest,
      public testing::WithParamInterface<
          std::tuple<GetDisplayMediaVariant, bool>> {
 public:
  WebRtcScreenCapturePermissionPolicyBrowserTest()
      : tested_variant_(std::get<0>(GetParam())),
        allowlisted_by_policy_(std::get<1>(GetParam())) {}

  ~WebRtcScreenCapturePermissionPolicyBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(
        switches::kAutoSelectTabCaptureSourceByTitle, kMainHtmlTitle);
  }

  bool PreferCurrentTab() const override {
    return tested_variant_ == GetDisplayMediaVariant::kPreferCurrentTab;
  }

 protected:
  const GetDisplayMediaVariant tested_variant_;
  const bool allowlisted_by_policy_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    WebRtcScreenCapturePermissionPolicyBrowserTest,
    testing::Combine(testing::Values(GetDisplayMediaVariant::kStandard,
                                     GetDisplayMediaVariant::kPreferCurrentTab),
                     /*allowlisted_by_policy=*/testing::Bool()));

// Flaky on Win bots http://crbug.com/1264805
#if BUILDFLAG(IS_WIN)
#define MAYBE_ScreenShareFromEmbedded DISABLED_ScreenShareFromEmbedded
#else
#define MAYBE_ScreenShareFromEmbedded ScreenShareFromEmbedded
#endif
IN_PROC_BROWSER_TEST_P(WebRtcScreenCapturePermissionPolicyBrowserTest,
                       MAYBE_ScreenShareFromEmbedded) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // The use of selfBrowserSurface is in order to simplify the test by
  // using just one tab. It is orthogonal to the test's purpose.
  const std::string constraints = base::StringPrintf(
      "{video: true, selfBrowserSurface: 'include', preferCurrentTab: %s}",
      PreferCurrentTab() ? "true" : "false");

  EXPECT_EQ(
      content::EvalJs(
          OpenTestPageInNewTab(kMainHtmlPage)->GetPrimaryMainFrame(),
          base::StringPrintf(
              "runGetDisplayMedia(%s, \"%s\");", constraints.c_str(),
              allowlisted_by_policy_ ? "allowedFrame" : "disallowedFrame")),
      allowlisted_by_policy_ ? "embedded-capture-success"
                             : "embedded-capture-failure");
}

// Test class used to test WebRTC with App Windows. Unfortunately, due to
// creating a diamond pattern of inheritance, we can only inherit from one of
// the PlatformAppBrowserTest and WebRtcBrowserTestBase (or it's children).
// We need a lot more heavy lifting on creating the AppWindow than we would get
// from WebRtcBrowserTestBase; so we inherit from PlatformAppBrowserTest to
// minimize the code duplication.
class WebRtcAppWindowCaptureBrowserTestWithPicker
    : public extensions::PlatformAppBrowserTest {
 public:
  WebRtcAppWindowCaptureBrowserTestWithPicker() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(
        switches::kAutoSelectTabCaptureSourceByTitle, kAppWindowTitle);

    AdjustCommandLineForZeroCopyCapture(command_line);
  }

  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(StartEmbeddedTestServer());

    // We will restrict all pages to "Tab Capture" only. This should force App
    // Windows to show up in the tabs list, and thus make it selectable.
    base::Value::List matchlist;
    matchlist.Append("*");
    browser()->profile()->GetPrefs()->SetList(
        prefs::kTabCaptureAllowedByOrigins, std::move(matchlist));
  }

  void TearDownOnMainThread() override {
    extensions::PlatformAppBrowserTest::TearDownOnMainThread();
    browser()->profile()->GetPrefs()->SetList(
        prefs::kTabCaptureAllowedByOrigins, base::Value::List());
  }

  extensions::AppWindow* CreateAppWindowWithTitle(const std::u16string& title) {
    extensions::AppWindow* app_window = CreateTestAppWindow("{}");
    EXPECT_TRUE(app_window);
    UpdateWebContentsTitle(app_window->web_contents(), title);

    return app_window;
  }

  // This is mostly lifted from WebRtcBrowserTestBase, with the exception that
  // because we know we're setting the auto-accept switches, we don't need to
  // set the PermissionsManager auto accept.
  content::WebContents* OpenTestPageInNewTab(const std::string& test_url) {
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
    GURL url = embedded_test_server()->GetURL(test_url);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcAppWindowCaptureBrowserTestWithPicker,
                       CaptureAppWindow) {
  extensions::AppWindow* app_window =
      CreateAppWindowWithTitle(base::UTF8ToUTF16(std::string(kAppWindowTitle)));
  content::WebContents* capturing_tab = OpenTestPageInNewTab(kMainHtmlPage);

  RunGetDisplayMedia(capturing_tab, "{video: true}", /*is_fake_ui=*/false,
                     /*expect_success=*/true,
                     /*is_tab_capture=*/true);
  CloseAppWindow(app_window);
}

// Base class for running tests with a SameOrigin policy applied.
class WebRtcSameOriginPolicyBrowserTest
    : public WebRtcScreenCaptureBrowserTest {
 public:
  ~WebRtcSameOriginPolicyBrowserTest() override = default;

  bool PreferCurrentTab() const override { return false; }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcScreenCaptureBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(
        switches::kAutoSelectTabCaptureSourceByTitle, kSameOriginRenamedTitle);

    AdjustCommandLineForZeroCopyCapture(command_line);
  }

  void SetUpOnMainThread() override {
    WebRtcScreenCaptureBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    // Restrict all origins to SameOrigin tab capture only.
    base::Value::List matchlist;
    matchlist.Append("*");
    browser()->profile()->GetPrefs()->SetList(
        prefs::kSameOriginTabCaptureAllowedByOrigins, std::move(matchlist));
  }

  void TearDownOnMainThread() override {
    WebRtcScreenCaptureBrowserTest::TearDownOnMainThread();
    browser()->profile()->GetPrefs()->SetList(
        prefs::kSameOriginTabCaptureAllowedByOrigins, base::Value::List());
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcSameOriginPolicyBrowserTest,
                       TerminateOnNavigationAwayFromSameOrigin) {
  // Open two pages, one to be captured, and one to do the capturing. Note that
  // we open the capturing page second so that is focused to allow the
  // getDisplayMedia request to succeed.
  content::WebContents* target_tab = OpenTestPageInNewTab(kMainHtmlPage);
  content::WebContents* capturing_tab = OpenTestPageInNewTab(kMainHtmlPage);

  // Update the target tab to a unique title, so that we can ensure that it is
  // the one that gets captured via the autoselection.
  UpdateWebContentsTitle(
      target_tab, base::UTF8ToUTF16(std::string(kSameOriginRenamedTitle)));
  RunGetDisplayMedia(capturing_tab,
                     GetConstraints(
                         /*video=*/true, /*audio=*/true,
                         /*select_all_screens=*/
                         SelectAllScreens::kUndefined),
                     /*is_fake_ui=*/false, /*expect_success=*/true,
                     /*is_tab_capture=*/true);

  // Though the target tab should've been focused as a result of starting the
  // capture, we don't want to take a dependency on that behavior. Ensure that
  // the target tab is focused, so that we can navigate it easily. If it is
  // already focused, this will just no-op.
  int target_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(target_tab);
  browser()->tab_strip_model()->ActivateTabAt(
      target_index, TabStripUserGestureDetails(
                        TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(target_tab, browser()->tab_strip_model()->GetActiveWebContents());

  // We navigate to a FileURL so that the origin will change, which should
  // trigger the capture to end.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetFileURL(kMainHtmlFileName)));

  // Verify that the video stream has ended.
  EXPECT_EQ(content::EvalJs(capturing_tab->GetPrimaryMainFrame(),
                            "waitVideoEnded();"),
            "ended");
}

IN_PROC_BROWSER_TEST_F(WebRtcSameOriginPolicyBrowserTest,
                       ContinueCapturingForSameOriginNavigation) {
  // Open two pages, one to be captured, and one to do the capturing. Note that
  // we open the capturing page second so that is focused to allow the
  // getDisplayMedia request to succeed.
  content::WebContents* target_tab = OpenTestPageInNewTab(kMainHtmlPage);
  content::WebContents* capturing_tab = OpenTestPageInNewTab(kMainHtmlPage);

  // Update the target tab to a unique title, so that we can ensure that it is
  // the one that gets captured via the autoselection.
  UpdateWebContentsTitle(
      target_tab, base::UTF8ToUTF16(std::string(kSameOriginRenamedTitle)));
  RunGetDisplayMedia(capturing_tab,
                     GetConstraints(
                         /*video=*/true, /*audio=*/true,
                         /*select_all_screens=*/
                         SelectAllScreens::kUndefined),
                     /*is_fake_ui=*/false, /*expect_success=*/true,
                     /*is_tab_capture=*/true);

  // Though the target tab should've been focused as a result of starting the
  // capture, we don't want to take a dependency on that behavior. Ensure that
  // the target tab is focused, so that we can navigate it easily. If it is
  // already focused, this will just no-op.
  int target_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(target_tab);
  browser()->tab_strip_model()->ActivateTabAt(
      target_index, TabStripUserGestureDetails(
                        TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(target_tab, browser()->tab_strip_model()->GetActiveWebContents());

  // We navigate using the test server so that the origin doesn't change.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/webrtc/captured_page_main.html")));

  // Verify that the video hasn't been ended.
  EXPECT_EQ(content::EvalJs(capturing_tab->GetPrimaryMainFrame(),
                            "video_track.readyState;"),
            "live");
}

class GetDisplayMediaVideoTrackBrowserTest
    : public WebRtcTestBase,
      public testing::WithParamInterface<std::tuple<bool, DisplaySurfaceType>> {
 public:
  GetDisplayMediaVideoTrackBrowserTest()
      : region_capture_enabled_(std::get<0>(GetParam())),
        display_surface_type_(std::get<1>(GetParam())) {}

  ~GetDisplayMediaVideoTrackBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }

  void SetUpOnMainThread() override {
    WebRtcTestBase::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Unlike SetUp(), this is called from the test body. This allows skipping
  // this test for (platform, test-case) combinations which are not supported.
  void SetupTest() {
    // Fire up the page.
    tab_ = OpenTestPageInNewTab(kMainHtmlPage);

    // Initiate the capture.
    ASSERT_EQ("capture-success",
              content::EvalJs(tab_->GetPrimaryMainFrame(),
                              "runGetDisplayMedia({video: true, audio: true}, "
                              "\"top-level-document\");"));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcTestBase::SetUpCommandLine(command_line);

    std::vector<std::string> enabled_blink_features;
    std::vector<std::string> disabled_blink_features;

    if (region_capture_enabled_) {
      enabled_blink_features.push_back("RegionCapture");
    } else {
      disabled_blink_features.push_back("RegionCapture");
    }

    if (!enabled_blink_features.empty()) {
      command_line->AppendSwitchASCII(
          switches::kEnableBlinkFeatures,
          base::JoinString(enabled_blink_features, ","));
    }

    if (!disabled_blink_features.empty()) {
      command_line->AppendSwitchASCII(
          switches::kDisableBlinkFeatures,
          base::JoinString(disabled_blink_features, ","));
    }

    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    command_line->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        base::StrCat({"display-media-type=",
                      DisplaySurfaceTypeAsString(display_surface_type_)}));

    AdjustCommandLineForZeroCopyCapture(command_line);
  }

  std::string GetVideoTrackType() {
    return content::EvalJs(tab_->GetPrimaryMainFrame(), "getVideoTrackType();")
        .ExtractString();
  }

  std::string GetVideoCloneTrackType() {
    return content::EvalJs(tab_->GetPrimaryMainFrame(),
                           "getVideoCloneTrackType();")
        .ExtractString();
  }

  bool HasAudioTrack() {
    std::string result =
        content::EvalJs(tab_->GetPrimaryMainFrame(), "hasAudioTrack();")
            .ExtractString();
    EXPECT_TRUE(result == "true" || result == "false");
    return result == "true";
  }

  std::string GetAudioTrackType() {
    return content::EvalJs(tab_->GetPrimaryMainFrame(), "getAudioTrackType();")
        .ExtractString();
  }

  std::string ExpectedVideoTrackType() const {
    switch (display_surface_type_) {
      case DisplaySurfaceType::kTab:
        return region_capture_enabled_ ? "BrowserCaptureMediaStreamTrack"
                                       : "MediaStreamTrack";
      case DisplaySurfaceType::kWindow:
      case DisplaySurfaceType::kScreen:
        return "MediaStreamTrack";
    }
    NOTREACHED_NORETURN();
  }

 protected:
  const bool region_capture_enabled_;
  const DisplaySurfaceType display_surface_type_;

 private:
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> tab_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(
    _,
    GetDisplayMediaVideoTrackBrowserTest,
    testing::Combine(/*region_capture_enabled=*/testing::Bool(),
                     /*display_surface_type=*/
                     testing::Values(DisplaySurfaceType::kTab,
                                     DisplaySurfaceType::kWindow,
                                     DisplaySurfaceType::kScreen)),
    [](const testing::TestParamInfo<
        GetDisplayMediaVideoTrackBrowserTest::ParamType>& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "RegionCapture" : "",
           std::get<1>(info.param) == DisplaySurfaceType::kTab ? "Tab"
           : std::get<1>(info.param) == DisplaySurfaceType::kWindow
               ? "Window"
               : "Screen"});
    });

// Normally, each of these these would have its own test, but the number of
// combinations and the setup time for browser-tests make this undesirable,
// especially given the simplicity of each of these tests.
// After both (a) Conditional Focus and (b) Region Capture ship, this can
// simpplified to three non-parameterized tests (tab/window/screen).
IN_PROC_BROWSER_TEST_P(GetDisplayMediaVideoTrackBrowserTest, RunCombinedTest) {
  SetupTest();

  // Test #1: The video track is of the expected type.
  EXPECT_EQ(GetVideoTrackType(), ExpectedVideoTrackType());

  // Test #2: Video clones are of the same type as the original.
  EXPECT_EQ(GetVideoTrackType(), GetVideoCloneTrackType());

  // Test #3: Audio tracks are all simply MediaStreamTrack.
  if (HasAudioTrack()) {
    EXPECT_EQ(GetAudioTrackType(), "MediaStreamTrack");
  }
}

// Flaky on Mac, Windows, and ChromeOS bots, https://crbug.com/1371309
// Also some flakes on Linux ASAN/MSAN builds.
#if BUILDFLAG(IS_LINUX) && \
    !(defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER))
class GetDisplayMediaHiDpiBrowserTest
    : public WebRtcTestBase,
      public testing::WithParamInterface<TestConfigForHiDpi> {
 public:
  GetDisplayMediaHiDpiBrowserTest() : test_config_(GetParam()) {}

  // The browser window size must be consistent with the
  // INSTANTIATE_TEST_SUITE_P TestConfigForHiDpi configurations below. See the
  // comments there for more details.
  static constexpr int kBrowserWindowWidth = 800;
  static constexpr int kBrowserWindowHeight = 600;

  bool enable_hidpi() const { return test_config_.enable_hidpi; }
  int constraint_width() const { return test_config_.constraint_width; }
  int constraint_height() const { return test_config_.constraint_height; }

  void SetUpInProcessBrowserTestFixture() override {
    if (enable_hidpi()) {
      feature_list_.InitAndEnableFeature(media::kWebContentsCaptureHiDpi);
    } else {
      feature_list_.InitAndDisableFeature(media::kWebContentsCaptureHiDpi);
    }

    WebRtcTestBase::SetUpInProcessBrowserTestFixture();

    DetectErrorsInJavaScript();
  }

  void SetUpOnMainThread() override {
    WebRtcTestBase::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // The picker itself shows previews which are unsupported in Lacros tests.
    base::Value::List matchlist;
    matchlist.Append("*");
    browser()->profile()->GetPrefs()->SetList(
        prefs::kTabCaptureAllowedByOrigins, std::move(matchlist));
#endif

    // Fire up the page.
    tab_ = OpenTestPageInNewTab(kMainHtmlPage);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcTestBase::SetUpCommandLine(command_line);

    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    command_line->AppendSwitch(switches::kThisTabCaptureAutoAccept);

    command_line->AppendSwitchASCII(
        switches::kWindowSize,
        base::StringPrintf("%d,%d", kBrowserWindowWidth, kBrowserWindowHeight));

    // Optionally, in case the test isn't working correctly, you can turn on
    // debug logging for the feature to help track down problems. For example:
    // command_line->AppendSwitchASCII(switches::kVModule,
    //                                "*host_view*=1,*frame_tracker*=3");
  }

  std::string ResizeVideoForHiDpiCapture(int width, int height) {
    return RunJs(base::StringPrintf("resizeVideoForHiDpiCapture(%d, %d);",
                                    width, height));
  }

  double GetDevicePixelRatio() {
    std::string result = RunJs("getDevicePixelRatio();");
    double device_pixel_ratio;
    EXPECT_TRUE(base::StringToDouble(result, &device_pixel_ratio));
    return device_pixel_ratio;
  }

  std::string GetDisplaySurfaceSetting() {
    return RunJs("getDisplaySurfaceSetting();");
  }

  std::string GetLogicalSurfaceSetting() {
    return RunJs("getLogicalSurfaceSetting();");
  }

  content::WebContents* Tab() const { return tab_; }

 private:
  std::string RunJs(const std::string& command) {
    return content::EvalJs(tab_->GetPrimaryMainFrame(), command)
        .ExtractString();
  }

  base::test::ScopedFeatureList feature_list_;
  const TestConfigForHiDpi test_config_;
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> tab_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(GetDisplayMediaHiDpiBrowserTest, Capture) {
  ASSERT_EQ(GetDevicePixelRatio(), 1.0);

  // Initiate the capture.
  RunGetDisplayMedia(
      Tab(),
      base::StringPrintf("{video: {width: {max: %d}, height: {max: %d}}, "
                         "preferCurrentTab: true}",
                         constraint_width(), constraint_height()),
      /*is_fake_ui=*/false, /*expect_success=*/true,
      /*is_tab_capture=*/true);

  // Ensure that the video is larger than the source tab to encourage use of a
  // higher-resolution video stream. The size is arbitrary, but it should be
  // significantly bigger than the switches::kWindowSize configured in this
  // test's setup.
  EXPECT_EQ(ResizeVideoForHiDpiCapture(kBrowserWindowWidth * 2,
                                       kBrowserWindowHeight * 2),
            "success");

  EXPECT_EQ(GetDisplaySurfaceSetting(), "browser");

  EXPECT_EQ(GetLogicalSurfaceSetting(), "true");

  // The HiDPI scale change only occurs once the capture has actually started
  // and the size information was propagated back to the browser process.
  // Waiting for the video to start playing helps ensure that this is the case.
  StartDetectingVideo(Tab(), "local-view");
  WaitForVideoToPlay(Tab());

  // If the video size is higher resolution than the browser window
  // size, expect that HiDPI mode should be active. This requires
  // the feature to be enabled.
  bool expect_hidpi = enable_hidpi() &&
                      constraint_width() > kBrowserWindowWidth &&
                      constraint_height() > kBrowserWindowHeight;

  double device_pixel_ratio = GetDevicePixelRatio();
  if (expect_hidpi) {
    EXPECT_GT(device_pixel_ratio, 1.0);
    EXPECT_LE(device_pixel_ratio, 2.0);
  } else {
    EXPECT_EQ(device_pixel_ratio, 1.0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GetDisplayMediaHiDpiBrowserTest,
    // The test configurations use both large and small constraint sizes. The
    // small constraint sizes must be smaller than the configured window size
    // (cf. kBrowserWindowWidth and kBrowserWindowHeight in
    // GetDisplayMediaHiDpiBrowserTest above), and the large sizes must be
    // significantly larger than the browser window size.
    testing::Values(TestConfigForHiDpi{/*enable_hidpi=*/false,
                                       /*constraint_width=*/3840,
                                       /*constraint_height=*/2160},
                    TestConfigForHiDpi{/*enable_hidpi=*/true,
                                       /*constraint_width=*/640,
                                       /*constraint_height=*/480},
                    TestConfigForHiDpi{/*enable_hidpi=*/true,
                                       /*constraint_width=*/3840,
                                       /*constraint_height=*/2160}));
#endif

class GetDisplayMediaChangeSourceBrowserTest
    : public WebRtcTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  GetDisplayMediaChangeSourceBrowserTest()
      : dynamic_surface_switching_requested_(std::get<0>(GetParam())),
        feature_enabled_(std::get<1>(GetParam())),
        user_shared_audio_(std::get<2>(GetParam())) {}
  ~GetDisplayMediaChangeSourceBrowserTest() override = default;

  void SetUp() override {
    // TODO(crbug.com/1381951): Fix GetDisplayMediaChangeSourceBrowserTest with
    // audio requested on ChromeOS
#if (BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS))
    if (dynamic_surface_switching_requested_ && feature_enabled_ &&
        user_shared_audio_) {
      GTEST_SKIP();
    }
#endif
    WebRtcTestBase::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitWithFeatureState(
        media::kShareThisTabInsteadButtonGetDisplayMedia, feature_enabled_);

    WebRtcTestBase::SetUpInProcessBrowserTestFixture();

    DetectErrorsInJavaScript();

    base::FilePath test_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(
        switches::kAutoSelectTabCaptureSourceByTitle, kCapturedTabTitle);

    AdjustCommandLineForZeroCopyCapture(command_line);

    if (!user_shared_audio_) {
      command_line->AppendSwitch(switches::kScreenCaptureAudioDefaultUnchecked);
    }
  }

  std::string GetConstraints() const {
    return base::StringPrintf(
        "{video: true, audio: true, surfaceSwitching: \"%s\"}",
        dynamic_surface_switching_requested_ ? "include" : "exclude");
  }

  bool ShouldShowShareThisTabInsteadButton() const {
    return dynamic_surface_switching_requested_ && feature_enabled_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  const bool dynamic_surface_switching_requested_;
  const bool feature_enabled_;
  const bool user_shared_audio_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         GetDisplayMediaChangeSourceBrowserTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// TODO(1428806) Re-enable flaky test.
IN_PROC_BROWSER_TEST_P(GetDisplayMediaChangeSourceBrowserTest,
                       DISABLED_ChangeSource) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* captured_tab = OpenTestPageInNewTab(kCapturedPageMain);
  content::WebContents* other_tab = OpenTestPageInNewTab(kMainHtmlPage);
  content::WebContents* capturing_tab = OpenTestPageInNewTab(kMainHtmlPage);

  RunGetDisplayMedia(capturing_tab, GetConstraints(), /*is_fake_ui=*/false,
                     /*expect_success=*/true,
                     /*is_tab_capture=*/true);

  EXPECT_TRUE(captured_tab->IsBeingCaptured());
  EXPECT_FALSE(other_tab->IsBeingCaptured());
  EXPECT_FALSE(capturing_tab->IsBeingCaptured());
  EXPECT_EQ(
      GetSecondaryButtonLabel(captured_tab),
      l10n_util::GetStringFUTF16(
          IDS_TAB_SHARING_INFOBAR_SWITCH_TO_BUTTON,
          url_formatter::FormatOriginForSecurityDisplay(
              captured_tab->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)));
  EXPECT_EQ(
      GetSecondaryButtonLabel(capturing_tab),
      l10n_util::GetStringFUTF16(
          IDS_TAB_SHARING_INFOBAR_SWITCH_TO_BUTTON,
          url_formatter::FormatOriginForSecurityDisplay(
              capturing_tab->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)));
  if (!ShouldShowShareThisTabInsteadButton()) {
    EXPECT_FALSE(HasSecondaryButton(other_tab));
    return;
  }
  EXPECT_EQ(GetSecondaryButtonLabel(other_tab), kShareThisTabInsteadMessage);

  // Click the secondary button, i.e., the "Share this tab instead" button
  GetDelegate(other_tab)->Cancel();

  // Wait until the capture of the other tab has started.
  while (!other_tab->IsBeingCaptured()) {
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(captured_tab->IsBeingCaptured());
  EXPECT_TRUE(other_tab->IsBeingCaptured());
  EXPECT_FALSE(capturing_tab->IsBeingCaptured());
  EXPECT_EQ(GetSecondaryButtonLabel(captured_tab), kShareThisTabInsteadMessage);
  EXPECT_EQ(GetSecondaryButtonLabel(other_tab),
            l10n_util::GetStringFUTF16(
                IDS_TAB_SHARING_INFOBAR_SWITCH_TO_BUTTON,
                url_formatter::FormatOriginForSecurityDisplay(
                    other_tab->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
                    url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)));
  EXPECT_EQ(
      GetSecondaryButtonLabel(capturing_tab),
      l10n_util::GetStringFUTF16(
          IDS_TAB_SHARING_INFOBAR_SWITCH_TO_BUTTON,
          url_formatter::FormatOriginForSecurityDisplay(
              capturing_tab->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)));
}
// TODO(1428806) Re-enable flaky test.
IN_PROC_BROWSER_TEST_P(GetDisplayMediaChangeSourceBrowserTest,
                       DISABLED_ChangeSourceThenStopTracksRemovesIndicators) {
  if (!ShouldShowShareThisTabInsteadButton()) {
    GTEST_SKIP();
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  OpenTestPageInNewTab(kCapturedPageMain);
  content::WebContents* other_tab = OpenTestPageInNewTab(kMainHtmlPage);
  content::WebContents* capturing_tab = OpenTestPageInNewTab(kMainHtmlPage);

  RunGetDisplayMedia(capturing_tab, GetConstraints(), /*is_fake_ui=*/false,
                     /*expect_success=*/true,
                     /*is_tab_capture=*/true);

  // Click the secondary button, i.e., the "Share this tab instead" button
  GetDelegate(other_tab)->Cancel();

  // Wait until the capture of the other tab has started.
  while (!other_tab->IsBeingCaptured()) {
    base::RunLoop().RunUntilIdle();
  }

  ASSERT_EQ(GetInfoBarManager(capturing_tab)->infobars().size(), 1u);
  StopAllTracks(capturing_tab);
  do {
    base::RunLoop().RunUntilIdle();
  } while (GetInfoBarManager(capturing_tab)->infobars().size() > 0u);
}

// TODO(1428806) Re-enable flaky test.
IN_PROC_BROWSER_TEST_P(GetDisplayMediaChangeSourceBrowserTest,
                       DISABLED_ChangeSourceReject) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* captured_tab = OpenTestPageInNewTab(kCapturedPageMain);
  content::WebContents* other_tab = OpenTestPageInNewTab(kMainHtmlPage);
  content::WebContents* capturing_tab = OpenTestPageInNewTab(kMainHtmlPage);

  RunGetDisplayMedia(capturing_tab, GetConstraints(), /*is_fake_ui=*/false,
                     /*expect_success=*/true,
                     /*is_tab_capture=*/true);

  EXPECT_TRUE(captured_tab->IsBeingCaptured());
  EXPECT_FALSE(other_tab->IsBeingCaptured());
  EXPECT_FALSE(capturing_tab->IsBeingCaptured());
  EXPECT_EQ(
      GetSecondaryButtonLabel(captured_tab),
      l10n_util::GetStringFUTF16(
          IDS_TAB_SHARING_INFOBAR_SWITCH_TO_BUTTON,
          url_formatter::FormatOriginForSecurityDisplay(
              captured_tab->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)));
  EXPECT_EQ(
      GetSecondaryButtonLabel(capturing_tab),
      l10n_util::GetStringFUTF16(
          IDS_TAB_SHARING_INFOBAR_SWITCH_TO_BUTTON,
          url_formatter::FormatOriginForSecurityDisplay(
              capturing_tab->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)));
  if (!ShouldShowShareThisTabInsteadButton()) {
    EXPECT_FALSE(HasSecondaryButton(other_tab));
    return;
  }
  EXPECT_EQ(GetSecondaryButtonLabel(other_tab), kShareThisTabInsteadMessage);

  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(other_tab));
  while (browser()->tab_strip_model()->GetActiveWebContents() != other_tab) {
    base::RunLoop().RunUntilIdle();
  }

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kScreenCaptureAllowed,
                                               false);

  // Click the secondary button, i.e., the "Share this tab instead" button. This
  // is rejected since screen capture is not allowed by the above policy.
  GetDelegate(other_tab)->Cancel();

  // When "Share this tab instead" fails for other_tab, the focus goes back to
  // the captured tab. Wait until that happens:
  while (browser()->tab_strip_model()->GetActiveWebContents() != captured_tab) {
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_TRUE(captured_tab->IsBeingCaptured());
  EXPECT_FALSE(other_tab->IsBeingCaptured());
  EXPECT_FALSE(capturing_tab->IsBeingCaptured());
  EXPECT_EQ(
      GetSecondaryButtonLabel(captured_tab),
      l10n_util::GetStringFUTF16(
          IDS_TAB_SHARING_INFOBAR_SWITCH_TO_BUTTON,
          url_formatter::FormatOriginForSecurityDisplay(
              captured_tab->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)));
  EXPECT_EQ(GetSecondaryButtonLabel(other_tab), kShareThisTabInsteadMessage);
  EXPECT_EQ(
      GetSecondaryButtonLabel(capturing_tab),
      l10n_util::GetStringFUTF16(
          IDS_TAB_SHARING_INFOBAR_SWITCH_TO_BUTTON,
          url_formatter::FormatOriginForSecurityDisplay(
              capturing_tab->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)));
}

class GetDisplayMediaSelfBrowserSurfaceBrowserTest
    : public WebRtcTestBase,
      public testing::WithParamInterface<std::string> {
 public:
  GetDisplayMediaSelfBrowserSurfaceBrowserTest()
      : self_browser_surface_(GetParam()) {}

  void SetUpInProcessBrowserTestFixture() override {
    WebRtcTestBase::SetUpInProcessBrowserTestFixture();

    DetectErrorsInJavaScript();

    base::FilePath test_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    command_line->AppendSwitchASCII(
        switches::kAutoSelectTabCaptureSourceByTitle, kMainHtmlTitle);

    AdjustCommandLineForZeroCopyCapture(command_line);
  }

  std::string GetConstraints(bool prefer_current_tab = false) {
    std::vector<std::string> constraints = {"video: true"};
    if (!self_browser_surface_.empty()) {
      constraints.push_back(base::StringPrintf("selfBrowserSurface: \"%s\"",
                                               self_browser_surface_.c_str()));
    }
    if (prefer_current_tab) {
      constraints.push_back("preferCurrentTab: true");
    }
    prefer_current_tab_ = prefer_current_tab;
    return "{" + base::JoinString(constraints, ",") + "}";
  }

  bool IsSelfBrowserSurfaceExclude() const {
    if (self_browser_surface_ == "" && !prefer_current_tab_) {
      // Special case - when using the new order, selfBrowserSurface
      // defaults to "exclude", unless {preferCurrentTab: true} is specified.
      return true;
    }
    return self_browser_surface_ == "exclude";
  }

 protected:
  // If empty, the constraint is unused. Otherwise, the value is either
  // "include" or "exclude"
  const std::string self_browser_surface_;

  // Whether {preferCurrentTab: true} will be specified by the test.
  bool prefer_current_tab_ = false;
};

INSTANTIATE_TEST_SUITE_P(All,
                         GetDisplayMediaSelfBrowserSurfaceBrowserTest,
                         testing::Values("", "include", "exclude"));

IN_PROC_BROWSER_TEST_P(GetDisplayMediaSelfBrowserSurfaceBrowserTest,
                       SelfBrowserSurfaceChangesCapturedTab) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // This test relies on |capturing_tab| appearing earlier in the media picker,
  // and being auto-selected earlier if it is offered.
  content::WebContents* other_tab = OpenTestPageInNewTab(kMainHtmlPage);
  content::WebContents* capturing_tab = OpenTestPageInNewTab(kMainHtmlPage);

  // Success expected either way, with the *other* tab being captured
  // when selfBrowserCapture is set to "exclude".
  RunGetDisplayMedia(capturing_tab, GetConstraints(), /*is_fake_ui=*/false,
                     /*expect_success=*/true, /*is_tab_capture=*/true);

  EXPECT_EQ(!IsSelfBrowserSurfaceExclude(), capturing_tab->IsBeingCaptured());
  EXPECT_EQ(IsSelfBrowserSurfaceExclude(), other_tab->IsBeingCaptured());
}

IN_PROC_BROWSER_TEST_P(GetDisplayMediaSelfBrowserSurfaceBrowserTest,
                       SelfBrowserSurfaceInteractionWithPreferCurrentTab) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // This test relies on |capturing_tab| appearing earlier in the media picker,
  // and being auto-selected earlier if it is offered.
  content::WebContents* other_tab = OpenTestPageInNewTab(kMainHtmlPage);
  content::WebContents* capturing_tab = OpenTestPageInNewTab(kMainHtmlPage);

  // Test focal point - getDisplayMedia() rejects if preferCurrentTab
  // and exclude-current-tab are simultaneously specified.
  // Note that preferCurrentTab is hard-coded in this test while
  // exclude-current-tab is parameterized.
  const bool expect_success = (self_browser_surface_ != "exclude");
  const std::string expected_error =
      expect_success ? ""
                     : "TypeError: Failed to execute 'getDisplayMedia' on "
                       "'MediaDevices': Self-contradictory configuration "
                       "(preferCurrentTab and selfBrowserSurface=exclude).";
  RunGetDisplayMedia(capturing_tab, GetConstraints(/*prefer_current_tab=*/true),
                     /*is_fake_ui=*/false, expect_success,
                     /*is_tab_capture=*/true, expected_error);

  EXPECT_EQ(!IsSelfBrowserSurfaceExclude(), capturing_tab->IsBeingCaptured());
  EXPECT_FALSE(other_tab->IsBeingCaptured());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_CHROMEOS_ASH)

class WebRtcScreenCaptureSelectAllScreensTest
    : public WebRtcScreenCaptureBrowserTest,
      public testing::WithParamInterface<TestConfigForSelectAllScreens> {
 public:
  WebRtcScreenCaptureSelectAllScreensTest() : test_config_(GetParam()) {}
  ~WebRtcScreenCaptureSelectAllScreensTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enables GetAllScreensMedia features for multi surface capture.
    // TODO(simonha): remove when feature becomes stable.
    if (test_config_.enable_select_all_screens) {
      command_line->AppendSwitch(switches::kEnableBlinkTestFeatures);
    }
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    command_line->RemoveSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        base::StringPrintf("display-media-type=%s",
                           test_config_.display_surface));
  }

  bool PreferCurrentTab() const override { return false; }

 protected:
  TestConfigForSelectAllScreens test_config_;
};

IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureSelectAllScreensTest,
                       GetDisplayMediaAutoSelectAllScreensTrueDisallowed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  RunGetDisplayMedia(tab,
                     GetConstraints(/*video=*/true, /*audio=*/false,
                                    /*select_all_screens=*/
                                    SelectAllScreens::kTrue),
                     /*is_fake_ui=*/true,
                     /*expect_success=*/!test_config_.enable_select_all_screens,
                     /*is_tab_capture=*/false);
}

IN_PROC_BROWSER_TEST_P(WebRtcScreenCaptureSelectAllScreensTest,
                       GetDisplayMediaAutoSelectAllScreensFalseAlwaysAllowed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);
  RunGetDisplayMedia(tab,
                     GetConstraints(/*video=*/true, /*audio=*/false,
                                    /*select_all_screens=*/
                                    SelectAllScreens::kFalse),
                     /*is_fake_ui=*/true, /*expect_success=*/true,
                     /*is_tab_capture=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebRtcScreenCaptureSelectAllScreensTest,
    testing::Values(
        TestConfigForSelectAllScreens{/*display_surface=*/"browser",
                                      /*enable_select_all_screens=*/true},
        TestConfigForSelectAllScreens{/*display_surface=*/"browser",
                                      /*enable_select_all_screens=*/false},
        TestConfigForSelectAllScreens{/*display_surface=*/"window",
                                      /*enable_select_all_screens=*/true},
        TestConfigForSelectAllScreens{/*display_surface=*/"window",
                                      /*enable_select_all_screens=*/false},
        TestConfigForSelectAllScreens{/*display_surface=*/"monitor",
                                      /*enable_select_all_screens=*/true},
        TestConfigForSelectAllScreens{/*display_surface=*/"monitor",
                                      /*enable_select_all_screens=*/false}));

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_CHROMEOS_ASH)

class GetDisplayMediaTransientActivationRequiredTest
    : public WebRtcScreenCaptureBrowserTest,
      public testing::WithParamInterface<
          std::tuple<bool, bool, bool, absl::optional<std::string>>> {
 public:
  GetDisplayMediaTransientActivationRequiredTest()
      : with_user_gesture_(std::get<0>(GetParam())),
        require_gesture_feature_enabled_(std::get<1>(GetParam())),
        prefer_current_tab_(std::get<2>(GetParam())),
        policy_allowlist_value_(std::get<3>(GetParam())) {}
  ~GetDisplayMediaTransientActivationRequiredTest() override = default;

  static std::string GetDescription(
      const testing::TestParamInfo<
          GetDisplayMediaTransientActivationRequiredTest::ParamType>& info) {
    std::string name = base::StrCat(
        {std::get<0>(info.param) ? "WithUserGesture_" : "WithoutUserGesture_",
         std::get<1>(info.param) ? "RequireGestureFeatureEnabled_"
                                 : "_RequireGestureFeatureDisabled_",
         std::get<2>(info.param) ? "PreferCurrentTab_"
                                 : "DontPreferCurrentTab_",
         std::get<3>(info.param).has_value()
             ? (*std::get<3>(info.param) == kEmbeddedTestServerOrigin)
                   ? "Allowlisted"
                   : "OtherAllowlisted"
             : "NoPolicySet"});
    return name;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
  }

  void SetUpInProcessBrowserTestFixture() override {
    WebRtcScreenCaptureBrowserTest::SetUpInProcessBrowserTestFixture();

    if (require_gesture_feature_enabled_) {
      feature_list_.InitAndEnableFeature(
          blink::features::kGetDisplayMediaRequiresUserActivation);
    } else {
      feature_list_.InitAndDisableFeature(
          blink::features::kGetDisplayMediaRequiresUserActivation);
    }

    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    DetectErrorsInJavaScript();
  }

  bool PreferCurrentTab() const override { return prefer_current_tab_; }

 protected:
  const bool with_user_gesture_;
  const bool require_gesture_feature_enabled_;
  const bool prefer_current_tab_;
  const absl::optional<std::string> policy_allowlist_value_;
  base::test::ScopedFeatureList feature_list_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(GetDisplayMediaTransientActivationRequiredTest, Check) {
  ASSERT_TRUE(embedded_test_server()->Start());

  if (policy_allowlist_value_.has_value()) {
    policy::PolicyMap policy_map;
    base::Value::List allowed_origins;
    allowed_origins.Append(base::Value(*policy_allowlist_value_));
    policy_map.Set(policy::key::kScreenCaptureWithoutGestureAllowedForOrigins,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_PLATFORM,
                   base::Value(std::move(allowed_origins)), nullptr);
    policy_provider_.UpdateChromePolicy(policy_map);
  }

  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  const bool expect_success =
      with_user_gesture_ || !require_gesture_feature_enabled_ ||
      (policy_allowlist_value_ &&
       *policy_allowlist_value_ == kEmbeddedTestServerOrigin);
  const std::string expected_error =
      expect_success
          ? ""
          : "InvalidStateError: Failed to execute 'getDisplayMedia' on "
            "'MediaDevices': getDisplayMedia() requires transient activation "
            "(user gesture).";

  RunGetDisplayMedia(tab,
                     GetConstraints(/*video=*/true, /*audio=*/true,
                                    SelectAllScreens::kUndefined),
                     /*is_fake_ui=*/true, expect_success,
                     /*is_tab_capture=*/false, expected_error,
                     with_user_gesture_);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GetDisplayMediaTransientActivationRequiredTest,
    testing::Combine(
        /*with_user_gesture=*/testing::Bool(),
        /*require_gesture_feature_enabled=*/testing::Bool(),
        /*prefer_current_tab=*/testing::Bool(),
        /*policy_allowlist_value=*/
        testing::Values(absl::nullopt,
                        kEmbeddedTestServerOrigin,
                        kOtherOrigin)),
    &GetDisplayMediaTransientActivationRequiredTest::GetDescription);
