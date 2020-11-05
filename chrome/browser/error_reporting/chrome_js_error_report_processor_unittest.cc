// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/error_reporting/chrome_js_error_report_processor.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/error_reporting/mock_chrome_js_error_report_processor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::HasSubstr;

class ChromeJsErrorReportProcessorTest : public ::testing::Test {
 public:
  ChromeJsErrorReportProcessorTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        run_loop_quit_(run_loop_.QuitClosure()),
        processor_(base::MakeRefCounted<MockChromeJsErrorReportProcessor>()) {}

  void SetUp() override {
    test_server_ = std::make_unique<net::test_server::EmbeddedTestServer>();
    endpoint_ = std::make_unique<MockCrashEndpoint>(test_server_.get());
    processor_->SetCrashEndpoint(endpoint_->GetCrashEndpointURL());
  }

  void FinishCallback() {
    finish_callback_was_called_ = true;
    run_loop_quit_.Run();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  base::RepeatingClosure run_loop_quit_;
  TestingProfile browser_context_;
  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
  std::unique_ptr<MockCrashEndpoint> endpoint_;
  bool finish_callback_was_called_ = false;
  scoped_refptr<MockChromeJsErrorReportProcessor> processor_;
};

TEST_F(ChromeJsErrorReportProcessorTest, Basic) {
  JavaScriptErrorReport report;
  report.message = "Hello World";
  report.url = "https://www.chromium.org/Home";

  processor_->SendErrorReport(
      std::move(report),
      base::BindOnce(&ChromeJsErrorReportProcessorTest::FinishCallback,
                     base::Unretained(this)),
      &browser_context_);
  run_loop_.Run();
  EXPECT_TRUE(finish_callback_was_called_);

  const base::Optional<MockCrashEndpoint::Report>& actual_report =
      endpoint_->last_report();
  ASSERT_TRUE(actual_report);
  EXPECT_THAT(actual_report->query, HasSubstr("error_message=Hello%20World"));
  EXPECT_THAT(actual_report->query, HasSubstr("type=JavascriptError"));
  EXPECT_THAT(actual_report->query, HasSubstr("browser_process_uptime_ms="));
  EXPECT_THAT(actual_report->query, HasSubstr("renderer_process_uptime_ms=0"));
  // TODO(iby) research why URL is repeated...
  EXPECT_THAT(actual_report->query,
              HasSubstr("src=https%3A%2F%2Fwww.chromium.org%2FHome"));
  EXPECT_THAT(actual_report->query,
              HasSubstr("full_url=https%3A%2F%2Fwww.chromium.org%2FHome"));
  EXPECT_THAT(actual_report->query, HasSubstr("url=%2FHome"));
  // This is from MockChromeJsErrorReportProcessor::GetOsVersion()
  EXPECT_THAT(actual_report->query, HasSubstr("os_version=7.20.1"));
  EXPECT_THAT(actual_report->query, HasSubstr("browser=Chrome"));
  // These are from MockCrashEndpoint::Client::GetProductNameAndVersion, which
  // is only defined for non-MAC POSIX systems. TODO(https://crbug.com/1121816):
  // Get this info for non-POSIX platforms.
#if defined(OS_POSIX) && !defined(OS_MAC)
  EXPECT_THAT(actual_report->query, HasSubstr("prod=Chrome_ChromeOS"));
  EXPECT_THAT(actual_report->query, HasSubstr("ver=1.2.3.4"));
  EXPECT_THAT(actual_report->query, HasSubstr("browser_version=1.2.3.4"));
  EXPECT_THAT(actual_report->query, HasSubstr("channel=Stable"));
#endif
  EXPECT_EQ(actual_report->content, "");
}

TEST_F(ChromeJsErrorReportProcessorTest, AllFields) {
  JavaScriptErrorReport report;
  report.message = "Hello World";
  report.url = "https://www.chromium.org/Home";
  report.product = "Unit test";
  report.version = "6.2.3.4";
  report.line_number = 83;
  report.column_number = 14;
  report.stack_trace = "bad_func(1, 2)\nonclick()\n";
  report.renderer_process_uptime_ms = 1234;
  report.window_type = WindowType::kSystemWebApp;

  processor_->SendErrorReport(
      std::move(report),
      base::BindOnce(&ChromeJsErrorReportProcessorTest::FinishCallback,
                     base::Unretained(this)),
      &browser_context_);
  run_loop_.Run();
  EXPECT_TRUE(finish_callback_was_called_);

  const base::Optional<MockCrashEndpoint::Report>& actual_report =
      endpoint_->last_report();
  ASSERT_TRUE(actual_report);
  EXPECT_THAT(actual_report->query, HasSubstr("error_message=Hello%20World"));
  EXPECT_THAT(actual_report->query, HasSubstr("type=JavascriptError"));
  EXPECT_THAT(actual_report->query, HasSubstr("browser_process_uptime_ms="));
  EXPECT_THAT(actual_report->query,
              HasSubstr("renderer_process_uptime_ms=1234"));
  EXPECT_THAT(actual_report->query, HasSubstr("window_type=SYSTEM_WEB_APP"));
  // TODO(iby) research why URL is repeated...
  EXPECT_THAT(actual_report->query,
              HasSubstr("src=https%3A%2F%2Fwww.chromium.org%2FHome"));
  EXPECT_THAT(actual_report->query,
              HasSubstr("full_url=https%3A%2F%2Fwww.chromium.org%2FHome"));
  EXPECT_THAT(actual_report->query, HasSubstr("url=%2FHome"));
  // This is from MockChromeJsErrorReportProcessor::GetOsVersion()
  EXPECT_THAT(actual_report->query, HasSubstr("os_version=7.20.1"));
  EXPECT_THAT(actual_report->query, HasSubstr("browser=Chrome"));
  // product is double-escaped. The first time, it transforms to Unit%20test,
  // then the % is turned into %25.
  EXPECT_THAT(actual_report->query, HasSubstr("prod=Unit%2520test"));
  EXPECT_THAT(actual_report->query, HasSubstr("ver=6.2.3.4"));
  EXPECT_THAT(actual_report->query, HasSubstr("line=83"));
  EXPECT_THAT(actual_report->query, HasSubstr("column=14"));
  // These are from MockCrashEndpoint::Client::GetProductNameAndVersion, which
  // is only defined for non-MAC POSIX systems. TODO(https://crbug.com/1121816):
  // Get this info for non-POSIX platforms.
#if defined(OS_POSIX) && !defined(OS_MAC)
  EXPECT_THAT(actual_report->query, HasSubstr("browser_version=1.2.3.4"));
  EXPECT_THAT(actual_report->query, HasSubstr("channel=Stable"));
#endif
  EXPECT_EQ(actual_report->content, "bad_func(1, 2)\nonclick()\n");
}

TEST_F(ChromeJsErrorReportProcessorTest, NoConsent) {
  endpoint_->set_consented(false);
  JavaScriptErrorReport report;
  report.message = "Hello World";
  report.url = "https://www.chromium.org/Home";

  processor_->SendErrorReport(
      std::move(report),
      base::BindOnce(&ChromeJsErrorReportProcessorTest::FinishCallback,
                     base::Unretained(this)),
      &browser_context_);
  run_loop_.Run();
  EXPECT_TRUE(finish_callback_was_called_);

  EXPECT_FALSE(endpoint_->last_report());
}

TEST_F(ChromeJsErrorReportProcessorTest, StackTraceWithErrorMessage) {
  JavaScriptErrorReport report;
  report.message = "Hello World";
  report.url = "https://www.chromium.org/Home";
  report.stack_trace = "Hello World\nbad_func(1, 2)\nonclick()\n";

  processor_->SendErrorReport(
      std::move(report),
      base::BindOnce(&ChromeJsErrorReportProcessorTest::FinishCallback,
                     base::Unretained(this)),
      &browser_context_);
  run_loop_.Run();
  EXPECT_TRUE(finish_callback_was_called_);

  const base::Optional<MockCrashEndpoint::Report>& actual_report =
      endpoint_->last_report();
  ASSERT_TRUE(actual_report);
  EXPECT_THAT(actual_report->query, HasSubstr("error_message=Hello%20World"));
  EXPECT_EQ(actual_report->content, "bad_func(1, 2)\nonclick()\n");
}

TEST_F(ChromeJsErrorReportProcessorTest, RedactMessage) {
  JavaScriptErrorReport report;
  report.message = "alpha@beta.org says hi to gamma@omega.co.uk";
  report.url = "https://www.chromium.org/Home";
  report.stack_trace =
      "alpha@beta.org says hi to gamma@omega.co.uk\n"
      "bad_func(1, 2)\nonclick()\n";

  processor_->SendErrorReport(
      std::move(report),
      base::BindOnce(&ChromeJsErrorReportProcessorTest::FinishCallback,
                     base::Unretained(this)),
      &browser_context_);
  run_loop_.Run();
  EXPECT_TRUE(finish_callback_was_called_);

  const base::Optional<MockCrashEndpoint::Report>& actual_report =
      endpoint_->last_report();
  ASSERT_TRUE(actual_report);
  // Escaped version of "<email: 1> says hi to <email: 2>"
  EXPECT_THAT(actual_report->query,
              HasSubstr("error_message=%3Cemail%3A%201%3E%20says%20hi%20to%20"
                        "%3Cemail%3A%202%3E"));
  // Redacted messages still need to be removed from stack trace.
  EXPECT_EQ(actual_report->content, "bad_func(1, 2)\nonclick()\n");
}
