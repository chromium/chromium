// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/error_reporting/mock_chrome_js_error_report_processor.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

using ::testing::HasSubstr;

namespace {
// Must match message in
// chrome/browser/resources/webui_js_error/webui_js_error.js, but with URL
// escapes.
constexpr char kPageLoadMessage[] =
    "WebUI%20JS%20Error%3A%20printing%20error%20on%20page%20load";
}  // namespace

class WebUIJSErrorReportingTest : public InProcessBrowserTest {
 public:
  WebUIJSErrorReportingTest() : error_url_(chrome::kChromeUIWebUIJsErrorURL) {
    CHECK(error_url_.is_valid());
  }

  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSendWebUIJavaScriptErrorReports,
        {{features::kSendWebUIJavaScriptErrorReportsSendToProductionVariation,
          "false"}});
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  const GURL error_url_;
};

IN_PROC_BROWSER_TEST_F(WebUIJSErrorReportingTest, ReportsErrors) {
  // mock_processor must be after BrowserProcessImpl::PreMainMessageLoopRun, so
  // it can't be created in SetUp or SetUpInProcessBrowserTestFixture.
  // Similarly, MockCrashEndpoint must be in the test function so that its
  // MockCrashEndpoint::Client is not replaced by other crash clients.
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor mock_processor(endpoint);

  NavigateParams navigate(browser(), error_url_, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&navigate);

  // Look for page load error report.
  MockCrashEndpoint::Report report = endpoint.WaitForReport();
  EXPECT_EQ(endpoint.report_count(), 1);
  EXPECT_THAT(report.query, HasSubstr(kPageLoadMessage));
  // Expect that we get a good stack trace as well
  EXPECT_THAT(report.content, AllOf(HasSubstr("logsErrorDuringPageLoadOuter"),
                                    HasSubstr("logsErrorDuringPageLoadInner")));

  endpoint.clear_last_report();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Trigger uncaught exception. Simulating mouse clicks on a button requires
  // there to not be CSP on the JavaScript, so use accesskeys instead.
  content::SimulateKeyPress(web_contents, ui::DomKey::NONE, ui::DomCode::US_T,
                            ui::VKEY_T, /*control=*/false, /*shift=*/false,
                            /*alt=*/true, /*command=*/false);
  report = endpoint.WaitForReport();
  EXPECT_EQ(endpoint.report_count(), 2);
  constexpr char kExceptionButtonMessage[] =
      "WebUI%20JS%20Error%3A%20exception%20button%20clicked";
  EXPECT_THAT(report.query, HasSubstr(kExceptionButtonMessage));
  EXPECT_THAT(report.content, AllOf(HasSubstr("throwExceptionHandler"),
                                    HasSubstr("throwExceptionInner")));

  endpoint.clear_last_report();
  // Trigger console.error call.
  content::SimulateKeyPress(web_contents, ui::DomKey::NONE, ui::DomCode::US_L,
                            ui::VKEY_L, /*control=*/false, /*shift=*/false,
                            /*alt=*/true, /*command=*/false);
  report = endpoint.WaitForReport();
  EXPECT_EQ(endpoint.report_count(), 3);
  constexpr char kTriggeredErrorMessage[] =
      "WebUI%20JS%20Error%3A%20printing%20error%20on%20button%20click";
  EXPECT_THAT(report.query, HasSubstr(kTriggeredErrorMessage));
  EXPECT_THAT(report.content,
              AllOf(HasSubstr("logsErrorFromButtonClickHandler"),
                    HasSubstr("logsErrorFromButtonClickInner")));

  endpoint.clear_last_report();
  // Trigger unhandled promise rejection.
  content::SimulateKeyPress(web_contents, ui::DomKey::NONE, ui::DomCode::US_P,
                            ui::VKEY_P, /*control=*/false, /*shift=*/false,
                            /*alt=*/true, /*command=*/false);
  report = endpoint.WaitForReport();
  EXPECT_EQ(endpoint.report_count(), 4);
  constexpr char kUnhandledPromiseRejectionMessage[] =
      "WebUI%20JS%20Error%3A%20The%20rejector%20always%20rejects!";
  EXPECT_THAT(report.query, HasSubstr(kUnhandledPromiseRejectionMessage));
  // V8 doesn't produce stacks for unhandle promise rejections.
}

// Set up a profile with "Continue where you left off". Navigate to the JS error
// page. Ensure that when the browser is closed and reopened, on-page-load
// errors are still reported.
IN_PROC_BROWSER_TEST_F(WebUIJSErrorReportingTest,
                       ReportsErrorsDuringContinueWhereYouLeftOff) {
  MockCrashEndpoint endpoint(embedded_test_server());
  auto mock_processor =
      std::make_unique<ScopedMockChromeJsErrorReportProcessor>(endpoint);

  Profile* profile = browser()->profile();
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile, pref);
  profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), error_url_);
  endpoint.WaitForReport();
  endpoint.clear_last_report();

  // Restart browser. Note: We can't do the normal PRE_Name / Name browsertest
  // pattern here because the Continue Where You Left Off pages are loaded
  // before the test starts, so we don't have a chance to set up the mock
  // error processor.
  {
    ScopedKeepAlive keep_alive(KeepAliveOrigin::SESSION_RESTORE,
                               KeepAliveRestartOption::DISABLED);
    CloseBrowserSynchronously(browser());

    // Create a new error processor to reset the list of already seen reports,
    // otherwise the report gets thrown away as a duplicate.
    mock_processor.reset();
    mock_processor =
        std::make_unique<ScopedMockChromeJsErrorReportProcessor>(endpoint);
    SessionServiceTestHelper helper(
        SessionServiceFactory::GetForProfileForSessionRestore(profile));
    helper.SetForceBrowserNotAliveWithNoWindows(true);
    helper.ReleaseService();
    chrome::NewEmptyWindow(profile);

    // ScopedKeepAlive goes out of scope, so the new browser will return to
    // normal behavior.
  }

  MockCrashEndpoint::Report report = endpoint.WaitForReport();
  EXPECT_EQ(endpoint.report_count(), 2);
  EXPECT_THAT(report.query, HasSubstr(kPageLoadMessage));
}
