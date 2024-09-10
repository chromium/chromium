// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/error_reporting/chrome_js_error_report_processor.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/crash_upload_list/crash_upload_list.h"
#include "chrome/browser/error_reporting/mock_chrome_js_error_report_processor.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"
#include "components/crash/core/app/crashpad.h"
#include "components/upload_list/upload_list.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/text/bytes_formatting.h"

using ::testing::AllOf;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::SizeIs;
using ::testing::StartsWith;

JavaScriptErrorReport MakeErrorReport(const std::string& message) {
  JavaScriptErrorReport report;
  report.message = message;
  return report;
}

std::string JoinStringPairs(const base::StringPairs& pairs) {
  std::string result;
  bool first = true;
  for (const auto& pair : pairs) {
    if (first) {
      first = false;
    } else {
      result += "; ";
    }

    base::StrAppend(&result, {"(", pair.first, ",", pair.second, ")"});
  }
  return result;
}

class ChromeJsErrorReportProcessorTest : public ::testing::Test {
 public:
  ChromeJsErrorReportProcessorTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        processor_(base::MakeRefCounted<MockChromeJsErrorReportProcessor>()) {}

  void SetUp() override {
    // Set clock to something arbitrary which is not the null value.
    test_clock_.SetNow(base::Time::FromTimeT(kFakeNow));
    test_server_ = std::make_unique<net::test_server::EmbeddedTestServer>();
    endpoint_ = std::make_unique<MockCrashEndpoint>(test_server_.get());
    processor_->SetCrashEndpoint(endpoint_->GetCrashEndpointURL());
    processor_->set_clock_for_testing(&test_clock_);
  }

  void FinishCallback(base::RepeatingClosure run_loop_quit) {
    // Callback should always be on the originating thread.
    CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    finish_callback_was_called_ = true;
    run_loop_quit.Run();
  }

  // Wrapper around processor_->SendErrorReport. Runs a RunLoop until the
  // callback is called.
  void SendErrorReport(JavaScriptErrorReport report) {
    base::RunLoop run_loop;
    processor_->SendErrorReport(
        std::move(report),
        base::BindOnce(&ChromeJsErrorReportProcessorTest::FinishCallback,
                       base::Unretained(this), run_loop.QuitClosure()),
        &browser_context_);
    run_loop.Run();
  }

  // Helper for TEST_F(ChromeJsErrorReportProcessorTest, AllFields) and
  // TEST_F(ChromeJsErrorReportProcessorTest, WorksWithoutMemfdCreate).
  void TestAllFields();

 protected:
  base::SimpleTestClock test_clock_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile browser_context_;
  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
  std::unique_ptr<MockCrashEndpoint> endpoint_;
  bool finish_callback_was_called_ = false;
  scoped_refptr<MockChromeJsErrorReportProcessor> processor_;

  static constexpr time_t kFakeNow = 1586581472;
  static constexpr char kFirstMessage[] = "An Error Is Me";
  static constexpr char kFirstMessageQuery[] =
      "error_message=An%20Error%20Is%20Me";
  static constexpr char kSecondMessage[] = "A bad error";
  static constexpr char kSecondMessageQuery[] = "error_message=A%20bad%20error";
  static constexpr char kThirdMessage[] = "Wow that's a lot of errors";
  static constexpr char kThirdMessageQuery[] =
      "error_message=Wow%20that%27s%20a%20lot%20of%20errors";
  static constexpr char kProduct[] = "Chrome_ChromeOS";
  static constexpr char kSecondProduct[] = "Chrome_Linux";
};

constexpr time_t ChromeJsErrorReportProcessorTest::kFakeNow;
constexpr char ChromeJsErrorReportProcessorTest::kFirstMessage[];
constexpr char ChromeJsErrorReportProcessorTest::kFirstMessageQuery[];
constexpr char ChromeJsErrorReportProcessorTest::kSecondMessage[];
constexpr char ChromeJsErrorReportProcessorTest::kSecondMessageQuery[];
constexpr char ChromeJsErrorReportProcessorTest::kThirdMessage[];
constexpr char ChromeJsErrorReportProcessorTest::kThirdMessageQuery[];
constexpr char ChromeJsErrorReportProcessorTest::kProduct[];
constexpr char ChromeJsErrorReportProcessorTest::kSecondProduct[];

TEST_F(ChromeJsErrorReportProcessorTest, Basic) {
  auto report = MakeErrorReport("Hello World");
  report.url = "https://www.chromium.org/Home";

  SendErrorReport(std::move(report));
  EXPECT_TRUE(finish_callback_was_called_);

  const std::optional<MockCrashEndpoint::Report>& actual_report =
      endpoint_->last_report();
  ASSERT_TRUE(actual_report);
  EXPECT_THAT(actual_report->query, HasSubstr("error_message=Hello%20World"));
  EXPECT_THAT(actual_report->query, HasSubstr("type=JavascriptError"));
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_THAT(actual_report->query, HasSubstr("build_time_millis="));
#endif
  EXPECT_THAT(actual_report->query, HasSubstr("browser_process_uptime_ms="));
  EXPECT_THAT(actual_report->query, HasSubstr("renderer_process_uptime_ms=0"));
  // TODO(iby) research why URL is repeated...
  EXPECT_THAT(actual_report->query,
              HasSubstr("src=https%3A%2F%2Fwww.chromium.org%2FHome"));
  EXPECT_THAT(actual_report->query,
              HasSubstr("full_url=https%3A%2F%2Fwww.chromium.org%2FHome"));
  EXPECT_THAT(actual_report->query, HasSubstr("url=%2FHome"));
  EXPECT_THAT(actual_report->query, HasSubstr("browser=Chrome"));
  EXPECT_THAT(actual_report->query, Not(HasSubstr("source_system=")));
  EXPECT_THAT(actual_report->query, HasSubstr("num-experiments=1"));
  EXPECT_THAT(
      actual_report->query,
      HasSubstr(base::StrCat(
          {"variations=",
           MockChromeJsErrorReportProcessor::kDefaultExperimentListString})));

#if !BUILDFLAG(IS_CHROMEOS)
  // This is from MockChromeJsErrorReportProcessor::GetOsVersion()
  EXPECT_THAT(actual_report->query, HasSubstr("os_version=7.20.1"));
#endif
  // These are from MockCrashEndpoint::Client::GetProductNameAndVersion, which
  // is only defined for non-MAC POSIX systems. TODO(crbug.com/40146362):
  // Get this info for non-POSIX platforms.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  EXPECT_THAT(actual_report->query, HasSubstr("prod=Chrome_ChromeOS"));
  EXPECT_THAT(actual_report->query, HasSubstr("ver=1.2.3.4"));
  EXPECT_THAT(actual_report->query, HasSubstr("browser_version=1.2.3.4"));
  EXPECT_THAT(actual_report->query, HasSubstr("channel=Stable"));
#endif
  EXPECT_EQ(actual_report->content, "");
}

void ChromeJsErrorReportProcessorTest::TestAllFields() {
  auto report = MakeErrorReport("Hello World");
  report.url = "https://www.chromium.org/Home/scripts.js";
  report.product = "Unit test";
  report.version = "6.2.3.4";
  report.line_number = 83;
  report.column_number = 14;
  report.page_url = "https://www.chromium.org/Home.html";
  report.debug_id = "ABC:123";
  report.stack_trace = "bad_func(1, 2)\nonclick()\n";
  report.renderer_process_uptime_ms = 1234;
  report.window_type = JavaScriptErrorReport::WindowType::kSystemWebApp;
  report.source_system = JavaScriptErrorReport::SourceSystem::kWebUIObserver;

  SendErrorReport(std::move(report));
  EXPECT_TRUE(finish_callback_was_called_);

  const std::optional<MockCrashEndpoint::Report>& actual_report =
      endpoint_->last_report();
  ASSERT_TRUE(actual_report);
  EXPECT_THAT(actual_report->query, HasSubstr("error_message=Hello%20World"));
  EXPECT_THAT(actual_report->query, HasSubstr("type=JavascriptError"));
  EXPECT_THAT(actual_report->query, HasSubstr("browser_process_uptime_ms="));
  EXPECT_THAT(actual_report->query,
              HasSubstr("renderer_process_uptime_ms=1234"));
  EXPECT_THAT(actual_report->query, HasSubstr("window_type=SYSTEM_WEB_APP"));
  EXPECT_THAT(actual_report->query, HasSubstr("debug_id=ABC%3A123"));
  // TODO(iby) research why URL is repeated...
  EXPECT_THAT(
      actual_report->query,
      HasSubstr("src=https%3A%2F%2Fwww.chromium.org%2FHome%2Fscripts.js"));
  EXPECT_THAT(
      actual_report->query,
      HasSubstr("full_url=https%3A%2F%2Fwww.chromium.org%2FHome%2Fscripts.js"));
  EXPECT_THAT(actual_report->query, HasSubstr("url=%2FHome%2Fscripts.js"));
  EXPECT_THAT(actual_report->query,
              HasSubstr("page_url=https%3A%2F%2Fwww.chromium.org%2FHome.html"));
  EXPECT_THAT(actual_report->query, HasSubstr("browser=Chrome"));
  // product is double-escaped. The first time, it transforms to Unit%20test,
  // then the % is turned into %25.
  EXPECT_THAT(actual_report->query, HasSubstr("prod=Unit%2520test"));
  EXPECT_THAT(actual_report->query, HasSubstr("ver=6.2.3.4"));
  EXPECT_THAT(actual_report->query, HasSubstr("line=83"));
  EXPECT_THAT(actual_report->query, HasSubstr("column=14"));
  EXPECT_THAT(actual_report->query, HasSubstr("source_system=webui_observer"));
  EXPECT_THAT(actual_report->query, HasSubstr("num-experiments=1"));
  EXPECT_THAT(
      actual_report->query,
      HasSubstr(base::StrCat(
          {"variations=",
           MockChromeJsErrorReportProcessor::kDefaultExperimentListString})));

#if !BUILDFLAG(IS_CHROMEOS)
  // This is from MockChromeJsErrorReportProcessor::GetOsVersion()
  EXPECT_THAT(actual_report->query, HasSubstr("os_version=7.20.1"));
#endif
  // These are from MockCrashEndpoint::Client::GetProductNameAndVersion, which
  // is only defined for non-MAC POSIX systems. TODO(crbug.com/40146362):
  // Get this info for non-POSIX platforms.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  EXPECT_THAT(actual_report->query, HasSubstr("browser_version=1.2.3.4"));
  EXPECT_THAT(actual_report->query, HasSubstr("channel=Stable"));
#endif
  EXPECT_EQ(actual_report->content, "bad_func(1, 2)\nonclick()\n");
}

TEST_F(ChromeJsErrorReportProcessorTest, AllFields) {
  TestAllFields();
}

#if !BUILDFLAG(IS_CHROMEOS)
// On Chrome OS, consent checks are handled in the crash_reporter, not in the
// browser.
TEST_F(ChromeJsErrorReportProcessorTest, NoConsent) {
  endpoint_->set_consented(false);
  auto report = MakeErrorReport("Hello World");
  report.url = "https://www.chromium.org/Home";

  SendErrorReport(std::move(report));
  EXPECT_TRUE(finish_callback_was_called_);

  EXPECT_FALSE(endpoint_->last_report());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(ChromeJsErrorReportProcessorTest, StackTraceWithErrorMessage) {
  auto report = MakeErrorReport("Hello World");
  report.url = "https://www.chromium.org/Home";
  report.stack_trace = "Hello World\nbad_func(1, 2)\nonclick()\n";

  SendErrorReport(std::move(report));
  EXPECT_TRUE(finish_callback_was_called_);

  const std::optional<MockCrashEndpoint::Report>& actual_report =
      endpoint_->last_report();
  ASSERT_TRUE(actual_report);
  EXPECT_THAT(actual_report->query, HasSubstr("error_message=Hello%20World"));
  EXPECT_EQ(actual_report->content, "bad_func(1, 2)\nonclick()\n");
}

TEST_F(ChromeJsErrorReportProcessorTest, RedactMessage) {
  auto report = MakeErrorReport("alpha@beta.org says hi to gamma@omega.co.uk");
  report.url = "https://www.chromium.org/Home";
  report.stack_trace =
      "alpha@beta.org says hi to gamma@omega.co.uk\n"
      "bad_func(1, 2)\nonclick()\n";

  SendErrorReport(std::move(report));
  EXPECT_TRUE(finish_callback_was_called_);

  const std::optional<MockCrashEndpoint::Report>& actual_report =
      endpoint_->last_report();
  ASSERT_TRUE(actual_report);
  // Escaped version of "(email: 1) says hi to (email: 2)"
  EXPECT_THAT(actual_report->query,
              HasSubstr("error_message=(email%3A%201)%20says%20hi%20to%20"
                        "(email%3A%202)"));
  // Redacted messages still need to be removed from stack trace.
  EXPECT_EQ(actual_report->content, "bad_func(1, 2)\nonclick()\n");
}

TEST_F(ChromeJsErrorReportProcessorTest, TruncateMessage) {
  std::string long_error_message;
  for (int i = 0; i <= 2000; i++) {
    base::StrAppend(&long_error_message, {base::NumberToString(i), "~"});
  }
  auto report = MakeErrorReport(long_error_message);
  report.url = "https://www.chromium.org/Home";

  SendErrorReport(std::move(report));
  EXPECT_TRUE(finish_callback_was_called_);

  const std::optional<MockCrashEndpoint::Report>& actual_report =
      endpoint_->last_report();
  ASSERT_TRUE(actual_report);

  // Find the error message line.
  base::StringPairs lines;
  ASSERT_TRUE(base::SplitStringIntoKeyValuePairs(actual_report->query, '=', '&',
                                                 &lines))
      << "Failed to split '" << actual_report->query << "'; got "
      << JoinStringPairs(lines);
  bool found_error_message = false;
  for (const auto& line : lines) {
    if (line.first == "error_message") {
      const std::string& error_message = line.second;
      // Size will be 1004 because the [ and ] of --[TRUNCATED]-- are escaped.
      EXPECT_THAT(error_message, SizeIs(1004));
      EXPECT_THAT(error_message, StartsWith("0~1~2~3~4~5~6~7~8~9~10~11~12"));
      EXPECT_THAT(error_message, EndsWith("1996~1997~1998~1999~2000~"));
      EXPECT_THAT(error_message, HasSubstr("--%5BTRUNCATED%5D--"));
      found_error_message = true;
      break;
    }
  }
  EXPECT_TRUE(found_error_message)
      << "Didn't find error_message in " << actual_report->query;
}

TEST_F(ChromeJsErrorReportProcessorTest, TruncateMessageWithEscapes) {
  std::string long_error_message(2000, ' ');
  auto report = MakeErrorReport(long_error_message);
  report.url = "https://www.chromium.org/Home";

  SendErrorReport(std::move(report));
  EXPECT_TRUE(finish_callback_was_called_);

  const std::optional<MockCrashEndpoint::Report>& actual_report =
      endpoint_->last_report();
  ASSERT_TRUE(actual_report);

  // Find the error message line.
  base::StringPairs lines;
  ASSERT_TRUE(base::SplitStringIntoKeyValuePairs(actual_report->query, '=', '&',
                                                 &lines))
      << "Failed to split '" << actual_report->query << "'; got "
      << JoinStringPairs(lines);
  bool found_error_message = false;
  for (const auto& line : lines) {
    if (line.first == "error_message") {
      const std::string& error_message = line.second;
      // Every character except the 24 -'s and letters of --[TRUNCATED]-- are
      // escaped. So size is 13 + (3*(1000 - 13)) = 2974
      EXPECT_THAT(error_message, SizeIs(2974));
      EXPECT_THAT(error_message, StartsWith("%20%20%20%20%20"));
      EXPECT_THAT(error_message, EndsWith("%20%20%20%20%20"));
      // Truncation happens before escapes so it can't cut in the middle of an
      // escape sequence:
      EXPECT_THAT(error_message, HasSubstr("%20%20--%5BTRUNCATED%5D--%20%20"));
      found_error_message = true;
      break;
    }
  }
  EXPECT_TRUE(found_error_message)
      << "Didn't find error_message in " << actual_report->query;
}

TEST_F(ChromeJsErrorReportProcessorTest, NoMoreThanOneDuplicatePerHour) {
  SendErrorReport(MakeErrorReport(kFirstMessage));
  EXPECT_TRUE(finish_callback_was_called_);
  EXPECT_EQ(endpoint_->report_count(), 1);

  finish_callback_was_called_ = false;
  test_clock_.SetNow(test_clock_.Now() + base::Minutes(1));
  SendErrorReport(MakeErrorReport(kFirstMessage));
  EXPECT_TRUE(finish_callback_was_called_);
  EXPECT_EQ(endpoint_->report_count(), 1);
}

TEST_F(ChromeJsErrorReportProcessorTest, MultipleDistinctReportsAllowed) {
  SendErrorReport(MakeErrorReport(kFirstMessage));
  EXPECT_EQ(endpoint_->report_count(), 1);

  SendErrorReport(MakeErrorReport(kSecondMessage));
  EXPECT_EQ(endpoint_->report_count(), 2);
  EXPECT_THAT(endpoint_->last_report()->query, HasSubstr(kSecondMessageQuery));
}

TEST_F(ChromeJsErrorReportProcessorTest, DuplicatesAllowedAfterAnHour) {
  SendErrorReport(MakeErrorReport(kFirstMessage));
  EXPECT_EQ(endpoint_->report_count(), 1);

  test_clock_.SetNow(test_clock_.Now() + base::Minutes(45));
  SendErrorReport(MakeErrorReport(kFirstMessage));
  EXPECT_EQ(endpoint_->report_count(), 1);

  test_clock_.SetNow(test_clock_.Now() + base::Minutes(20));
  SendErrorReport(MakeErrorReport(kFirstMessage));
  EXPECT_EQ(endpoint_->report_count(), 2);
}

TEST_F(ChromeJsErrorReportProcessorTest, DuplicateTimingIsIndependent) {
  SendErrorReport(MakeErrorReport(kFirstMessage));
  EXPECT_EQ(endpoint_->report_count(), 1);

  test_clock_.SetNow(test_clock_.Now() + base::Minutes(15));
  SendErrorReport(MakeErrorReport(kSecondMessage));
  EXPECT_EQ(endpoint_->report_count(), 2);

  test_clock_.SetNow(test_clock_.Now() + base::Minutes(15));
  SendErrorReport(MakeErrorReport(kThirdMessage));
  EXPECT_EQ(endpoint_->report_count(), 3);

  test_clock_.SetNow(test_clock_.Now() + base::Minutes(15));
  // 45 minutes from first error, all of these should be ignored.
  SendErrorReport(MakeErrorReport(kFirstMessage));
  SendErrorReport(MakeErrorReport(kSecondMessage));
  SendErrorReport(MakeErrorReport(kThirdMessage));
  EXPECT_EQ(endpoint_->report_count(), 3);  // Unchanged

  // An hour+ from first error. First error should be OK to send again, others
  // should not.
  test_clock_.SetNow(test_clock_.Now() + base::Minutes(20));
  SendErrorReport(MakeErrorReport(kFirstMessage));
  SendErrorReport(MakeErrorReport(kSecondMessage));
  SendErrorReport(MakeErrorReport(kThirdMessage));
  EXPECT_EQ(endpoint_->report_count(), 4);
  EXPECT_THAT(endpoint_->last_report()->query, HasSubstr(kFirstMessageQuery));

  // An hour+ from second error. First error should be back in cooldown, and
  // third error should still be blocked from its original send.
  test_clock_.SetNow(test_clock_.Now() + base::Minutes(15));
  SendErrorReport(MakeErrorReport(kFirstMessage));
  SendErrorReport(MakeErrorReport(kSecondMessage));
  SendErrorReport(MakeErrorReport(kThirdMessage));
  EXPECT_EQ(endpoint_->report_count(), 5);
  EXPECT_THAT(endpoint_->last_report()->query, HasSubstr(kSecondMessageQuery));

  // An hour+ from third error. First and second are still in cooldown.
  test_clock_.SetNow(test_clock_.Now() + base::Minutes(15));
  SendErrorReport(MakeErrorReport(kFirstMessage));
  SendErrorReport(MakeErrorReport(kSecondMessage));
  SendErrorReport(MakeErrorReport(kThirdMessage));
  EXPECT_EQ(endpoint_->report_count(), 6);
  EXPECT_THAT(endpoint_->last_report()->query, HasSubstr(kThirdMessageQuery));
}

TEST_F(ChromeJsErrorReportProcessorTest,
       BackwardsClockResetsAllDuplicateBlocks) {
  SendErrorReport(MakeErrorReport(kFirstMessage));
  EXPECT_EQ(endpoint_->report_count(), 1);

  test_clock_.SetNow(test_clock_.Now() + base::Minutes(15));
  SendErrorReport(MakeErrorReport(kSecondMessage));
  EXPECT_EQ(endpoint_->report_count(), 2);

  test_clock_.SetNow(test_clock_.Now() + base::Minutes(15));
  SendErrorReport(MakeErrorReport(kThirdMessage));
  EXPECT_EQ(endpoint_->report_count(), 3);

  // Move clock back 10 hours.
  test_clock_.SetNow(test_clock_.Now() + base::Minutes(-600));
  SendErrorReport(MakeErrorReport(kFirstMessage));
  EXPECT_EQ(endpoint_->report_count(), 4);
  EXPECT_THAT(endpoint_->last_report()->query, HasSubstr(kFirstMessageQuery));

  test_clock_.SetNow(test_clock_.Now() + base::Minutes(15));
  SendErrorReport(MakeErrorReport(kFirstMessage));
  SendErrorReport(MakeErrorReport(kSecondMessage));
  EXPECT_EQ(endpoint_->report_count(), 5);
  EXPECT_THAT(endpoint_->last_report()->query, HasSubstr(kSecondMessageQuery));

  // First and second are still in cooldown.
  test_clock_.SetNow(test_clock_.Now() + base::Minutes(15));
  SendErrorReport(MakeErrorReport(kFirstMessage));
  SendErrorReport(MakeErrorReport(kSecondMessage));
  SendErrorReport(MakeErrorReport(kThirdMessage));
  EXPECT_EQ(endpoint_->report_count(), 6);
  EXPECT_THAT(endpoint_->last_report()->query, HasSubstr(kThirdMessageQuery));
}

TEST_F(ChromeJsErrorReportProcessorTest,
       BackwardsClockResetsSomeDuplicateBlocks) {
  SendErrorReport(MakeErrorReport(kFirstMessage));
  EXPECT_EQ(endpoint_->report_count(), 1);

  test_clock_.SetNow(test_clock_.Now() + base::Minutes(15));
  SendErrorReport(MakeErrorReport(kSecondMessage));
  EXPECT_EQ(endpoint_->report_count(), 2);

  test_clock_.SetNow(test_clock_.Now() + base::Minutes(15));
  SendErrorReport(MakeErrorReport(kThirdMessage));
  EXPECT_EQ(endpoint_->report_count(), 3);

  // Move clock back before 3rd message was sent.
  test_clock_.SetNow(test_clock_.Now() + base::Minutes(-10));
  SendErrorReport(MakeErrorReport(kFirstMessage));
  SendErrorReport(MakeErrorReport(kSecondMessage));
  SendErrorReport(MakeErrorReport(kThirdMessage));
  EXPECT_EQ(endpoint_->report_count(), 4);
  EXPECT_THAT(endpoint_->last_report()->query, HasSubstr(kThirdMessageQuery));
}
TEST_F(ChromeJsErrorReportProcessorTest, DuplicateMapIsCleanedUpAfterAnHour) {
  SendErrorReport(MakeErrorReport(kFirstMessage));
  SendErrorReport(MakeErrorReport(kSecondMessage));

  test_clock_.SetNow(test_clock_.Now() + base::Minutes(70));
  SendErrorReport(MakeErrorReport(kThirdMessage));
  // Only record for third message should be present now.
  EXPECT_THAT(processor_->get_recent_error_reports_for_testing(), SizeIs(1));
}

TEST_F(ChromeJsErrorReportProcessorTest, DifferentProductsAreDistinct) {
  auto report = MakeErrorReport(kFirstMessage);
  report.product = kProduct;
  SendErrorReport(std::move(report));
  EXPECT_EQ(endpoint_->report_count(), 1);

  auto report2 = MakeErrorReport(kFirstMessage);
  report2.product = kSecondProduct;
  SendErrorReport(std::move(report2));
  EXPECT_EQ(endpoint_->report_count(), 2);

  auto report3 = MakeErrorReport(kFirstMessage);
  // No product at all, using default.
  SendErrorReport(std::move(report3));
  EXPECT_EQ(endpoint_->report_count(), 3);
}

TEST_F(ChromeJsErrorReportProcessorTest, DifferentLineNumbersAreDistinct) {
  // Many error messages have the same message, especially exceptions. Make sure
  // errors from different source lines are treated as distinct.
  auto report = MakeErrorReport(kFirstMessage);
  report.line_number = 10;
  SendErrorReport(std::move(report));
  EXPECT_EQ(endpoint_->report_count(), 1);

  auto report2 = MakeErrorReport(kFirstMessage);
  report2.line_number = 20;
  SendErrorReport(std::move(report2));
  EXPECT_EQ(endpoint_->report_count(), 2);

  auto report3 = MakeErrorReport(kFirstMessage);
  // No line number at all.
  SendErrorReport(std::move(report3));
  EXPECT_EQ(endpoint_->report_count(), 3);
}

TEST_F(ChromeJsErrorReportProcessorTest, DifferentColumnNumbersAreDistinct) {
  auto report = MakeErrorReport(kFirstMessage);
  report.column_number = 10;
  SendErrorReport(std::move(report));
  EXPECT_EQ(endpoint_->report_count(), 1);

  auto report2 = MakeErrorReport(kFirstMessage);
  report2.column_number = 20;
  SendErrorReport(std::move(report2));
  EXPECT_EQ(endpoint_->report_count(), 2);

  auto report3 = MakeErrorReport(kFirstMessage);
  // No column number at all.
  SendErrorReport(std::move(report3));
  EXPECT_EQ(endpoint_->report_count(), 3);
}

#if !BUILDFLAG(IS_CHROMEOS)
static std::string UploadInfoStateToString(
    UploadList::UploadInfo::State state) {
  switch (state) {
    case UploadList::UploadInfo::State::NotUploaded:
      return "NotUploaded";
    case UploadList::UploadInfo::State::Pending:
      return "Pending";
    case UploadList::UploadInfo::State::Pending_UserRequested:
      return "Pending_UserRequested";
    case UploadList::UploadInfo::State::Uploaded:
      return "Uploaded";
    default:
      return base::StrCat({"Unknown upload state ",
                           base::NumberToString(static_cast<int>(state))});
  }
}

static std::string UploadInfoVectorToString(
    const std::vector<const UploadList::UploadInfo*>& uploads) {
  std::string result = "[";
  bool first = true;
  for (const UploadList::UploadInfo* upload : uploads) {
    if (first) {
      first = false;
    } else {
      result += ", ";
    }
    auto file_size =
        upload->file_size.has_value()
            ? base::UTF16ToUTF8(ui::FormatBytes(*upload->file_size))
            : "";
    base::StrAppend(
        &result, {"{state ", UploadInfoStateToString(upload->state),
                  ", upload_id ", upload->upload_id, ", upload_time ",
                  base::NumberToString(upload->upload_time.ToTimeT()),
                  ", local_id ", upload->local_id, ", capture_time ",
                  base::NumberToString(upload->capture_time.ToTimeT()),
                  ", source ", upload->source, ", file size ", file_size, "}"});
  }
  result += "]";
  return result;
}

TEST_F(ChromeJsErrorReportProcessorTest, UpdatesUploadsLog) {
  base::ScopedPathOverride crash_dir_override(chrome::DIR_CRASH_DUMPS);
  processor_->set_update_report_database(true);

  constexpr char kCrashId[] = "123abc456def";
  endpoint_->set_response(net::HTTP_OK, kCrashId);

  SendErrorReport(MakeErrorReport(kFirstMessage));
  EXPECT_EQ(endpoint_->report_count(), 1);

  auto upload_list = CreateCrashUploadList();
  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();
  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(50);
  EXPECT_EQ(uploads.size(), 1U) << UploadInfoVectorToString(uploads);

  bool found = false;
  for (const UploadList::UploadInfo* upload : uploads) {
    if (upload->state == UploadList::UploadInfo::State::Uploaded &&
        upload->upload_id == kCrashId) {
      EXPECT_FALSE(found) << "Found twice";
      found = true;
      EXPECT_EQ(upload->upload_time.ToTimeT(), kFakeNow);
    }
  }
  EXPECT_TRUE(found) << "Didn't find upload record in "
                     << UploadInfoVectorToString(uploads);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ChromeJsErrorReportProcessorTest, WorksWithoutMemfdCreate) {
  processor_->set_force_non_memfd_for_test();
  TestAllFields();
}
#endif  // BUILDFLAG(IS_CHROMEOS)
