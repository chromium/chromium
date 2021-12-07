// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_results_tracker.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;

namespace base {

TEST(TestResultsTrackerTest, SaveSummaryAsJSONWithLinkInResult) {
  TestResultsTracker tracker;
  TestResult result;
  result.AddLink("link", "http://google.com");
  TestResultsTracker::AggregateTestResult aggregate_result;
  aggregate_result.test_results.push_back(result);
  tracker.per_iteration_data_.emplace_back(
      TestResultsTracker::PerIterationData());
  tracker.per_iteration_data_.back().results["dummy"] = aggregate_result;
  FilePath temp_file;
  CreateTemporaryFile(&temp_file);
  ASSERT_TRUE(tracker.SaveSummaryAsJSON(temp_file, std::vector<std::string>()));
  std::string content;
  ASSERT_TRUE(ReadFileToString(temp_file, &content));
  std::string expected_content = R"raw("links":{"link":)raw"
                                 R"raw({"content":"http://google.com"}})raw";
  EXPECT_TRUE(content.find(expected_content) != std::string::npos)
      << expected_content << " not found in " << content;
}

TEST(TestResultsTrackerTest, SaveSummaryAsJSONWithOutTimestampInResult) {
  TestResultsTracker tracker;
  TestResult result;
  result.full_name = "A.B";

  TestResultsTracker::AggregateTestResult aggregate_result;
  aggregate_result.test_results.push_back(result);
  tracker.per_iteration_data_.emplace_back(
      TestResultsTracker::PerIterationData());
  tracker.per_iteration_data_.back().results["dummy"] = aggregate_result;
  FilePath temp_file;
  CreateTemporaryFile(&temp_file);
  ASSERT_TRUE(tracker.SaveSummaryAsJSON(temp_file, std::vector<std::string>()));
  std::string content;
  ASSERT_TRUE(ReadFileToString(temp_file, &content));

  for (auto* not_expected_content : {"thread_id", "process_num", "timestamp"}) {
    EXPECT_THAT(content, ::testing::Not(HasSubstr(not_expected_content)));
  }
}

TEST(TestResultsTrackerTest, SaveSummaryAsJSONWithTimestampInResult) {
  TestResultsTracker tracker;
  TestResult result;
  result.full_name = "A.B";
  result.thread_id = 123;
  result.process_num = 456;
  result.timestamp = Time::Now();

  TestResultsTracker::AggregateTestResult aggregate_result;
  aggregate_result.test_results.push_back(result);
  tracker.per_iteration_data_.emplace_back(
      TestResultsTracker::PerIterationData());
  tracker.per_iteration_data_.back().results["dummy"] = aggregate_result;
  FilePath temp_file;
  CreateTemporaryFile(&temp_file);
  ASSERT_TRUE(tracker.SaveSummaryAsJSON(temp_file, std::vector<std::string>()));
  std::string content;
  ASSERT_TRUE(ReadFileToString(temp_file, &content));

  EXPECT_THAT(content, HasSubstr(R"raw("thread_id":123)raw"));
  EXPECT_THAT(content, HasSubstr(R"raw("process_num":456)raw"));
  EXPECT_THAT(content, HasSubstr(R"raw("timestamp":)raw"));
}

}  // namespace base
