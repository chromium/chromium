// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_session_metrics_manager.h"

#include <memory>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/app_list/search/search_metrics_util.h"
#include "chrome/browser/ash/app_list/search/test/search_metrics_test_util.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace {

using Type = ash::SearchResultType;

constexpr char HomeButtonHistogram[] =
    "Apps.AppList.Search.Session2.HomeButton";
constexpr char SearchKeyHistogram[] = "Apps.AppList.Search.Session2.SearchKey";
constexpr char QueryLengthAggregateHistogram[] =
    "Apps.AppList.Search.Session2.QueryLength";
constexpr char QueryLengthAnswerCardSeenHistogram[] =
    "Apps.AppList.Search.Session2.QueryLength.AnswerCardSeen";
constexpr char QueryLengthLaunchHistogram[] =
    "Apps.AppList.Search.Session2.QueryLength.Launch";
constexpr char QueryLengthQuitHistogram[] =
    "Apps.AppList.Search.Session2.QueryLength.Quit";

}  // namespace

class SearchSessionMetricsManagerTest : public testing::Test {
 public:
  void SetUp() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    metrics_manager_ = std::make_unique<app_list::SearchSessionMetricsManager>(
        nullptr, nullptr);
    app_list_controller_ = std::make_unique<::test::TestAppListController>();
  }

  ::test::TestAppListController* app_list_controller() {
    return app_list_controller_.get();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  app_list::SearchSessionMetricsManager* metrics_manager() {
    return metrics_manager_.get();
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<app_list::SearchSessionMetricsManager> metrics_manager_;
  std::unique_ptr<::test::TestAppListController> app_list_controller_;
};

TEST_F(SearchSessionMetricsManagerTest, AnswerCardImpression) {
  // The kMissingNotifier log comes from the constructor with null notifier.
  histogram_tester()->ExpectUniqueSample(
      base::StrCat({app_list::kSessionHistogramPrefix, "Error"}),
      app_list::Error::kMissingNotifier, 1);

  Location location = Location::kAnswerCard;
  const std::u16string query = u"query";

  app_list_controller()->ShowAppList(ash::AppListShowSource::kSearchKey);
  std::vector<Result> results;
  results.emplace_back(CreateFakeResult(Type::ANSWER_CARD, "result_id"));
  results.emplace_back(CreateFakeResult(Type::KEYBOARD_SHORTCUT, "result_id"));

  metrics_manager()->OnSearchSessionStarted();
  metrics_manager_->OnSeen(location, results, query);

  // No metrics should be recorded until the search session ends.
  histogram_tester()->ExpectTotalCount(HomeButtonHistogram, 0);
  histogram_tester()->ExpectTotalCount(SearchKeyHistogram, 0);
  histogram_tester()->ExpectTotalCount(QueryLengthAggregateHistogram, 0);

  // One answer card impression should be recorded for the kSearchKey show
  // source.
  metrics_manager()->OnSearchSessionEnded(query);
  histogram_tester()->ExpectTotalCount(HomeButtonHistogram, 0);
  histogram_tester()->ExpectTotalCount(SearchKeyHistogram, 1);
  histogram_tester()->ExpectUniqueSample(
      SearchKeyHistogram, ash::SearchSessionConclusion::kAnswerCardSeen, 1);

  // The query length should be recorded once under the histogram relevant to
  // AnswerCardSeen, and once under an aggregate histogram.
  histogram_tester()->ExpectUniqueSample(QueryLengthAnswerCardSeenHistogram,
                                         query.length(),
                                         /*expected_bucket_count*/ 1);
  histogram_tester()->ExpectTotalCount(QueryLengthAggregateHistogram, 1);
}

TEST_F(SearchSessionMetricsManagerTest, LaunchResult) {
  // The kMissingNotifier log comes from the constructor with null notifier.
  histogram_tester()->ExpectUniqueSample(
      base::StrCat({app_list::kSessionHistogramPrefix, "Error"}),
      app_list::Error::kMissingNotifier, 1);

  Location location = Location::kList;
  const std::u16string query = u"query";

  app_list_controller()->ShowAppList(ash::AppListShowSource::kSearchKey);
  std::vector<Result> results;
  results.emplace_back(CreateFakeResult(Type::ANSWER_CARD, "result_id"));
  results.emplace_back(CreateFakeResult(Type::KEYBOARD_SHORTCUT, "result_id"));
  Result launched_result =
      CreateFakeResult(Type::FILE_SEARCH, "fake_id_launched");
  results.emplace_back(launched_result);

  metrics_manager()->OnSearchSessionStarted();
  metrics_manager_->OnSeen(location, results, query);

  // No metrics should be recorded until the search session ends.
  histogram_tester()->ExpectTotalCount(HomeButtonHistogram, 0);
  histogram_tester()->ExpectTotalCount(SearchKeyHistogram, 0);
  histogram_tester()->ExpectTotalCount(QueryLengthAggregateHistogram, 0);

  metrics_manager()->OnLaunch(location, launched_result, results, query);

  // No metrics should be recorded until the search session ends.
  histogram_tester()->ExpectTotalCount(HomeButtonHistogram, 0);
  histogram_tester()->ExpectTotalCount(SearchKeyHistogram, 0);
  histogram_tester()->ExpectTotalCount(QueryLengthAggregateHistogram, 0);

  metrics_manager()->OnSearchSessionEnded(query);
  histogram_tester()->ExpectTotalCount(HomeButtonHistogram, 0);
  histogram_tester()->ExpectTotalCount(SearchKeyHistogram, 1);
  histogram_tester()->ExpectUniqueSample(
      SearchKeyHistogram, ash::SearchSessionConclusion::kLaunch, 1);

  // The query length should be recorded once under the histogram relevant to
  // Launch, and once under an aggregate histogram.
  histogram_tester()->ExpectUniqueSample(QueryLengthLaunchHistogram,
                                         query.length(),
                                         /*expected_bucket_count*/ 1);
  histogram_tester()->ExpectTotalCount(QueryLengthAggregateHistogram, 1);
}

TEST_F(SearchSessionMetricsManagerTest, AbandonResult) {
  // The kMissingNotifier log comes from the constructor with null notifier.
  histogram_tester()->ExpectUniqueSample(
      base::StrCat({app_list::kSessionHistogramPrefix, "Error"}),
      app_list::Error::kMissingNotifier, 1);

  app_list_controller()->ShowAppList(ash::AppListShowSource::kSearchKey);
  std::vector<Result> results;
  results.emplace_back(CreateFakeResult(Type::ANSWER_CARD, "result_id"));
  results.emplace_back(CreateFakeResult(Type::KEYBOARD_SHORTCUT, "result_id"));

  const std::u16string query = u"query";

  metrics_manager()->OnSearchSessionStarted();

  // No metrics should be recorded until the search session ends.
  histogram_tester()->ExpectTotalCount(QueryLengthAggregateHistogram, 0);
  histogram_tester()->ExpectTotalCount(HomeButtonHistogram, 0);
  histogram_tester()->ExpectTotalCount(SearchKeyHistogram, 0);

  metrics_manager()->OnSearchSessionEnded(query);

  histogram_tester()->ExpectTotalCount(HomeButtonHistogram, 0);
  histogram_tester()->ExpectTotalCount(SearchKeyHistogram, 1);
  histogram_tester()->ExpectUniqueSample(
      SearchKeyHistogram, ash::SearchSessionConclusion::kQuit, 1);

  // The query length should be recorded once under the histogram relevant to
  // Quit, and once under an aggregate histogram.
  histogram_tester()->ExpectUniqueSample(QueryLengthQuitHistogram,
                                         query.length(),
                                         /*expected_bucket_count*/ 1);
  histogram_tester()->ExpectTotalCount(QueryLengthAggregateHistogram, 1);

  // No additional session should be logged if no session was started
  metrics_manager()->OnSearchSessionEnded(u"");

  histogram_tester()->ExpectTotalCount(SearchKeyHistogram, 1);
}

TEST_F(SearchSessionMetricsManagerTest, QueryLengthLoggingMultiSession) {
  // Session 1: An answer card is seen.
  Location location_1 = Location::kAnswerCard;

  std::vector<Result> results_1;
  results_1.emplace_back(CreateFakeResult(Type::ANSWER_CARD, "result_id"));
  results_1.emplace_back(
      CreateFakeResult(Type::KEYBOARD_SHORTCUT, "result_id"));

  const std::u16string query_1 = u"query";

  app_list_controller()->ShowAppList(ash::AppListShowSource::kSearchKey);
  metrics_manager()->OnSearchSessionStarted();
  metrics_manager_->OnSeen(location_1, results_1, query_1);

  // No metrics should be recorded until the search session ends.
  histogram_tester()->ExpectTotalCount(QueryLengthAggregateHistogram, 0);
  histogram_tester()->ExpectTotalCount(QueryLengthAnswerCardSeenHistogram, 0);
  histogram_tester()->ExpectTotalCount(QueryLengthQuitHistogram, 0);
  histogram_tester()->ExpectTotalCount(QueryLengthLaunchHistogram, 0);

  metrics_manager()->OnSearchSessionEnded(query_1);

  // The query length should be recorded once under the histogram relevant to
  // AnswerCardSeen, and also once under an aggregate histogram.
  histogram_tester()->ExpectUniqueSample(QueryLengthAnswerCardSeenHistogram,
                                         query_1.length(),
                                         /*expected_bucket_count*/ 1);
  histogram_tester()->ExpectTotalCount(QueryLengthAnswerCardSeenHistogram, 1);
  histogram_tester()->ExpectTotalCount(QueryLengthAggregateHistogram, 1);

  // Session 2: A result is launched.
  Location location_2 = Location::kList;

  std::vector<Result> results_2;
  results_2.emplace_back(CreateFakeResult(Type::ANSWER_CARD, "result_id"));
  results_2.emplace_back(
      CreateFakeResult(Type::KEYBOARD_SHORTCUT, "result_id"));

  Result launched_result =
      CreateFakeResult(Type::FILE_SEARCH, "fake_id_launched");
  results_2.emplace_back(launched_result);

  const std::u16string query_2 = u"another query";
  // Rest of test assumes length difference between `query_1` and
  // `query_2`.
  EXPECT_NE(query_1.length(), query_2.length());

  app_list_controller()->ShowAppList(ash::AppListShowSource::kSearchKey);
  metrics_manager()->OnSearchSessionStarted();
  metrics_manager_->OnSeen(location_2, results_2, query_2);

  // No metrics should be changed from previously, until the search session
  // ends.
  histogram_tester()->ExpectTotalCount(QueryLengthAggregateHistogram, 1);
  histogram_tester()->ExpectTotalCount(QueryLengthAnswerCardSeenHistogram, 1);
  histogram_tester()->ExpectTotalCount(QueryLengthQuitHistogram, 0);
  histogram_tester()->ExpectTotalCount(QueryLengthLaunchHistogram, 0);

  metrics_manager()->OnLaunch(location_2, launched_result, results_2, query_2);
  metrics_manager()->OnSearchSessionEnded(query_2);

  // The query length should be recorded once under the histogram relevant to
  // Launch, and also once under an aggregate histogram.
  histogram_tester()->ExpectUniqueSample(QueryLengthLaunchHistogram,
                                         query_2.length(),
                                         /*expected_bucket_count*/ 1);
  histogram_tester()->ExpectTotalCount(QueryLengthLaunchHistogram, 1);
  histogram_tester()->ExpectTotalCount(QueryLengthAggregateHistogram, 2);

  // Other histograms remain unchanged by Session 2 activity.
  histogram_tester()->ExpectTotalCount(QueryLengthAnswerCardSeenHistogram, 1);
  histogram_tester()->ExpectTotalCount(QueryLengthQuitHistogram, 0);
}

}  // namespace app_list::test
