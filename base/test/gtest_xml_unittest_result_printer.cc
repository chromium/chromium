// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_xml_unittest_result_printer.h"

#include <string_view>

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_util.h"
#include "base/test/test_switches.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace base {

namespace {
const int kDefaultTestPartResultsLimit = 10;

const char kTestPartLesultsLimitExceeded[] =
    "Test part results limit exceeded. Use --test-launcher-test-part-limit to "
    "increase or disable limit.";

std::string EscapeString(std::string_view input_string) {
  std::string escaped_string;
  ReplaceChars(input_string, "&", "&amp;", &escaped_string);
  ReplaceChars(escaped_string, "<", "&lt;", &escaped_string);
  ReplaceChars(escaped_string, ">", "&gt;", &escaped_string);
  ReplaceChars(escaped_string, "'", "&apos;", &escaped_string);
  ReplaceChars(escaped_string, "\"", "&quot;", &escaped_string);
  return escaped_string;
}

}  // namespace

XmlUnitTestResultPrinter* XmlUnitTestResultPrinter::instance_ = nullptr;

XmlUnitTestResultPrinter::XmlUnitTestResultPrinter() {
  DCHECK_EQ(instance_, nullptr);
  instance_ = this;
}

XmlUnitTestResultPrinter::~XmlUnitTestResultPrinter() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
  if (output_file_ && !open_failed_) {
    fprintf(output_file_.get(), "</testsuites>\n");
    fflush(output_file_);
    CloseFile(output_file_.ExtractAsDangling());
  }
}

XmlUnitTestResultPrinter* XmlUnitTestResultPrinter::Get() {
  DCHECK(instance_);
  DCHECK(instance_->thread_checker_.CalledOnValidThread());
  return instance_;
}

void XmlUnitTestResultPrinter::AddLink(const std::string& name,
                                       const std::string& url) {
  DCHECK(output_file_);
  DCHECK(!open_failed_);
  // Escape the url so it's safe to save in xml file.
  const std::string escaped_url = EscapeString(url);
  const testing::TestInfo* info =
      testing::UnitTest::GetInstance()->current_test_info();
  // When this function is not called from a gtest test body, it will
  // return null. E.g. call from Chromium itself or from test launcher.
  // But when that happens, the previous two DCHECK won't pass. So in
  // theory it should not be possible to reach here and the info is null.
  DCHECK(info);

  UNSAFE_TODO(fprintf(output_file_.get(),
                      "    <link name=\"%s\" classname=\"%s\" "
                      "link_name=\"%s\">%s</link>\n",
                      info->name(), info->test_suite_name(), name.c_str(),
                      escaped_url.c_str()));
  fflush(output_file_);
}

void XmlUnitTestResultPrinter::AddTag(const std::string& name,
                                      const std::string& value) {
  DCHECK(output_file_);
  DCHECK(!open_failed_);
  // Escape the value so it's safe to save in xml file.
  const std::string escaped_value = EscapeString(value);
  const testing::TestInfo* info =
      testing::UnitTest::GetInstance()->current_test_info();
  // When this function is not called from a gtest test body, it will
  // return null. E.g. call from Chromium itself or from test launcher.
  // But when that happens, the previous two DCHECK won't pass. So in
  // theory it should not be possible to reach here and the info is null.
  DCHECK(info);

  UNSAFE_TODO(fprintf(output_file_.get(),
                      "    <tag name=\"%s\" classname=\"%s\" "
                      "tag_name=\"%s\">%s</tag>\n",
                      info->name(), info->test_suite_name(), name.c_str(),
                      escaped_value.c_str()));
  fflush(output_file_);
}

void XmlUnitTestResultPrinter::AddSubTestResult(
    std::string_view name,
    testing::TimeInMillis elapsed_time,
    std::optional<std::string_view> failure_message) {
  CHECK(output_file_);
  CHECK(!open_failed_);
  // `name` should have already been canonicalized.
  CHECK_EQ(EscapeString(name), name);
  const testing::TestInfo* info =
      testing::UnitTest::GetInstance()->current_test_info();
  // `info` can only be null if this function is called outside of a test body,
  // which violates this function's preconditions.
  CHECK(info);

  UNSAFE_TODO(fprintf(
      output_file_.get(),
      "    <x-sub-test-result name=\"%s\" classname=\"%s\" "
      "subname=\"%s\" time=\"%.3f\"",
      info->name(), info->test_suite_name(), name.data(),
      static_cast<double>(elapsed_time) / Time::kMillisecondsPerSecond));
  if (failure_message) {
    std::string encoded = base::Base64Encode(*failure_message);
    fprintf(output_file_.get(), " failure_message=\"%s\"", encoded.c_str());
  }
  fprintf(output_file_.get(), "></x-sub-test-result>\n");
  fflush(output_file_);
}

bool XmlUnitTestResultPrinter::Initialize(const FilePath& output_file_path) {
  DCHECK(!output_file_);
  output_file_ = OpenFile(output_file_path, "w");
  if (!output_file_) {
    // If the file open fails, we set the output location to stderr. This is
    // because in current usage our caller CHECKs the result of this function.
    // But that in turn causes a LogMessage that comes back to this object,
    // which in turn causes a (double) crash. By pointing at stderr, there might
    // be some indication what's going wrong. See https://crbug.com/736783.
    output_file_ = stderr;
    open_failed_ = true;
    return false;
  }

  fprintf(output_file_.get(),
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuites>\n");
  fflush(output_file_);

  return true;
}

void XmlUnitTestResultPrinter::OnAssert(const char* file,
                                        int line,
                                        const std::string& summary,
                                        const std::string& message) {
  WriteTestPartResult(file, line, testing::TestPartResult::kFatalFailure,
                      summary, message);
}

void XmlUnitTestResultPrinter::OnTestSuiteStart(
    const testing::TestSuite& test_suite) {
  fprintf(output_file_.get(), "  <testsuite>\n");
  fflush(output_file_);
}

void XmlUnitTestResultPrinter::OnTestStart(const testing::TestInfo& test_info) {
  DCHECK(!test_running_);
  // This is our custom extension - it helps to recognize which test was
  // running when the test binary crashed. Note that we cannot even open the
  // <testcase> tag here - it requires e.g. run time of the test to be known.
  UNSAFE_TODO(fprintf(
      output_file_.get(),
      "    <x-teststart name=\"%s\" classname=\"%s\" timestamp=\"%s\" />\n",
      test_info.name(), test_info.test_suite_name(),
      TimeFormatAsIso8601(Time::Now()).c_str()));
  fflush(output_file_);
  test_running_ = true;
}

void XmlUnitTestResultPrinter::OnTestEnd(const testing::TestInfo& test_info) {
  DCHECK(test_running_);
  UNSAFE_TODO(
      fprintf(output_file_.get(),
              "    <testcase name=\"%s\" status=\"run\" time=\"%.3f\""
              " classname=\"%s\" timestamp=\"%s\">\n",
              test_info.name(),
              static_cast<double>(test_info.result()->elapsed_time()) /
                  Time::kMillisecondsPerSecond,
              test_info.test_suite_name(),
              TimeFormatAsIso8601(Time::FromMillisecondsSinceUnixEpoch(
                                      test_info.result()->start_timestamp()))
                  .c_str()));
  if (test_info.result()->Failed()) {
    fprintf(output_file_.get(),
            "      <failure message=\"\" type=\"\"></failure>\n");
  }

  int limit = test_info.result()->total_part_count();
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherTestPartResultsLimit)) {
    std::string limit_str =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kTestLauncherTestPartResultsLimit);
    int test_part_results_limit =
        UNSAFE_TODO(std::strtol(limit_str.c_str(), nullptr, 10));
    if (test_part_results_limit >= 0) {
      limit = std::min(limit, test_part_results_limit);
    }
  } else {
    limit = std::min(limit, kDefaultTestPartResultsLimit);
  }

  for (int i = 0; i < limit; ++i) {
    const auto& test_part_result = test_info.result()->GetTestPartResult(i);
    WriteTestPartResult(test_part_result.file_name(),
                        test_part_result.line_number(), test_part_result.type(),
                        test_part_result.summary(), test_part_result.message());
  }

  if (test_info.result()->total_part_count() > limit) {
    WriteTestPartResult("unknown", 0, testing::TestPartResult::kNonFatalFailure,
                        kTestPartLesultsLimitExceeded,
                        kTestPartLesultsLimitExceeded);
  }

  fprintf(output_file_.get(), "    </testcase>\n");
  fflush(output_file_);
  test_running_ = false;
}

void XmlUnitTestResultPrinter::OnTestSuiteEnd(
    const testing::TestSuite& test_suite) {
  fprintf(output_file_.get(), "  </testsuite>\n");
  fflush(output_file_);
}

void XmlUnitTestResultPrinter::WriteTestPartResult(
    const char* file,
    int line,
    testing::TestPartResult::Type result_type,
    const std::string& summary,
    const std::string& message) {
  // Don't write `<x-test-result-part>` if there's no associated
  // `<x-teststart>` or open `<testcase>`.
  if (!test_running_) {
    return;
  }
  const char* type = "unknown";
  switch (result_type) {
    case testing::TestPartResult::kSuccess:
      type = "success";
      break;
    case testing::TestPartResult::kNonFatalFailure:
      type = "failure";
      break;
    case testing::TestPartResult::kFatalFailure:
      type = "fatal_failure";
      break;
    case testing::TestPartResult::kSkip:
      type = "skip";
      break;
  }
  std::string summary_encoded = base::Base64Encode(summary);
  std::string message_encoded = base::Base64Encode(message);
  UNSAFE_TODO(fprintf(
      output_file_.get(),
      "      <x-test-result-part type=\"%s\" file=\"%s\" line=\"%d\">\n"
      "        <summary>%s</summary>\n"
      "        <message>%s</message>\n"
      "      </x-test-result-part>\n",
      type, file, line, summary_encoded.c_str(), message_encoded.c_str()));
  fflush(output_file_);
}

}  // namespace base
