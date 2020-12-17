// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/error_reporting/mock_chrome_js_error_report_processor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"
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

class WebUIJSErrorReportingTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSendWebUIJavaScriptErrorReports,
        {{features::kSendWebUIJavaScriptErrorReportsSendToProductionVariation,
          "true"}});
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIJSErrorReportingTest, ReportsErrors) {
  // mock_processor must be after BrowserProcessImpl::PreMainMessageLoopRun, so
  // it can't be created in SetUp or SetUpInProcessBrowserTestFixture.
  // Similarly, MockCrashEndpoint must be in the test function so that its
  // MockCrashEndpoint::Client is not replaced by other crash clients.
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor mock_processor(endpoint);

  GURL url(chrome::kChromeUIWebUIJsErrorURL);
  ASSERT_TRUE(url.is_valid());
  NavigateParams navigate(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&navigate);

  // Look for page load error report.
  MockCrashEndpoint::Report report = endpoint.WaitForReport();
  EXPECT_EQ(endpoint.report_count(), 1);
  // Must match message in
  // chrome/browser/resources/webui_js_error/webui_js_error.js, but with URL
  // escapes.
  constexpr char kPageLoadMessage[] =
      "WebUI%20JS%20Error%3A%20printing%20error%20on%20page%20load";
  EXPECT_THAT(report.query, HasSubstr(kPageLoadMessage));
  // TODO(iby): Check stack once stack tracing is working.

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
}
