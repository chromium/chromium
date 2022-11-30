// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/contains.h"
#include "build/build_config.h"
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
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

using ::testing::Contains;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::SizeIs;

namespace {
// Must match message in
// chrome/browser/resources/webui_js_error/webui_js_error.js, but with URL
// escapes.
constexpr char kPageLoadMessage[] =
    "WebUI%20JS%20Error%3A%20printing%20error%20on%20page%20load";

// A simple webpage that generates a JavaScript error on load.
constexpr char kJavaScriptErrorPage[] = R"(
<html>
  <head>
    <meta charset="utf-8">
    <title>Bad Page</title>
  </head>
  <body>
    Text
    <script>
      console.error('special error message for WebUIJSErrorReportingTest');
    </script>
  </body>
</html>
)";

// The error message printed by kJavaScriptErrorPage
constexpr char kWebpageErrorMessage[] =
    "special error message for WebUIJSErrorReportingTest";

// Callback for the error_page_test_server_. Tells the server to always return
// the contents of kJavaScriptErrorPage.
std::unique_ptr<net::test_server::HttpResponse> ReturnErrorPage(
    const net::test_server::HttpRequest&) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(kJavaScriptErrorPage);
  http_response->set_content_type("text/html");
  return http_response;
}

// A class that waits for a log message like
//  [4193947:4193947:0108/114152.942981:INFO:CONSOLE(10)] "special error message
//  for WebUIJSErrorReportingTest", source: http://127.0.0.1:36521/index.html
//  (10)
// to appear and then calls a callback (usually a RunLoop quit closure)
class ScopedLogMessageWatcher {
 public:
  explicit ScopedLogMessageWatcher(base::RepeatingClosure callback)
      : callback_(std::move(callback)) {
    previous_handler_ = logging::GetLogMessageHandler();
    // base::LogMessageHandlerFunction must be a pure function, not a functor,
    // so we need a global to find this object again.
    CHECK(current_handler_ == nullptr);
    current_handler_ = this;
    logging::SetLogMessageHandler(&ScopedLogMessageWatcher::MessageHandler);
  }
  ScopedLogMessageWatcher(const ScopedLogMessageWatcher&) = delete;
  ScopedLogMessageWatcher& operator=(const ScopedLogMessageWatcher&) = delete;
  ~ScopedLogMessageWatcher() {
    CHECK(current_handler_ == this);
    current_handler_ = nullptr;
    logging::SetLogMessageHandler(previous_handler_);
  }

 private:
  static bool MessageHandler(int severity,
                             const char* file,
                             int line,
                             size_t message_start,
                             const std::string& str) {
    CHECK(current_handler_ != nullptr);
    if (base::Contains(str, kWebpageErrorMessage)) {
      current_handler_->callback_.Run();
    }
    if (current_handler_->previous_handler_ != nullptr) {
      return (*current_handler_->previous_handler_)(severity, file, line,
                                                    message_start, str);
    }
    return false;
  }
  static ScopedLogMessageWatcher* current_handler_;
  base::RepeatingClosure callback_;
  logging::LogMessageHandlerFunction previous_handler_;
};
ScopedLogMessageWatcher* ScopedLogMessageWatcher::current_handler_ = nullptr;
}  // namespace

class WebUIJSErrorReportingTest : public InProcessBrowserTest {
 public:
  WebUIJSErrorReportingTest() : error_url_(chrome::kChromeUIWebUIJsErrorURL) {
    CHECK(error_url_.is_valid());
  }

  void SetUpOnMainThread() override {
    error_page_test_server_.RegisterRequestHandler(
        base::BindRepeating(&ReturnErrorPage));
    EXPECT_TRUE(error_page_test_server_.Start());

    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  // NoErrorsAfterNavigation needs a second embedded test server to serve up
  // its error page, since embedded_test_server() is in use by the
  // MockCrashEndpoint.
  net::test_server::EmbeddedTestServer error_page_test_server_;
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
  EXPECT_THAT(endpoint.all_reports(), SizeIs(1));
  EXPECT_THAT(report.query, HasSubstr(kPageLoadMessage)) << report;
  // Expect that we get a good stack trace as well
  EXPECT_THAT(report.content, AllOf(HasSubstr("logsErrorDuringPageLoadOuter"),
                                    HasSubstr("logsErrorDuringPageLoadInner")))
      << report;

  endpoint.clear_last_report();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Trigger uncaught exception. Simulating mouse clicks on a button requires
  // there to not be CSP on the JavaScript, so use accesskeys instead.
  content::SimulateKeyPress(web_contents, ui::DomKey::NONE, ui::DomCode::US_T,
                            ui::VKEY_T, /*control=*/false, /*shift=*/false,
                            /*alt=*/true, /*command=*/false);
  report = endpoint.WaitForReport();
  EXPECT_THAT(endpoint.all_reports(), SizeIs(2));
  constexpr char kExceptionButtonMessage[] =
      "WebUI%20JS%20Error%3A%20exception%20button%20clicked";
  EXPECT_THAT(report.query, HasSubstr(kExceptionButtonMessage)) << report;
  EXPECT_THAT(report.content, AllOf(HasSubstr("throwExceptionHandler"),
                                    HasSubstr("throwExceptionInner")))
      << report;

  endpoint.clear_last_report();
  // Trigger console.error call.
  content::SimulateKeyPress(web_contents, ui::DomKey::NONE, ui::DomCode::US_L,
                            ui::VKEY_L, /*control=*/false, /*shift=*/false,
                            /*alt=*/true, /*command=*/false);
  report = endpoint.WaitForReport();
  EXPECT_THAT(endpoint.all_reports(), SizeIs(3));
  constexpr char kTriggeredErrorMessage[] =
      "WebUI%20JS%20Error%3A%20printing%20error%20on%20button%20click";
  EXPECT_THAT(report.query, HasSubstr(kTriggeredErrorMessage)) << report;
  EXPECT_THAT(report.content,
              AllOf(HasSubstr("logsErrorFromButtonClickHandler"),
                    HasSubstr("logsErrorFromButtonClickInner")))
      << report;

  endpoint.clear_last_report();
  // Trigger unhandled promise rejection.
  content::SimulateKeyPress(web_contents, ui::DomKey::NONE, ui::DomCode::US_P,
                            ui::VKEY_P, /*control=*/false, /*shift=*/false,
                            /*alt=*/true, /*command=*/false);
  report = endpoint.WaitForReport();
  EXPECT_THAT(endpoint.all_reports(), SizeIs(4));
  constexpr char kUnhandledPromiseRejectionMessage[] =
      "WebUI%20JS%20Error%3A%20The%20rejector%20always%20rejects!";
  EXPECT_THAT(report.query, HasSubstr(kUnhandledPromiseRejectionMessage))
      << report;
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), error_url_));
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
    SessionServiceTestHelper helper(profile);
    helper.SetForceBrowserNotAliveWithNoWindows(true);
    chrome::NewEmptyWindow(profile);

    // ScopedKeepAlive goes out of scope, so the new browser will return to
    // normal behavior.
  }

  MockCrashEndpoint::Report report = endpoint.WaitForReport();

  EXPECT_THAT(endpoint.all_reports(), SizeIs(2));
  EXPECT_THAT(report.query, HasSubstr(kPageLoadMessage)) << report;
}

// Show that navigating from a WebUI page to a http page that produces
// JavaScript errors on load does not create an error report.
IN_PROC_BROWSER_TEST_F(WebUIJSErrorReportingTest, NoErrorsAfterNavigation) {
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor mock_processor(endpoint);

  NavigateParams navigate(browser(), error_url_, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&navigate);

  // Wait for page load error report.
  MockCrashEndpoint::Report report = endpoint.WaitForReport();
  EXPECT_THAT(endpoint.all_reports(), SizeIs(1));
  EXPECT_EQ(mock_processor.processor().send_count(), 1);

  {
    base::RunLoop run_loop;
    ScopedLogMessageWatcher log_watcher(run_loop.QuitClosure());

    NavigateParams navigate_to_http(
        browser(), error_page_test_server_.GetURL("/index.html"),
        ui::PAGE_TRANSITION_TYPED);
    ui_test_utils::NavigateToURL(&navigate_to_http);

    run_loop.Run();  // Run until the error message is seen on the console.
  }

  // Now run more to make sure the error reporter system doesn't have an
  // in-flight error report.
  {
    base::RunLoop run_loop2;
    run_loop2.RunUntilIdle();
  }

  // Count should not change.
  EXPECT_THAT(endpoint.all_reports(), SizeIs(1));
  EXPECT_EQ(mock_processor.processor().send_count(), 1);
}

// Test that using the real variation::GetExperimentListString() system works.
// We don't know the list of experiments we are in, so we don't know precisely
// what to expect, but we shouldn't fail to send.
IN_PROC_BROWSER_TEST_F(WebUIJSErrorReportingTest, ExperimentListSmokeTest) {
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor mock_processor(endpoint);
  mock_processor.processor().set_use_real_experiment_list();

  NavigateParams navigate(browser(), error_url_, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&navigate);

  MockCrashEndpoint::Report report = endpoint.WaitForReport();
  EXPECT_THAT(report.query, HasSubstr("num-experiments=")) << report;
  EXPECT_THAT(report.query, HasSubstr("variations=")) << report;
}
