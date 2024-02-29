// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/best_match_ranker.h"

#include "base/containers/to_vector.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/test/test_result.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace {

using testing::ElementsAreArray;

std::unique_ptr<ChromeSearchResult> MakeResult(
    const std::string& id,
    double normalized_relevance,
    ChromeSearchResult::MetricsType metrics_type =
        ChromeSearchResult::MetricsType::OMNIBOX_URL_WHAT_YOU_TYPED) {
  // |relevance| must be set but is unused.
  return std::make_unique<TestResult>(id, /*relevance=*/0.0,
                                      normalized_relevance, metrics_type);
}

Results MakeAnswers(
    std::vector<std::pair<std::string, double>> ids_relevances) {
  return base::ToVector(ids_relevances, [](const auto& ids_relevance) {
    return MakeResult(ids_relevance.first, ids_relevance.second);
  });
}

}  // namespace

class BestMatchRankerTest : public testing::Test {
 public:
  void ExpectBestMatchOrderAndRanks(
      std::vector<std::pair<std::string, int>> expected_ids_ranks) {
    EXPECT_EQ(expected_ids_ranks.size(), ranker_.best_matches_.size());
    EXPECT_THAT(base::ToVector(ranker_.best_matches_,
                               [](const auto& res) {
                                 return std::make_pair(
                                     res->id(),
                                     res->scoring().best_match_rank());
                               }),
                ElementsAreArray(expected_ids_ranks));
  }

  void ElapseBurnInPeriod() { ranker_.OnBurnInPeriodElapsed(); }

  BestMatchRanker ranker_;
};

// Check that:
//   - Results below the score threshold are ignored.
//   - Some qualifying results are ignored when there are more than
//   kNumBestMatches.
//   - The sorting of results within |results_map| is unaffected.
TEST_F(BestMatchRankerTest, ResultThresholdingAndSorting) {
  ResultsMap results_map;
  results_map[ResultType::kOmnibox] = MakeAnswers({{"omni_1", 0.98},
                                                   {"omni_2", 0.3},
                                                   {"omni_3", 0.1},
                                                   {"omni_4", 0.99},
                                                   {"omni_5", 0.96},
                                                   {"omni_6", 0.97}});
  ranker_.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ExpectBestMatchOrderAndRanks({{"omni_4", 0}, {"omni_1", 1}, {"omni_6", 2}});

  const auto& results = results_map[ResultType::kOmnibox];
  ASSERT_EQ(results.size(), 6u);

  std::vector<std::string> result_map_ids;
  std::vector<int> result_map_ranks;
  std::vector<bool> shared_metadata_best_match_status;

  for (const auto& res : results) {
    result_map_ids.push_back(res->id());
    result_map_ranks.push_back(res->scoring().best_match_rank());
    shared_metadata_best_match_status.push_back(res->best_match());
  }

  // The ranker should not affect result order within the results map.
  EXPECT_THAT(result_map_ids, ElementsAreArray({"omni_1", "omni_2", "omni_3",
                                                "omni_4", "omni_5", "omni_6"}));
  // Non-best matches should have a best match rank of -1.
  EXPECT_THAT(result_map_ranks, ElementsAreArray({1, -1, -1, 0, -1, 2}));

  // The best match status of all results should be correctly reflected in
  // shared result metadata.
  EXPECT_THAT(shared_metadata_best_match_status,
              ElementsAreArray({true, false, false, true, false, true}));
}

// Check that ranker handles case where no best results are found.
TEST_F(BestMatchRankerTest, NoBestResults) {
  ResultsMap results_map;

  // Simulate one provider returning.
  results_map[ResultType::kOmnibox] =
      MakeAnswers({{"omni_1", 0.2}, {"omni_2", 0.3}});
  ranker_.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ExpectBestMatchOrderAndRanks({});
}

// Check that ranker handles case where a single best result is found.
TEST_F(BestMatchRankerTest, SingleBestResult) {
  ResultsMap results_map;

  // Simulate one provider returning.
  results_map[ResultType::kOmnibox] =
      MakeAnswers({{"omni_1", 0.2}, {"omni_2", 0.99}});
  ranker_.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ExpectBestMatchOrderAndRanks({{"omni_2", 0.99}});
}

// A result which gets stored as a best match may later be deleted. Check that
// the corresponding weak pointer stored within the ranker gets removed from the
// the ranker's best matches tracking vector.
TEST_F(BestMatchRankerTest, IgnoreInvalidatedResults) {
  ResultsMap results_map;

  // Simulate one provider returning.
  results_map[ResultType::kOmnibox] =
      MakeAnswers({{"omni_1", 0.92}, {"omni_2", 0.99}});
  ranker_.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ExpectBestMatchOrderAndRanks({{"omni_2", 0}, {"omni_1", 1}});

  // Simulate a result being destroyed.
  results_map[ResultType::kOmnibox][0].reset();

  // Simulate a second provider returning. This should result in the removal of
  // the now invalid result from above.
  results_map[ResultType::kFileSearch] = MakeAnswers({{"file_1", 0.98}});
  ranker_.UpdateResultRanks(results_map, ProviderType::kFileSearch);
  ExpectBestMatchOrderAndRanks({{"omni_2", 0}, {"file_1", 1}});
}

// Some providers should never contribute to the best matches, and thus should
// be ignored by the best match ranker.
TEST_F(BestMatchRankerTest, IgnoreProviders) {
  ResultsMap results_map;
  results_map[ResultType::kOmnibox] =
      MakeAnswers({{"omni_1", 0.92}, {"omni_2", 0.93}});
  results_map[ResultType::kAssistantText] =
      MakeAnswers({{"asst_1", 0.98}, {"asst_2", 0.97}});

  ranker_.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ranker_.UpdateResultRanks(results_map, ProviderType::kAssistantText);

  // kAssistantText is a low-intent provider and should be ignored from best
  // match. The other results should be sorted by (normalized) relevance.
  ExpectBestMatchOrderAndRanks({{"omni_2", 0}, {"omni_1", 1}});
}

// During the post-burn-in phase, the highest-ranked best match should remain
// stabilized in this position, and any remaining best matches should be sorted
// by (normalized) relevance score.
TEST_F(BestMatchRankerTest, PostBurnInHighestBestMatchIsStabilized) {
  ResultsMap results_map;

  // Simulate one provider returning pre-burn-in.
  results_map[ResultType::kOmnibox] =
      MakeAnswers({{"omni_1", 0.92}, {"omni_2", 0.93}});
  ranker_.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ExpectBestMatchOrderAndRanks({{"omni_2", 0}, {"omni_1", 1}});

  // Simulate a second provider returning post-burnin.
  ElapseBurnInPeriod();
  results_map[ResultType::kFileSearch] = MakeAnswers({{"file_1", 0.98}});
  ranker_.UpdateResultRanks(results_map, ProviderType::kFileSearch);
  // The top-ranked from the pre-burn-in period retains its rank.
  ExpectBestMatchOrderAndRanks({{"omni_2", 0}, {"file_1", 1}, {"omni_1", 2}});
}

// The Omnibox provider may return more than once. This should not cause the
// storage of duplicate best matches.
//
// This test also checks that a result which is demoted out of best match has
// this correctly reflected.
TEST_F(BestMatchRankerTest, ProviderReturnsMoreThanOnceResultDemoted) {
  ResultsMap results_map;

  // Simulate a provider returning.
  results_map[ResultType::kOmnibox] =
      MakeAnswers({{"omni_1", 0.96}, {"omni_2", 0.3}, {"omni_3", 0.1}});
  ranker_.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ExpectBestMatchOrderAndRanks({{"omni_1", 0}});

  // Simulate the same provider returning for a second time.
  results_map[ResultType::kOmnibox] = MakeAnswers(
      {{"omni_1", 0.96}, {"omni_2", 0.3}, {"omni_3", 0.1}, {"omni_4", 0.97}});
  ranker_.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ExpectBestMatchOrderAndRanks({{"omni_4", 0}, {"omni_1", 1}});

  results_map[ResultType::kOmnibox] = MakeAnswers({{"omni_1", 0.96},
                                                   {"omni_2", 0.3},
                                                   {"omni_3", 0.1},
                                                   {"omni_4", 0.97},
                                                   {"omni_5", 0.99},
                                                   {"omni_6", 0.98}});
  ranker_.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  // "omni_1" has been demoted out of the category. "omni_4" has moved downwards
  // in rank.
  ExpectBestMatchOrderAndRanks({{"omni_5", 0}, {"omni_6", 1}, {"omni_4", 2}});

  const auto& results = results_map[ResultType::kOmnibox];
  ASSERT_EQ(results.size(), 6u);

  std::vector<std::string> result_map_ids;
  std::vector<int> result_map_ranks;
  std::vector<bool> shared_metadata_best_match_status;

  for (const auto& res : results) {
    result_map_ids.push_back(res->id());
    result_map_ranks.push_back(res->scoring().best_match_rank());
    shared_metadata_best_match_status.push_back(res->best_match());
  }

  // The ranker should not affect result order within the results map.
  EXPECT_THAT(result_map_ids, ElementsAreArray({"omni_1", "omni_2", "omni_3",
                                                "omni_4", "omni_5", "omni_6"}));

  // Non-best matches should have a best match rank of -1. This includes the
  // result which was originally a best match but got demoted out of best match.
  EXPECT_THAT(result_map_ranks, ElementsAreArray({-1, -1, -1, 2, 0, 1}));

  // The best match status of all results should be correctly reflected in
  // shared result metadata.
  EXPECT_THAT(shared_metadata_best_match_status,
              ElementsAreArray({false, false, false, true, true, true}));
}

TEST_F(BestMatchRankerTest, RankerResetBetweenQueries) {
  // Simulate a query starting.
  ResultsMap results_1;
  CategoriesList categories_1;

  ranker_.Start(u"ABC", categories_1);
  results_1[ResultType::kOmnibox] =
      MakeAnswers({{"omni_1", 0.92}, {"omni_2", 0.3}});
  ranker_.UpdateResultRanks(results_1, ProviderType::kOmnibox);
  ExpectBestMatchOrderAndRanks({{"omni_1", 0}});

  // Simulate a second query starting.
  ResultsMap results_2;
  CategoriesList categories_2;

  ranker_.Start(u"ABC", categories_2);
  results_2[ResultType::kFileSearch] =
      MakeAnswers({{"files_1", 0.7}, {"files_2", 0.97}});
  ranker_.UpdateResultRanks(results_2, ProviderType::kFileSearch);
  ExpectBestMatchOrderAndRanks({{"files_2", 0}});
}

TEST_F(BestMatchRankerTest, IgnoreSearchSuggest) {
  ResultsMap results_map;

  Results results;
  results.push_back(MakeResult(
      "omni_1", 0.99, ChromeSearchResult::MetricsType::OMNIBOX_SEARCH_SUGGEST));
  results.push_back(MakeResult(
      "omni_2", 0.92,
      ChromeSearchResult::MetricsType::OMNIBOX_RECENTLY_VISITED_WEBSITE));

  // Simulate one provider returning.
  results_map[ResultType::kOmnibox] = std::move(results);
  ranker_.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ExpectBestMatchOrderAndRanks({{"omni_2", 0.99}});
}

}  // namespace app_list::test
