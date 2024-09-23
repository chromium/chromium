// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/test/launcher/test_results_tracker.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_switches.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {

namespace {

// The default output file for XML output.
const FilePath::CharType kDefaultOutputFile[] = FILE_PATH_LITERAL(
    "test_detail.xml");

// Converts the given epoch time in milliseconds to a date string in the ISO
// 8601 format, without the timezone information.
// TODO(pkasting): Consider using `TimeFormatAsIso8601()`, possibly modified.
std::string FormatTimeAsIso8601(Time time) {
  return base::UnlocalizedTimeFormatWithPattern(time, "yyyy-MM-dd'T'HH:mm:ss",
                                                icu::TimeZone::getGMT());
}

struct TestSuiteResultsAggregator {
  TestSuiteResultsAggregator()
      : tests(0), failures(0), disabled(0), errors(0) {}

  void Add(const TestResult& result) {
    tests++;
    elapsed_time += result.elapsed_time;

    switch (result.status) {
      case TestResult::TEST_SUCCESS:
        break;
      case TestResult::TEST_FAILURE:
        failures++;
        break;
      case TestResult::TEST_EXCESSIVE_OUTPUT:
      case TestResult::TEST_FAILURE_ON_EXIT:
      case TestResult::TEST_TIMEOUT:
      case TestResult::TEST_CRASH:
      case TestResult::TEST_UNKNOWN:
      case TestResult::TEST_NOT_RUN:
        errors++;
        break;
      case TestResult::TEST_SKIPPED:
        disabled++;
        break;
    }
  }

  int tests;
  int failures;
  int disabled;
  int errors;

  TimeDelta elapsed_time;
};

}  // namespace

TestResultsTracker::TestResultsTracker() : iteration_(-1), out_(nullptr) {}

TestResultsTracker::~TestResultsTracker() {
  CHECK(thread_checker_.CalledOnValidThread());

  if (!out_)
    return;

  CHECK_GE(iteration_, 0);

  // Maps test case names to test results.
  typedef std::map<std::string, std::vector<TestResult> > TestCaseMap;
  TestCaseMap test_case_map;

  TestSuiteResultsAggregator all_tests_aggregator;
  for (const PerIterationData::ResultsMap::value_type& i
           : per_iteration_data_[iteration_].results) {
    // Use the last test result as the final one.
    TestResult result = i.second.test_results.back();
    test_case_map[result.GetTestCaseName()].push_back(result);
    all_tests_aggregator.Add(result);
  }

  fprintf(out_.get(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(out_.get(),
          "<testsuites name=\"AllTests\" tests=\"%d\" failures=\"%d\""
          " disabled=\"%d\" errors=\"%d\" time=\"%.3f\" timestamp=\"%s\">\n",
          all_tests_aggregator.tests, all_tests_aggregator.failures,
          all_tests_aggregator.disabled, all_tests_aggregator.errors,
          all_tests_aggregator.elapsed_time.InSecondsF(),
          FormatTimeAsIso8601(Time::Now()).c_str());

  for (const TestCaseMap::value_type& i : test_case_map) {
    const std::string testsuite_name = i.first;
    const std::vector<TestResult>& results = i.second;

    TestSuiteResultsAggregator aggregator;
    for (const TestResult& result : results) {
      aggregator.Add(result);
    }
    fprintf(out_.get(),
            "  <testsuite name=\"%s\" tests=\"%d\" "
            "failures=\"%d\" disabled=\"%d\" errors=\"%d\" time=\"%.3f\" "
            "timestamp=\"%s\">\n",
            testsuite_name.c_str(), aggregator.tests, aggregator.failures,
            aggregator.disabled, aggregator.errors,
            aggregator.elapsed_time.InSecondsF(),
            FormatTimeAsIso8601(Time::Now()).c_str());

    for (const TestResult& result : results) {
      fprintf(out_.get(),
              "    <testcase name=\"%s\" status=\"run\" time=\"%.3f\""
              "%s classname=\"%s\">\n",
              result.GetTestName().c_str(), result.elapsed_time.InSecondsF(),
              (result.timestamp
                   ? StrCat({" timestamp=\"",
                             FormatTimeAsIso8601(*result.timestamp), "\""})
                         .c_str()
                   : ""),
              result.GetTestCaseName().c_str());
      if (result.status != TestResult::TEST_SUCCESS) {
        // The actual failure message is not propagated up to here, as it's too
        // much work to escape it properly, and in case of failure, almost
        // always one needs to look into full log anyway.
        fprintf(out_.get(),
                "      <failure message=\"\" type=\"\"></failure>\n");
      }
      fprintf(out_.get(), "    </testcase>\n");
    }
    fprintf(out_.get(), "  </testsuite>\n");
  }

  fprintf(out_.get(), "</testsuites>\n");
  fclose(out_);
}

bool TestResultsTracker::Init(const CommandLine& command_line) {
  CHECK(thread_checker_.CalledOnValidThread());

  // Prevent initializing twice.
  CHECK(!out_);

  print_temp_leaks_ =
      command_line.HasSwitch(switches::kTestLauncherPrintTempLeaks);

  if (!command_line.HasSwitch(kGTestOutputFlag))
    return true;

  std::string flag = command_line.GetSwitchValueASCII(kGTestOutputFlag);
  size_t colon_pos = flag.find(':');
  FilePath path;
  if (colon_pos != std::string::npos) {
    FilePath flag_path =
        command_line.GetSwitchValuePath(kGTestOutputFlag);
    FilePath::StringType path_string = flag_path.value();
    path = FilePath(path_string.substr(colon_pos + 1));
    // If the given path ends with '/', consider it is a directory.
    // Note: This does NOT check that a directory (or file) actually exists
    // (the behavior is same as what gtest does).
    if (path.EndsWithSeparator()) {
      FilePath executable = command_line.GetProgram().BaseName();
      path = path.Append(executable.ReplaceExtension(
                             FilePath::StringType(FILE_PATH_LITERAL("xml"))));
    }
  }
  if (path.value().empty())
    path = FilePath(kDefaultOutputFile);
  FilePath dir_name = path.DirName();
  if (!DirectoryExists(dir_name)) {
    LOG(WARNING) << "The output directory does not exist. "
                 << "Creating the directory: " << dir_name.value();
    // Create the directory if necessary (because the gtest does the same).
    if (!CreateDirectory(dir_name)) {
      LOG(ERROR) << "Failed to created directory " << dir_name.value();
      return false;
    }
  }
  out_ = OpenFile(path, "w");
  if (!out_) {
    LOG(ERROR) << "Cannot open output file: "
               << path.value() << ".";
    return false;
  }

  return true;
}

void TestResultsTracker::OnTestIterationStarting() {
  CHECK(thread_checker_.CalledOnValidThread());

  // Start with a fresh state for new iteration.
  iteration_++;
  per_iteration_data_.push_back(PerIterationData());
}

void TestResultsTracker::AddTest(const std::string& test_name) {
  // Record disabled test names without DISABLED_ prefix so that they are easy
  // to compare with regular test names, e.g. before or after disabling.
  all_tests_.insert(TestNameWithoutDisabledPrefix(test_name));
}

void TestResultsTracker::AddDisabledTest(const std::string& test_name) {
  // Record disabled test names without DISABLED_ prefix so that they are easy
  // to compare with regular test names, e.g. before or after disabling.
  disabled_tests_.insert(TestNameWithoutDisabledPrefix(test_name));
}

void TestResultsTracker::AddTestLocation(const std::string& test_name,
                                         const std::string& file,
                                         int line) {
  test_locations_.insert(std::make_pair(
      TestNameWithoutDisabledPrefix(test_name), CodeLocation(file, line)));
}

void TestResultsTracker::AddTestPlaceholder(const std::string& test_name) {
  test_placeholders_.insert(test_name);
}

void TestResultsTracker::AddTestResult(const TestResult& result) {
  CHECK(thread_checker_.CalledOnValidThread());
  CHECK_GE(iteration_, 0);

  PerIterationData::ResultsMap& results_map =
      per_iteration_data_[iteration_].results;
  std::string test_name_without_disabled_prefix =
      TestNameWithoutDisabledPrefix(result.full_name);
  auto it = results_map.find(test_name_without_disabled_prefix);

  // Record disabled test names without DISABLED_ prefix so that they are easy
  // to compare with regular test names, e.g. before or after disabling.
  AggregateTestResult& aggregate_test_result = it->second;

  // If the current test_result is a PRE test and it failed, insert its result
  // in the corresponding non-PRE test's place.
  std::string test_name_without_pre_prefix(test_name_without_disabled_prefix);
  ReplaceSubstringsAfterOffset(&test_name_without_pre_prefix, 0, "PRE_", "");
  if (test_name_without_pre_prefix != test_name_without_disabled_prefix) {
    if (result.status != TestResult::TEST_SUCCESS) {
      it = results_map.find(test_name_without_pre_prefix);
      if (!it->second.test_results.empty() &&
          it->second.test_results.back().status == TestResult::TEST_NOT_RUN) {
        // Also need to remove the non-PRE test's placeholder.
        it->second.test_results.pop_back();
      }
      it->second.test_results.push_back(result);
    }
    // We quit early here and let the non-PRE test detect this result and
    // modify its result appropriately.
    return;
  }

  // If the last test result is a placeholder, then get rid of it now that we
  // have real results.
  if (!aggregate_test_result.test_results.empty() &&
      aggregate_test_result.test_results.back().status ==
          TestResult::TEST_NOT_RUN) {
    aggregate_test_result.test_results.pop_back();
  }

  TestResult result_to_add = result;
  result_to_add.full_name = test_name_without_disabled_prefix;
  if (!aggregate_test_result.test_results.empty()) {
    TestResult prev_result = aggregate_test_result.test_results.back();
    if (prev_result.full_name != test_name_without_disabled_prefix) {
      // Some other test's result is in our place! It must be our failed PRE
      // test. Modify our own result if it failed and we succeeded so we don't
      // end up silently swallowing PRE-only failures.
      std::string prev_result_name(prev_result.full_name);
      ReplaceSubstringsAfterOffset(&prev_result_name, 0, "PRE_", "");
      CHECK_EQ(prev_result_name, test_name_without_disabled_prefix);

      if (result.status == TestResult::TEST_SUCCESS) {
        TestResult modified_result(prev_result);
        modified_result.full_name = test_name_without_disabled_prefix;
        result_to_add = modified_result;
      }
      aggregate_test_result.test_results.pop_back();
    }
  }
  aggregate_test_result.test_results.push_back(result_to_add);
}

void TestResultsTracker::AddLeakedItems(
    int count,
    const std::vector<std::string>& test_names) {
  DCHECK(count);
  per_iteration_data_.back().leaked_temp_items.emplace_back(count, test_names);
}

void TestResultsTracker::GeneratePlaceholderIteration() {
  CHECK(thread_checker_.CalledOnValidThread());

  for (auto& full_test_name : test_placeholders_) {
    std::string test_name = TestNameWithoutDisabledPrefix(full_test_name);

    TestResult test_result;
    test_result.full_name = test_name;
    test_result.status = TestResult::TEST_NOT_RUN;

    // There shouldn't be any existing results when we generate placeholder
    // results.
    CHECK(
        per_iteration_data_[iteration_].results[test_name].test_results.empty())
        << test_name;
    per_iteration_data_[iteration_].results[test_name].test_results.push_back(
        test_result);
  }
}

void TestResultsTracker::PrintSummaryOfCurrentIteration() const {
  TestStatusMap tests_by_status(GetTestStatusMapForCurrentIteration());

  PrintTests(tests_by_status[TestResult::TEST_FAILURE].begin(),
             tests_by_status[TestResult::TEST_FAILURE].end(),
             "failed");
  PrintTests(tests_by_status[TestResult::TEST_FAILURE_ON_EXIT].begin(),
             tests_by_status[TestResult::TEST_FAILURE_ON_EXIT].end(),
             "failed on exit");
  PrintTests(tests_by_status[TestResult::TEST_EXCESSIVE_OUTPUT].begin(),
             tests_by_status[TestResult::TEST_EXCESSIVE_OUTPUT].end(),
             "produced excessive output");
  PrintTests(tests_by_status[TestResult::TEST_TIMEOUT].begin(),
             tests_by_status[TestResult::TEST_TIMEOUT].end(),
             "timed out");
  PrintTests(tests_by_status[TestResult::TEST_CRASH].begin(),
             tests_by_status[TestResult::TEST_CRASH].end(),
             "crashed");
  PrintTests(tests_by_status[TestResult::TEST_SKIPPED].begin(),
             tests_by_status[TestResult::TEST_SKIPPED].end(),
             "skipped");
  PrintTests(tests_by_status[TestResult::TEST_UNKNOWN].begin(),
             tests_by_status[TestResult::TEST_UNKNOWN].end(),
             "had unknown result");
  PrintTests(tests_by_status[TestResult::TEST_NOT_RUN].begin(),
             tests_by_status[TestResult::TEST_NOT_RUN].end(), "not run");

  if (print_temp_leaks_) {
    for (const auto& leaking_tests :
         per_iteration_data_.back().leaked_temp_items) {
      PrintLeaks(leaking_tests.first, leaking_tests.second);
    }
  }
}

void TestResultsTracker::PrintSummaryOfAllIterations() const {
  CHECK(thread_checker_.CalledOnValidThread());

  TestStatusMap tests_by_status(GetTestStatusMapForAllIterations());

  fprintf(stdout, "Summary of all test iterations:\n");
  fflush(stdout);

  PrintTests(tests_by_status[TestResult::TEST_FAILURE].begin(),
             tests_by_status[TestResult::TEST_FAILURE].end(),
             "failed");
  PrintTests(tests_by_status[TestResult::TEST_FAILURE_ON_EXIT].begin(),
             tests_by_status[TestResult::TEST_FAILURE_ON_EXIT].end(),
             "failed on exit");
  PrintTests(tests_by_status[TestResult::TEST_EXCESSIVE_OUTPUT].begin(),
             tests_by_status[TestResult::TEST_EXCESSIVE_OUTPUT].end(),
             "produced excessive output");
  PrintTests(tests_by_status[TestResult::TEST_TIMEOUT].begin(),
             tests_by_status[TestResult::TEST_TIMEOUT].end(),
             "timed out");
  PrintTests(tests_by_status[TestResult::TEST_CRASH].begin(),
             tests_by_status[TestResult::TEST_CRASH].end(),
             "crashed");
  PrintTests(tests_by_status[TestResult::TEST_SKIPPED].begin(),
             tests_by_status[TestResult::TEST_SKIPPED].end(),
             "skipped");
  PrintTests(tests_by_status[TestResult::TEST_UNKNOWN].begin(),
             tests_by_status[TestResult::TEST_UNKNOWN].end(),
             "had unknown result");
  PrintTests(tests_by_status[TestResult::TEST_NOT_RUN].begin(),
             tests_by_status[TestResult::TEST_NOT_RUN].end(), "not run");

  fprintf(stdout, "End of the summary.\n");
  fflush(stdout);
}

void TestResultsTracker::AddGlobalTag(const std::string& tag) {
  global_tags_.insert(tag);
}

bool TestResultsTracker::SaveSummaryAsJSON(
    const FilePath& path,
    const std::vector<std::string>& additional_tags) const {
  Value::Dict summary_root;

  Value::List global_tags;
  for (const auto& global_tag : global_tags_) {
    global_tags.Append(global_tag);
  }
  for (const auto& tag : additional_tags) {
    global_tags.Append(tag);
  }
  summary_root.Set("global_tags", std::move(global_tags));

  Value::List all_tests;
  for (const auto& test : all_tests_) {
    all_tests.Append(test);
  }
  summary_root.Set("all_tests", std::move(all_tests));

  Value::List disabled_tests;
  for (const auto& disabled_test : disabled_tests_) {
    disabled_tests.Append(disabled_test);
  }
  summary_root.Set("disabled_tests", std::move(disabled_tests));

  Value::List per_iteration_data;

  // Even if we haven't run any tests, we still have the dummy iteration.
  int max_iteration = iteration_ < 0 ? 0 : iteration_;

  for (int i = 0; i <= max_iteration; i++) {
    Value::Dict current_iteration_data;

    for (const auto& j : per_iteration_data_[i].results) {
      Value::List test_results;

      for (size_t k = 0; k < j.second.test_results.size(); k++) {
        const TestResult& test_result = j.second.test_results[k];

        Value::Dict test_result_value;

        test_result_value.Set("status", test_result.StatusAsString());
        test_result_value.Set(
            "elapsed_time_ms",
            static_cast<int>(test_result.elapsed_time.InMilliseconds()));

        if (test_result.thread_id) {
          test_result_value.Set("thread_id",
                                static_cast<int>(*test_result.thread_id));
        }
        if (test_result.process_num)
          test_result_value.Set("process_num", *test_result.process_num);
        if (test_result.timestamp) {
          // The timestamp is formatted using TimeFormatAsIso8601 instead of
          // FormatTimeAsIso8601 here for a better accuracy, since the former
          // method includes fractions of a second.
          test_result_value.Set(
              "timestamp", TimeFormatAsIso8601(*test_result.timestamp).c_str());
        }

        bool lossless_snippet = false;
        if (IsStringUTF8(test_result.output_snippet)) {
          test_result_value.Set("output_snippet", test_result.output_snippet);
          lossless_snippet = true;
        } else {
          test_result_value.Set(
              "output_snippet",
              "<non-UTF-8 snippet, see output_snippet_base64>");
        }

        // TODO(phajdan.jr): Fix typo in JSON key (losless -> lossless)
        // making sure not to break any consumers of this data.
        test_result_value.Set("losless_snippet", lossless_snippet);

        // Also include the raw version (base64-encoded so that it can be safely
        // JSON-serialized - there are no guarantees about character encoding
        // of the snippet). This can be very useful piece of information when
        // debugging a test failure related to character encoding.
        std::string base64_output_snippet =
            base::Base64Encode(test_result.output_snippet);
        test_result_value.Set("output_snippet_base64", base64_output_snippet);
        if (!test_result.links.empty()) {
          Value::Dict links;
          for (const auto& link : test_result.links) {
            Value::Dict link_info;
            link_info.Set("content", link.second);
            links.SetByDottedPath(link.first, std::move(link_info));
          }
          test_result_value.Set("links", std::move(links));
        }
        if (!test_result.tags.empty()) {
          Value::Dict tags;
          for (const auto& tag : test_result.tags) {
            Value::List tag_values;
            for (const auto& tag_value : tag.second) {
              tag_values.Append(tag_value);
            }
            Value::Dict tag_info;
            tag_info.Set("values", std::move(tag_values));
            tags.SetByDottedPath(tag.first, std::move(tag_info));
          }
          test_result_value.Set("tags", std::move(tags));
        }
        if (!test_result.properties.empty()) {
          Value::Dict properties;
          for (const auto& property : test_result.properties) {
            Value::Dict property_info;
            property_info.Set("value", property.second);
            properties.SetByDottedPath(property.first,
                                       std::move(property_info));
          }
          test_result_value.Set("properties", std::move(properties));
        }

        Value::List test_result_parts;
        for (const TestResultPart& result_part :
             test_result.test_result_parts) {
          Value::Dict result_part_value;

          result_part_value.Set("type", result_part.TypeAsString());
          result_part_value.Set("file", result_part.file_name);
          result_part_value.Set("line", result_part.line_number);

          bool lossless_summary = IsStringUTF8(result_part.summary);
          if (lossless_summary) {
            result_part_value.Set("summary", result_part.summary);
          } else {
            result_part_value.Set("summary",
                                  "<non-UTF-8 snippet, see summary_base64>");
          }
          result_part_value.Set("lossless_summary", lossless_summary);

          std::string encoded_summary = base::Base64Encode(result_part.summary);
          result_part_value.Set("summary_base64", encoded_summary);

          bool lossless_message = IsStringUTF8(result_part.message);
          if (lossless_message) {
            result_part_value.Set("message", result_part.message);
          } else {
            result_part_value.Set("message",
                                  "<non-UTF-8 snippet, see message_base64>");
          }
          result_part_value.Set("lossless_message", lossless_message);

          std::string encoded_message = base::Base64Encode(result_part.message);
          result_part_value.Set("message_base64", encoded_message);

          test_result_parts.Append(std::move(result_part_value));
        }
        test_result_value.Set("result_parts", std::move(test_result_parts));

        test_results.Append(std::move(test_result_value));
      }

      current_iteration_data.Set(j.first, std::move(test_results));
    }
    per_iteration_data.Append(std::move(current_iteration_data));
  }
  summary_root.Set("per_iteration_data", std::move(per_iteration_data));

  Value::Dict test_locations;
  for (const auto& item : test_locations_) {
    std::string test_name = item.first;
    CodeLocation location = item.second;
    Value::Dict location_value;
    location_value.Set("file", location.file);
    location_value.Set("line", location.line);
    test_locations.Set(test_name, std::move(location_value));
  }
  summary_root.Set("test_locations", std::move(test_locations));

  std::string json;
  if (!JSONWriter::Write(summary_root, &json))
    return false;

  File output(path, File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE);
  if (!output.IsValid()) {
    return false;
  }
  if (!output.WriteAtCurrentPosAndCheck(base::as_byte_span(json))) {
    return false;
  }

#if BUILDFLAG(IS_FUCHSIA)
  // File::Flush() will call fsync(). This is important on Fuchsia to ensure
  // that the file is written to the disk - the system running under qemu will
  // shutdown shortly after the test completes. On Fuchsia fsync() times out
  // after 15 seconds. Apparently this may not be enough in some cases,
  // particularly when running net_unittests on buildbots, see
  // https://crbug.com/796318. Try calling fsync() more than once to workaround
  // this issue.
  //
  // TODO(sergeyu): Figure out a better solution.
  int flush_attempts_left = 4;
  while (flush_attempts_left-- > 0) {
    if (output.Flush())
      return true;
    LOG(ERROR) << "fsync() failed when saving test output summary. "
               << ((flush_attempts_left > 0) ? "Retrying." : " Giving up.");
  }

  return false;
#else
  return true;
#endif
}

TestResultsTracker::TestStatusMap
    TestResultsTracker::GetTestStatusMapForCurrentIteration() const {
  TestStatusMap tests_by_status;
  GetTestStatusForIteration(iteration_, &tests_by_status);
  return tests_by_status;
}

TestResultsTracker::TestStatusMap
    TestResultsTracker::GetTestStatusMapForAllIterations() const {
  TestStatusMap tests_by_status;
  for (int i = 0; i <= iteration_; i++)
    GetTestStatusForIteration(i, &tests_by_status);
  return tests_by_status;
}

void TestResultsTracker::GetTestStatusForIteration(
    int iteration, TestStatusMap* map) const {
  for (const auto& j : per_iteration_data_[iteration].results) {
    // Use the last test result as the final one.
    const TestResult& result = j.second.test_results.back();
    (*map)[result.status].insert(result.full_name);
  }
}

// Utility function to print a list of test names. Uses iterator to be
// compatible with different containers, like vector and set.
template<typename InputIterator>
void TestResultsTracker::PrintTests(InputIterator first,
                                    InputIterator last,
                                    const std::string& description) const {
  size_t count = std::distance(first, last);
  if (count == 0)
    return;

  fprintf(stdout,
          "%" PRIuS " test%s %s:\n",
          count,
          count != 1 ? "s" : "",
          description.c_str());
  for (InputIterator it = first; it != last; ++it) {
    const std::string& test_name = *it;
    const auto location_it = test_locations_.find(test_name);
    CHECK(location_it != test_locations_.end()) << test_name;
    const CodeLocation& location = location_it->second;
    fprintf(stdout, "    %s (%s:%d)\n", test_name.c_str(),
            location.file.c_str(), location.line);
  }
  fflush(stdout);
}

void TestResultsTracker::PrintLeaks(
    int count,
    const std::vector<std::string>& test_names) const {
  fprintf(stdout,
          "ERROR: %d files and/or directories were left behind in the temporary"
          " directory by one or more of these tests: %s\n",
          count, JoinString(test_names, ":").c_str());
  fflush(stdout);
}

TestResultsTracker::AggregateTestResult::AggregateTestResult() = default;

TestResultsTracker::AggregateTestResult::AggregateTestResult(
    const AggregateTestResult& other) = default;

TestResultsTracker::AggregateTestResult::~AggregateTestResult() = default;

TestResultsTracker::PerIterationData::PerIterationData() = default;

TestResultsTracker::PerIterationData::PerIterationData(
    const PerIterationData& other) = default;

TestResultsTracker::PerIterationData::~PerIterationData() = default;

}  // namespace base
