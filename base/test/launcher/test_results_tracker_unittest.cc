// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_results_tracker.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace base
