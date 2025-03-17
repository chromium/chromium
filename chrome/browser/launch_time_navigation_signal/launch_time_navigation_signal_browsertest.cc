// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace {
const char url1[] = "/page_with_image.html";
const char url2[] = "/page_with_svg_image.html";
const char url3[] = "/page_with_iframe.html";

// Values for kRestoreOnStartup pref.
const int kRestoreLastSession = 1;
const int kRestoreUrls = 4;
const int kRestoreNtp = 5;

struct StartupPrefs {
  StartupPrefs() : restore_on_startup(kRestoreNtp) {}
  explicit StartupPrefs(int restore_on_startup_pref_value,
                        const std::vector<std::string>& urls = {})
      : restore_on_startup(restore_on_startup_pref_value),
        urls_to_restore(std::move(urls)) {}

  // The value set for the kRestoreOnStartup pref.
  int restore_on_startup;
  // The list of URLs we set for the kURLsToRestoreOnStartup pref.
  std::vector<std::string> urls_to_restore;
};

struct LaunchNavigationBrowserTestParam {
  LaunchNavigationBrowserTestParam(
      bool enable_feature,
      StartupPrefs prefs,
      const std::vector<std::string>& urls,
      const std::vector<std::string>& urls_after_restart = {},
      const std::vector<std::string>& cmd_line_switches = {})
      : enable_feature(enable_feature),
        startup_prefs(prefs),
        cmd_line_urls(std::move(urls)),
        cmd_line_urls_after_restart(std::move(urls_after_restart)),
        cmd_line_switches(std::move(cmd_line_switches)) {}

  const bool enable_feature;
  const StartupPrefs startup_prefs;
  const std::vector<std::string> cmd_line_urls;
  // `cmd_line_urls_after_restart` is only applicable to test cases under
  // LaunchNavigationBrowserRestartTest.
  const std::vector<std::string> cmd_line_urls_after_restart;
  const std::vector<std::string> cmd_line_switches;
};

void AppendUrlsToCmdLine(base::CommandLine* command_line,
                         net::EmbeddedTestServer* embedded_test_server,
                         const std::vector<std::string>& urls) {
  for (const std::string& url : urls) {
    command_line->AppendArg(embedded_test_server->GetURL(url).spec());
  }
}

base::Value::List ListValueFromTestUrls(
    const std::vector<std::string>& test_urls,
    net::EmbeddedTestServer* embedded_test_server) {
  base::Value::List url_list;
  for (const std::string& url : test_urls) {
    url_list.Append(base::Value(embedded_test_server->GetURL(url).spec()));
  }
  return url_list;
}

bool IsPerformanceNavigationTimingConfidenceEnabled() {
  return base::FeatureList::IsEnabled(
      blink::features::kPerformanceNavigationTimingConfidence);
}
}  // namespace

class LaunchNavigationBrowserTest
    : public InProcessBrowserTest,
      public BrowserListObserver,
      public testing::WithParamInterface<LaunchNavigationBrowserTestParam> {
 public:
  LaunchNavigationBrowserTest() {
    if (GetParam().enable_feature) {
      // Set the noise probability to 18.0 to ensure we always get the
      // computed value back.
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{{features::kNavigationConfidenceEpsilon,
                                 {{"navigation-confidence-epsilon-value",
                                   "18.0"}}},
                                {blink::features::
                                     kPerformanceNavigationTimingConfidence,
                                 {}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          blink::features::kPerformanceNavigationTimingConfidence);
    }
  }

  ~LaunchNavigationBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Backgrounded renderer processes run at a lower priority, causing the
    // tests to take more time to complete. Disable backgrounding so that the
    // tests don't time out.
    command_line->AppendSwitch(switches::kDisableRendererBackgrounding);

    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();

    SetLaunchUrls(command_line);
    for (const std::string& cmd_line_switch : GetParam().cmd_line_switches) {
      command_line->AppendSwitch(cmd_line_switch);
    }
  }

  void CheckActivePageNavigationConfidence(
      const std::string& expected_navigation_confidence) {
    CheckPageNavigationConfidenceForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents(),
        expected_navigation_confidence);
  }

  void CheckUseCounterCount(int expected_count) {
    // Fetch the Blink.UseCounter.Features histogram in every renderer process
    // until reaching, but not exceeding, `expected_count`.
    while (true) {
      content::FetchHistogramsFromChildProcesses();
      metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

      int count = histogram_tester_.GetBucketCount(
          "Blink.UseCounter.Features",
          blink::mojom::WebFeature::kPerformanceNavigationTimingConfidence);
      CHECK_LE(count, expected_count);
      if (count == expected_count) {
        return;
      }

      base::PlatformThread::Sleep(base::Milliseconds(5));
    }
  }

  void CheckPageNavigationConfidenceAt(
      int tab_index,
      const std::string& expected_navigation_confidence) {
    CheckPageNavigationConfidenceForWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(tab_index),
        expected_navigation_confidence);
  }

  void CheckPageNavigationConfidenceForWebContents(
      content::WebContents* const web_contents,
      const std::string& expected_navigation_confidence) {
    CHECK(web_contents);
    AwaitDocumentOnLoadCompleted(web_contents);
    std::string result =
        ExtractNavigationConfidenceFromTargetRenderFrameHost(web_contents);
    EXPECT_EQ(result, expected_navigation_confidence);
  }

  void Navigate(const std::string& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(url)));
  }

  std::string ExtractNavigationConfidenceFromTargetRenderFrameHost(
      const content::ToRenderFrameHost& frame_host) {
    return content::EvalJs(
               frame_host,
               "let navigationEntry = "
               "window.performance.getEntriesByType('navigation')[0];"
               "if ('confidence' in navigationEntry) {"
               "    if (navigationEntry.confidence) {"
               "        navigationEntry.confidence.value;"
               "    } else {"
               "        'null';"
               "    }"
               "} else {"
               "    'undefined';"
               "}")
        .ExtractString();
  }

 private:
  virtual void SetLaunchUrls(base::CommandLine*) = 0;

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

class LaunchNavigationBrowserBasicTest : public LaunchNavigationBrowserTest {
 private:
  void SetLaunchUrls(base::CommandLine* command_line) override {
    if (!GetParam().cmd_line_urls.empty()) {
      AppendUrlsToCmdLine(command_line, embedded_test_server(),
                          GetParam().cmd_line_urls);
    }
  }
};

IN_PROC_BROWSER_TEST_P(LaunchNavigationBrowserBasicTest, CmdLineLaunch) {
  std::vector<size_t> expected_usecounter_count = {0, 0, 0};
  std::vector<std::string> expected_navigation_confidence = {
      "undefined", "undefined", "undefined"};

  if (IsPerformanceNavigationTimingConfidenceEnabled()) {
    expected_navigation_confidence = {"low", "high", "high"};
    expected_usecounter_count = {1, 2, 3};
  }

  CheckActivePageNavigationConfidence(expected_navigation_confidence[0]);
  CheckUseCounterCount(expected_usecounter_count[0]);

  Navigate("/page_with_image.html");
  CheckActivePageNavigationConfidence(expected_navigation_confidence[1]);
  CheckUseCounterCount(expected_usecounter_count[1]);

  Navigate("/hello.html");
  CheckActivePageNavigationConfidence(expected_navigation_confidence[2]);
  CheckUseCounterCount(expected_usecounter_count[2]);
}

INSTANTIATE_TEST_SUITE_P(
    CmdLineURLBasicTestFeatureEnabled,
    LaunchNavigationBrowserBasicTest,
    testing::Values(LaunchNavigationBrowserTestParam(/*enable_feature=*/true,
                                                     StartupPrefs(),
                                                     {url1})));

INSTANTIATE_TEST_SUITE_P(
    CmdLineURLBasicTestFeatureDisabled,
    LaunchNavigationBrowserBasicTest,
    testing::Values(LaunchNavigationBrowserTestParam(/*enable_feature=*/false,
                                                     StartupPrefs(),
                                                     {url1})));

INSTANTIATE_TEST_SUITE_P(
    CmdLineURLIncognitoBasicTestFeatureEnabled,
    LaunchNavigationBrowserBasicTest,
    testing::Values(LaunchNavigationBrowserTestParam(/*enable_feature=*/true,
                                                     StartupPrefs(),
                                                     {url1},
                                                     {},
                                                     {switches::kIncognito})));

INSTANTIATE_TEST_SUITE_P(
    CmdLineURLIncognitoBasicTestFeatureDisabled,
    LaunchNavigationBrowserBasicTest,
    testing::Values(LaunchNavigationBrowserTestParam(/*enable_feature=*/false,
                                                     StartupPrefs(),
                                                     {url1},
                                                     {},
                                                     {switches::kIncognito})));

class LaunchNavigationBrowserRestartTest : public LaunchNavigationBrowserTest {
 public:
  // This function returns the expected number of tabs in various restore
  // scenarios.
  size_t GetExpectedTabCountFromRestore() {
    const ParamType& test_params = GetParam();
    size_t cmd_line_tab_count = test_params.cmd_line_urls_after_restart.size();

    // The default is 1, since we will launch NTP if we aren't restoring other
    // tabs.
    size_t total_tab_count = 1;

    if (test_params.startup_prefs.restore_on_startup == kRestoreLastSession) {
      // If restoring previous session on startup we expect the navigation
      // confidence field to return "low" for all opened tabs.
      CHECK(!test_params.cmd_line_urls.empty());
      total_tab_count = test_params.cmd_line_urls.size() + cmd_line_tab_count;
    } else if (cmd_line_tab_count) {
      // Open URLs passed via the command line
      total_tab_count = cmd_line_tab_count;
    } else if (test_params.startup_prefs.restore_on_startup == kRestoreUrls) {
      // Restore from the user-specified list in prefs
      total_tab_count = test_params.startup_prefs.urls_to_restore.size();
    }

    return total_tab_count;
  }

 private:
  void SetLaunchUrls(base::CommandLine* command_line) override {
    const std::string test_case_name(
        testing::UnitTest::GetInstance()->current_test_info()->name());
    const auto& cmd_line_urls = (test_case_name.find("PRE_") == 0
                                     ? GetParam().cmd_line_urls
                                     : GetParam().cmd_line_urls_after_restart);
    if (!cmd_line_urls.empty()) {
      AppendUrlsToCmdLine(command_line, embedded_test_server(), cmd_line_urls);
    }
  }
};

IN_PROC_BROWSER_TEST_P(LaunchNavigationBrowserRestartTest,
                       PRE_CmdLineURLRestartTest) {
  std::vector<size_t> expected_usecounter_count = {0, 0, 0};
  std::vector<std::string> expected_navigation_confidence = {
      "undefined", "undefined", "undefined"};

  if (IsPerformanceNavigationTimingConfidenceEnabled()) {
    expected_navigation_confidence = {"high", "high", "high"};
    expected_usecounter_count = {1, 2, 3};
  }

  Navigate("/hello.html");
  CheckActivePageNavigationConfidence(expected_navigation_confidence[0]);
  CheckUseCounterCount(expected_usecounter_count[0]);

  Navigate("/page_with_image.html");
  CheckActivePageNavigationConfidence(expected_navigation_confidence[1]);
  CheckUseCounterCount(expected_usecounter_count[1]);

  Navigate("/hello.html");
  CheckActivePageNavigationConfidence(expected_navigation_confidence[2]);
  CheckUseCounterCount(expected_usecounter_count[2]);

  // Set browser startup behavior here for the non-PRE split test.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kRestoreOnStartup, GetParam().startup_prefs.restore_on_startup);
  browser()->profile()->GetPrefs()->SetList(
      prefs::kURLsToRestoreOnStartup,
      ListValueFromTestUrls(GetParam().startup_prefs.urls_to_restore,
                            embedded_test_server()));
}

// We attempt to restart the browser in this test body.
IN_PROC_BROWSER_TEST_P(LaunchNavigationBrowserRestartTest,
                       CmdLineURLRestartTest) {
  const ParamType& test_params = GetParam();
  const size_t expected_initial_tab_count = GetExpectedTabCountFromRestore();
  const bool expect_valid_navigation_confidence =
      IsPerformanceNavigationTimingConfidenceEnabled();
  std::string expected_initial_navigation_confidence_value =
      expect_valid_navigation_confidence ? "low" : "undefined";

  std::vector<std::string> expected_initial_navigation_confidence;
  expected_initial_navigation_confidence.insert(
      expected_initial_navigation_confidence.end(), expected_initial_tab_count,
      expected_initial_navigation_confidence_value);

  PrefService* prefs = browser()->profile()->GetPrefs();
  ASSERT_EQ(prefs->GetUserPrefValue(prefs::kRestoreOnStartup)->GetInt(),
            test_params.startup_prefs.restore_on_startup);
  ASSERT_EQ(expected_initial_tab_count,
            static_cast<size_t>(browser()->tab_strip_model()->count()));
  ASSERT_EQ(expected_initial_tab_count,
            expected_initial_navigation_confidence.size());

  // Validate initial navigation confidence state.
  for (size_t i = 0; i < expected_initial_navigation_confidence.size(); i++) {
    CheckPageNavigationConfidenceAt(i,
                                    expected_initial_navigation_confidence[i]);
  }

  // Confirm new navigations get "high" navigation confidence.
  {
    std::vector<size_t> expected_usecounter_count = {0, 0};
    std::vector<std::string> expected_navigation_confidence = {"undefined",
                                                               "undefined"};

    if (IsPerformanceNavigationTimingConfidenceEnabled()) {
      expected_navigation_confidence = {"high", "high"};
      expected_usecounter_count = {expected_initial_tab_count + 1,
                                   expected_initial_tab_count + 2};
    }

    Navigate("/page_with_image.html");
    CheckActivePageNavigationConfidence(expected_navigation_confidence[0]);

    Navigate("/hello.html");
    CheckActivePageNavigationConfidence(expected_navigation_confidence[1]);
  }
}

// Tests navigation type for pages when the browser is restarted with
// session.restore_on_startup pref set to restore last session on startup.
INSTANTIATE_TEST_SUITE_P(CmdLineURLRestartTestRestorePreviousSession,
                         LaunchNavigationBrowserRestartTest,
                         testing::Values(LaunchNavigationBrowserTestParam(
                             /*enable_feature=*/true,
                             StartupPrefs(kRestoreLastSession),
                             {url1},
                             {url1, url2})));

// Tests navigation type for pages when the browser is restarted with
// session.restore_on_startup pref set to restore a list of urls specified by
// session.startup_urls on startup.
INSTANTIATE_TEST_SUITE_P(CmdLineURLRestartTestRestoreUrlList,
                         LaunchNavigationBrowserRestartTest,
                         testing::Values(LaunchNavigationBrowserTestParam(
                             /*enable_feature=*/true,
                             StartupPrefs(kRestoreUrls, {url1, url2}),
                             {url1},
                             {url1, url2})));

// Tests navigation type for pages when the browser is restarted with
// session.restore_on_startup pref set to open NTP on startup.
INSTANTIATE_TEST_SUITE_P(CmdLineURLRestartTestBasic,
                         LaunchNavigationBrowserRestartTest,
                         testing::Values(LaunchNavigationBrowserTestParam(
                             /*enable_feature=*/true,
                             StartupPrefs(),
                             {url1},
                             {url1, url2})));

class LaunchNavigationBrowserWithIFrameTest
    : public LaunchNavigationBrowserTest {
 protected:
  void CheckActivePageFrameNavigationConfidence(
      const std::string& expected_navigation_confidence) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
    AwaitDocumentOnLoadCompleted(web_contents);

    content::RenderFrameHost* frame_rfh = ChildFrameAt(main_rfh, 0);
    std::string frame_navigation_confidence_result =
        ExtractNavigationConfidenceFromTargetRenderFrameHost(frame_rfh);
    EXPECT_EQ(frame_navigation_confidence_result,
              expected_navigation_confidence);
  }

 private:
  void SetLaunchUrls(base::CommandLine* command_line) override {
    if (!GetParam().cmd_line_urls.empty()) {
      AppendUrlsToCmdLine(command_line, embedded_test_server(),
                          GetParam().cmd_line_urls);
    }
  }
};

IN_PROC_BROWSER_TEST_P(LaunchNavigationBrowserWithIFrameTest,
                       CmdLineLaunchWithIFrame) {
  std::vector<std::string> expected_main_frame_navigation_confidence = {
      "undefined", "undefined"};
  std::vector<std::string> expected_iframe_navigation_confidence = {
      "undefined", "undefined"};

  if (IsPerformanceNavigationTimingConfidenceEnabled()) {
    expected_main_frame_navigation_confidence = {"low", "high"};
    expected_iframe_navigation_confidence = {"null", "null"};
  }

  CheckActivePageNavigationConfidence(
      expected_main_frame_navigation_confidence[0]);
  CheckActivePageFrameNavigationConfidence(
      expected_iframe_navigation_confidence[0]);

  Navigate("/page_with_iframe_and_image.html");
  CheckActivePageNavigationConfidence(
      expected_main_frame_navigation_confidence[1]);
  CheckActivePageFrameNavigationConfidence(
      expected_iframe_navigation_confidence[1]);
}

IN_PROC_BROWSER_TEST_P(LaunchNavigationBrowserWithIFrameTest,
                       CreateEmptyFrame) {
  Navigate("/launch_navigation_frame.html");
  CheckActivePageNavigationConfidence(
      IsPerformanceNavigationTimingConfidenceEnabled() ? "high" : "undefined");
  CheckActivePageFrameNavigationConfidence(
      IsPerformanceNavigationTimingConfidenceEnabled() ? "null" : "undefined");
}

INSTANTIATE_TEST_SUITE_P(
    CmdLineURLIFrameTestFeatureEnabled,
    LaunchNavigationBrowserWithIFrameTest,
    testing::Values(LaunchNavigationBrowserTestParam(/*enable_feature=*/true,
                                                     StartupPrefs(),
                                                     {url3})));

INSTANTIATE_TEST_SUITE_P(
    CmdLineURLIFrameTestFeatureDisabled,
    LaunchNavigationBrowserWithIFrameTest,
    testing::Values(LaunchNavigationBrowserTestParam(/*enable_feature=*/false,
                                                     StartupPrefs(),
                                                     {url3})));

class LaunchNavigationBrowserWithPopupTest
    : public LaunchNavigationBrowserTest {
 protected:
  content::WebContents* OpenPopupFromActiveWebContents() const {
    auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
    content::ExecuteScriptAsync(
        contents, "w = open('about:blank', '', 'width=200,height=200');");
    Browser* popup = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_NE(popup, browser());
    auto* popup_contents = popup->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(WaitForLoadStop(popup_contents));
    EXPECT_TRUE(WaitForRenderFrameReady(popup_contents->GetPrimaryMainFrame()));
    return popup_contents;
  }

 private:
  void SetLaunchUrls(base::CommandLine* command_line) override {
    if (!GetParam().cmd_line_urls.empty()) {
      AppendUrlsToCmdLine(command_line, embedded_test_server(),
                          GetParam().cmd_line_urls);
    }
  }
};

IN_PROC_BROWSER_TEST_P(LaunchNavigationBrowserWithPopupTest,
                       CmdLineLaunchWithPopup) {
  std::vector<std::string> expected_main_frame_navigation_confidence = {
      "undefined", "undefined"};
  std::vector<std::string> expected_popup_navigation_confidence = {"undefined",
                                                                   "undefined"};

  if (IsPerformanceNavigationTimingConfidenceEnabled()) {
    expected_main_frame_navigation_confidence = {"low", "high"};
    expected_popup_navigation_confidence = {"high", "high"};
  }

  {
    auto* popup_contents = OpenPopupFromActiveWebContents();
    CheckActivePageNavigationConfidence(
        expected_main_frame_navigation_confidence[0]);
    CheckPageNavigationConfidenceForWebContents(
        popup_contents, expected_popup_navigation_confidence[0]);
  }

  {
    Navigate(url2);
    auto* popup_contents = OpenPopupFromActiveWebContents();
    CheckActivePageNavigationConfidence(
        expected_main_frame_navigation_confidence[1]);
    CheckPageNavigationConfidenceForWebContents(
        popup_contents, expected_popup_navigation_confidence[1]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    CmdLineURLPopupTestFeatureEnabled,
    LaunchNavigationBrowserWithPopupTest,
    testing::Values(LaunchNavigationBrowserTestParam(/*enable_feature=*/true,
                                                     StartupPrefs(),
                                                     {url1})));

INSTANTIATE_TEST_SUITE_P(
    CmdLineURLPopupTestFeatureDisabled,
    LaunchNavigationBrowserWithPopupTest,
    testing::Values(LaunchNavigationBrowserTestParam(/*enable_feature=*/false,
                                                     StartupPrefs(),
                                                     {url1})));
