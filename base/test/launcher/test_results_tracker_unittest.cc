// Copyright 2020 The Chromium Authors
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

TEST(TestResultsTrackerTest, SaveSummaryAsJSONWithTagInResult) {
  TestResultsTracker tracker;
  TestResult result;
  result.AddTag("tag_name", "tag_value");
  TestResultsTracker::AggregateTestResult aggregate_result;
  aggregate_result.test_results.push_back(result);
  tracker.per_iteration_data_.push_back({});
  tracker.per_iteration_data_.back().results["dummy"] = aggregate_result;
  FilePath temp_file;
  CreateTemporaryFile(&temp_file);
  ASSERT_TRUE(tracker.SaveSummaryAsJSON(temp_file, std::vector<std::string>()));
  std::string content;
  ASSERT_TRUE(ReadFileToString(temp_file, &content));
  std::string expected_content = R"raw("tags":{"tag_name":)raw"
                                 R"raw({"values":["tag_value"]}})raw";
  EXPECT_TRUE(content.find(expected_content) != std::string::npos)
      << expected_content << " not found in " << content;
}

TEST(TestResultsTrackerTest, SaveSummaryAsJSONWithMultiTagsInResult) {
  TestResultsTracker tracker;
  TestResult result;
  result.AddTag("tag_name1", "tag_value1");
  result.AddTag("tag_name2", "tag_value2");
  TestResultsTracker::AggregateTestResult aggregate_result;
  aggregate_result.test_results.push_back(result);
  tracker.per_iteration_data_.emplace_back();
  tracker.per_iteration_data_.back().results["dummy"] = aggregate_result;
  FilePath temp_file;
  CreateTemporaryFile(&temp_file);
  ASSERT_TRUE(tracker.SaveSummaryAsJSON(temp_file, std::vector<std::string>()));
  std::string content;
  ASSERT_TRUE(ReadFileToString(temp_file, &content));
  std::string expected_content = R"raw("tags":{"tag_name1":)raw"
                                 R"raw({"values":["tag_value1"]})raw"
                                 R"raw(,"tag_name2":)raw"
                                 R"raw({"values":["tag_value2"]}})raw";
  EXPECT_TRUE(content.find(expected_content) != std::string::npos)
      << expected_content << " not found in " << content;
}

TEST(TestResultsTrackerTest, SaveSummaryAsJSONWithMultiTagsSameNameInResult) {
  TestResultsTracker tracker;
  TestResult result;
  result.AddTag("tag_name", "tag_value1");
  result.AddTag("tag_name", "tag_value2");
  TestResultsTracker::AggregateTestResult aggregate_result;
  aggregate_result.test_results.push_back(result);
  tracker.per_iteration_data_.emplace_back();
  tracker.per_iteration_data_.back().results["dummy"] = aggregate_result;
  FilePath temp_file;
  CreateTemporaryFile(&temp_file);
  ASSERT_TRUE(tracker.SaveSummaryAsJSON(temp_file, std::vector<std::string>()));
  std::string content;
  ASSERT_TRUE(ReadFileToString(temp_file, &content));
  std::string expected_content = R"raw("tags":{"tag_name":)raw"
                                 R"raw({"values":)raw"
                                 R"raw(["tag_value1","tag_value2"]}})raw";
  EXPECT_TRUE(content.find(expected_content) != std::string::npos)
      << expected_content << " not found in " << content;
}

TEST(TestResultsTrackerTest, SaveSummaryAsJSONWithPropertyInResult) {
  TestResultsTracker tracker;
  TestResult result;
  result.AddProperty("test_property_name", "test_property_value");
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
  std::string expected_content = R"raw("properties":{"test_property_name":)raw"
                                 R"raw({"value":"test_property_value"}})raw";
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

TEST(TestResultsTrackerTest, RepeatDisabledTests) {
  constexpr char TEST_NAME[] = "DISABLED_Test1";
  constexpr char TEST_NAME_WITHOUT_PREFIX[] = "Test1";
  TestResultsTracker tracker;
  tracker.AddTestPlaceholder(TEST_NAME);
  tracker.OnTestIterationStarting();
  tracker.GeneratePlaceholderIteration();
  TestResult result;
  result.full_name = TEST_NAME;
  result.status = TestResult::TEST_SUCCESS;
  for (int i = 0; i < 10; i++) {
    tracker.AddTestResult(result);
  }
  TestResultsTracker::TestStatusMap results =
      tracker.GetTestStatusMapForAllIterations();
  ASSERT_TRUE(results[TestResult::TEST_SUCCESS].find(TEST_NAME) ==
              results[TestResult::TEST_SUCCESS].end());
  ASSERT_TRUE(
      results[TestResult::TEST_SUCCESS].find(TEST_NAME_WITHOUT_PREFIX) !=
      results[TestResult::TEST_SUCCESS].end());
}

}  // namespace base
