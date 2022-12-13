// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_metrics_manager.h"

#include <memory>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/app_list/search/search_metrics_util.h"
#include "chrome/browser/ash/app_list/search/test/search_metrics_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

using Action = SearchMetricsManager::Action;

}  // namespace

class SearchMetricsManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    metrics_manager_ = std::make_unique<SearchMetricsManager>(nullptr, nullptr);
  }

  base::HistogramTester histogram_tester_;
  std::unique_ptr<SearchMetricsManager> metrics_manager_;
};

TEST_F(SearchMetricsManagerTest, OnImpression) {
  // The kMissingNotifier log comes from the constructor with null notifier.
  histogram_tester_.ExpectUniqueSample(
      base::StrCat({kHistogramPrefix, "Error"}), Error::kMissingNotifier, 1);

  Location location = Location::kList;
  const std::u16string query = u"query";
  const std::string action_histogram(
      base::StrCat({kHistogramPrefix, "ListSearch"}));
  const std::string view_histogram(
      base::StrCat({action_histogram, ".Impression"}));

  // No update for empty results.
  std::vector<Result> results;
  metrics_manager_->OnImpression(location, results, query);
  histogram_tester_.ExpectTotalCount(view_histogram, 0);
  histogram_tester_.ExpectTotalCount(action_histogram, 0);

  // Metrics Updated for non_empty results.
  results.emplace_back(CreateFakeResult(Type::ANSWER_CARD, "result_id"));
  results.emplace_back(CreateFakeResult(Type::KEYBOARD_SHORTCUT, "result_id"));
  metrics_manager_->OnImpression(location, results, query);

  histogram_tester_.ExpectTotalCount(view_histogram, 2);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::ANSWER_CARD, 1);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::KEYBOARD_SHORTCUT,
                                      1);
  histogram_tester_.ExpectUniqueSample(action_histogram, Action::kImpression,
                                       1);

  // Duplicate type is counted once only.
  results.emplace_back(CreateFakeResult(Type::ANSWER_CARD, "result_id"));
  metrics_manager_->OnImpression(location, results, query);

  histogram_tester_.ExpectTotalCount(view_histogram, 4);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::ANSWER_CARD, 2);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::KEYBOARD_SHORTCUT,
                                      2);
  histogram_tester_.ExpectUniqueSample(action_histogram, Action::kImpression,
                                       2);
}

TEST_F(SearchMetricsManagerTest, OnImpressionLogContinue) {
  // The kMissingNotifier log comes from the constructor with null notifier.
  histogram_tester_.ExpectUniqueSample(
      base::StrCat({kHistogramPrefix, "Error"}), Error::kMissingNotifier, 1);

  Location location = Location::kContinue;
  const std::u16string query = u"query";

  const std::string total_histogram(
      base::StrCat({kHistogramPrefix, "ContinueResultCount.Total"}));
  const std::string drive_histogram(
      base::StrCat({kHistogramPrefix, "ContinueResultCount.Drive"}));
  const std::string local_histogram(
      base::StrCat({kHistogramPrefix, "ContinueResultCount.Local"}));
  const std::string help_app_histogram(
      base::StrCat({kHistogramPrefix, "ContinueResultCount.HelpApp"}));
  const std::string bool_histogram(
      base::StrCat({kHistogramPrefix, "DriveContinueResultsShown"}));

  // Without drive results.
  std::vector<Result> results;
  results.emplace_back(CreateFakeResult(Type::ZERO_STATE_FILE, "result_id"));
  results.emplace_back(CreateFakeResult(Type::HELP_APP_UPDATES, "result_id"));
  results.emplace_back(CreateFakeResult(Type::HELP_APP_UPDATES, "result_id"));
  metrics_manager_->OnImpression(location, results, query);

  histogram_tester_.ExpectUniqueSample(total_histogram, 3, 1);
  histogram_tester_.ExpectUniqueSample(drive_histogram, 0, 1);
  histogram_tester_.ExpectUniqueSample(local_histogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(help_app_histogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(bool_histogram, false, 1);

  // With drive results.
  results.emplace_back(CreateFakeResult(Type::ZERO_STATE_DRIVE, "result_id"));
  metrics_manager_->OnImpression(location, results, query);

  histogram_tester_.ExpectTotalCount(total_histogram, 2);
  histogram_tester_.ExpectBucketCount(total_histogram, 3, 1);
  histogram_tester_.ExpectBucketCount(total_histogram, 4, 1);

  histogram_tester_.ExpectTotalCount(drive_histogram, 2);
  histogram_tester_.ExpectBucketCount(drive_histogram, 0, 1);
  histogram_tester_.ExpectBucketCount(drive_histogram, 1, 1);

  histogram_tester_.ExpectUniqueSample(local_histogram, 1, 2);
  histogram_tester_.ExpectUniqueSample(help_app_histogram, 2, 2);

  histogram_tester_.ExpectTotalCount(bool_histogram, 2);
  histogram_tester_.ExpectBucketCount(bool_histogram, false, 1);
  histogram_tester_.ExpectBucketCount(bool_histogram, true, 1);
}

TEST_F(SearchMetricsManagerTest, OnAbandon) {
  // The kMissingNotifier log comes from the constructor with null notifier.
  histogram_tester_.ExpectUniqueSample(
      base::StrCat({kHistogramPrefix, "Error"}), Error::kMissingNotifier, 1);

  Location location = Location::kAnswerCard;
  const std::u16string query = u"query";
  const std::string action_histogram(
      base::StrCat({kHistogramPrefix, "AnswerCard"}));
  const std::string view_histogram(
      base::StrCat({action_histogram, ".Abandon"}));

  // No update for empty results.
  std::vector<Result> results;
  metrics_manager_->OnAbandon(location, results, query);
  histogram_tester_.ExpectTotalCount(view_histogram, 0);
  histogram_tester_.ExpectTotalCount(action_histogram, 0);

  // Metrics Updated for non_empty results.
  results.emplace_back(CreateFakeResult(Type::FILE_SEARCH, "result_id"));
  results.emplace_back(CreateFakeResult(Type::ASSISTANT, "result_id"));
  metrics_manager_->OnAbandon(location, results, query);

  histogram_tester_.ExpectTotalCount(view_histogram, 2);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::FILE_SEARCH, 1);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::ASSISTANT, 1);
  histogram_tester_.ExpectUniqueSample(action_histogram, Action::kAbandon, 1);

  // Duplicate type is counted once only.
  results.emplace_back(CreateFakeResult(Type::FILE_SEARCH, "result_id"));
  metrics_manager_->OnAbandon(location, results, query);

  histogram_tester_.ExpectTotalCount(view_histogram, 4);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::FILE_SEARCH, 2);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::ASSISTANT, 2);
  histogram_tester_.ExpectUniqueSample(action_histogram, Action::kAbandon, 2);
}

TEST_F(SearchMetricsManagerTest, OnLaunch) {
  // The kMissingNotifier log comes from the constructor with null notifier.
  histogram_tester_.ExpectUniqueSample(
      base::StrCat({kHistogramPrefix, "Error"}), Error::kMissingNotifier, 1);

  Location location = Location::kAnswerCard;
  const std::u16string query = u"query";

  std::vector<Result> shown_results;
  shown_results.emplace_back(CreateFakeResult(Type::FILE_SEARCH, "fake_id_0"));
  shown_results.emplace_back(CreateFakeResult(Type::FILE_SEARCH, "fake_id_1"));
  Result launched_result = CreateFakeResult(Type::FILE_SEARCH, "fake_id_2");
  shown_results.emplace_back(launched_result);
  shown_results.emplace_back(
      CreateFakeResult(Type::HELP_APP_UPDATES, "fake_id_3"));
  shown_results.emplace_back(CreateFakeResult(Type::ASSISTANT, "fake_id_4"));

  metrics_manager_->OnLaunch(location, launched_result, shown_results, query);

  const std::string action_histogram(
      base::StrCat({kHistogramPrefix, "AnswerCard"}));
  const std::string launch_histogram(
      base::StrCat({action_histogram, ".Launch"}));
  const std::string ignore_histogram(
      base::StrCat({action_histogram, ".Ignore"}));
  const std::string index_histogram(
      base::StrCat({action_histogram, ".LaunchIndex"}));

  // Update the action histogram.
  histogram_tester_.ExpectUniqueSample(action_histogram, Action::kLaunch, 1);

  // Update histogram for the launched result.
  histogram_tester_.ExpectUniqueSample(launch_histogram, Type::FILE_SEARCH, 1);

  // Update histogram for the ignored result.
  histogram_tester_.ExpectTotalCount(ignore_histogram, 2);
  histogram_tester_.ExpectBucketCount(ignore_histogram, Type::HELP_APP_UPDATES,
                                      1);
  histogram_tester_.ExpectBucketCount(ignore_histogram, Type::ASSISTANT, 1);

  // Update the index histogram.
  histogram_tester_.ExpectUniqueSample(index_histogram, 2, 1);
}

TEST_F(SearchMetricsManagerTest, OnIgnore) {
  // The kMissingNotifier log comes from the constructor with null notifier.
  histogram_tester_.ExpectUniqueSample(
      base::StrCat({kHistogramPrefix, "Error"}), Error::kMissingNotifier, 1);

  Location location = Location::kAnswerCard;
  const std::u16string query = u"query";
  const std::string action_histogram(
      base::StrCat({kHistogramPrefix, "AnswerCard"}));
  const std::string view_histogram(base::StrCat({action_histogram, ".Ignore"}));

  // No update for empty results.
  std::vector<Result> results;
  metrics_manager_->OnIgnore(location, results, query);
  histogram_tester_.ExpectTotalCount(view_histogram, 0);
  histogram_tester_.ExpectTotalCount(action_histogram, 0);

  // Metrics Updated for non_empty results.
  results.emplace_back(CreateFakeResult(Type::FILE_SEARCH, "result_id"));
  results.emplace_back(CreateFakeResult(Type::ASSISTANT, "result_id"));
  metrics_manager_->OnIgnore(location, results, query);

  histogram_tester_.ExpectTotalCount(view_histogram, 2);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::FILE_SEARCH, 1);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::ASSISTANT, 1);
  histogram_tester_.ExpectUniqueSample(action_histogram, Action::kIgnore, 1);

  // Duplicate type is counted once only.
  results.emplace_back(CreateFakeResult(Type::FILE_SEARCH, "result_id"));
  metrics_manager_->OnIgnore(location, results, query);

  histogram_tester_.ExpectTotalCount(view_histogram, 4);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::FILE_SEARCH, 2);
  histogram_tester_.ExpectBucketCount(view_histogram, Type::ASSISTANT, 2);
  histogram_tester_.ExpectUniqueSample(action_histogram, Action::kIgnore, 2);
}

}  // namespace app_list::test
