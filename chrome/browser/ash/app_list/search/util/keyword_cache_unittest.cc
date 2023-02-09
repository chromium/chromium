// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/keyword_cache.h"

#include "chrome/browser/ash/app_list/search/test/ranking_test_util.h"
#include "chrome/browser/ash/app_list/search/test/test_result.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

constexpr double kEps = 1e-3;

}  // namespace

class KeywordCacheTest : public RankerTestBase {};

// Test that last_item_scores_ and last_matched_items_ are empty after calling
// Clear(). The boost_factor should not be changed.
TEST_F(KeywordCacheTest, Clear) {
  KeywordCache cache;

  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.5});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.7, 0.5});

  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);
  cache.RecordNonMatch(results, ResultType::kFileSearch);

  cache.Clear();
  cache.Train("app_b");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.25);

  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);
  cache.RecordNonMatch(results, ResultType::kFileSearch);

  cache.Clear();
  cache.Train("local_b");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.25);
}

// Train when the selected result is boosted and rank the highest. The
// boost_factor_ should remain unchange.
TEST_F(KeywordCacheTest, BoostedResultRankHighest) {
  KeywordCache cache;

  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.5});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.7, 0.5});

  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);
  cache.RecordNonMatch(results, ResultType::kFileSearch);

  cache.Train("app_a");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.25);
}

// Train when the selected result is not boosted and rank the highest. The
// boost_factor_ should remain unchange.
TEST_F(KeywordCacheTest, NonBoostedResultRankHighest) {
  KeywordCache cache;

  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.9, 0.5});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.5});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.9, 0.5});

  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);
  cache.RecordNonMatch(results, ResultType::kFileSearch);

  cache.Train("local_a");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.25);
}

// Train when the selected result is boosted, and all result above it are
// boosted. The boost_factor_ should remain unchange.
TEST_F(KeywordCacheTest, AllTopResultsAreBoosted) {
  KeywordCache cache;

  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b", "app_c"}, {0.7, 0.6, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.6, 0.5});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.6, 0.5});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.6, 0.5});

  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);
  cache.RecordNonMatch(results, ResultType::kFileSearch);

  cache.Train("app_c");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.25);
}

// Train when the selected result is not boosted, and all result above it are
// not boosted. The boost_factor_ should remain unchange.
TEST_F(KeywordCacheTest, AllTopResultsAreNotBoosted) {
  KeywordCache cache;

  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.4, 0.3});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b", "local_c"}, {0.8, 0.7, 0.6});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.4, 0.3});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.8, 0.7, 0.6});

  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);
  cache.RecordNonMatch(results, ResultType::kFileSearch);

  cache.Train("local_c");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.25);
}

// Train() when the selected result is boosted. Out of the 2 results that scores
// higher than selected result, 1 of them is not boosted. The boost_factor_
// should increase by half of (kMaxBoostFactor - origin).
TEST_F(KeywordCacheTest, MediumIncreaseInBoostFactorTrain) {
  KeywordCache cache;

  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.5});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.7, 0.5});

  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);
  cache.RecordNonMatch(results, ResultType::kFileSearch);

  cache.Train("app_b");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.375);
}

// Train() when the selected result is boosted. Out of the 4 results that scores
// higher than selected result, 3 of them is not boosted. The boost_factor_
// should increase by 3/4 of (kMaxBoostFactor - origin).
TEST_F(KeywordCacheTest, BigIncreaseInBoostFactorTrain) {
  KeywordCache cache;

  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b", "local_c"}, {0.8, 0.7, 0.7});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.5});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.8, 0.7, 0.7});

  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);
  cache.RecordNonMatch(results, ResultType::kFileSearch);

  cache.Train("app_b");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.4375);
}

// Train() when the selected result is not boosted. Out of the 8 results that
// scores higher than selected result, 3 of them is boosted. The boost_factor_
// should decreased by 3/8 of origin.
TEST_F(KeywordCacheTest, SamllDecreaseInBoostFactorTrain) {
  KeywordCache cache;

  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b", "app_c"}, {0.7, 0.5, 0.3});
  results[ResultType::kFileSearch] = MakeScoredResults(
      {"local_a", "local_b", "local_c", "local_d", "local_e", "local_f"},
      {0.8, 0.7, 0.7, 0.6, 0.5, 0.3});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.5, 0.3});
  SetFtrlScore(results, ProviderType::kFileSearch,
               {0.8, 0.7, 0.7, 0.6, 0.5, 0.3});

  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);
  cache.RecordNonMatch(results, ResultType::kFileSearch);

  cache.Train("local_f");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.15625);
}

// Train() multiple times where the boost_factor increased twice consecutively.
TEST_F(KeywordCacheTest, ConsecutiveIncreaseInBoostFactorTrain) {
  KeywordCache cache;
  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.5});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.7, 0.5});

  cache.RecordNonMatch(results, ResultType::kFileSearch);
  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);

  // Increased by 1/2 of (kMaxBoostFactor - current boost factor), that is
  // 1/2 * (0.5 - 0.25).
  cache.Train("app_b");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.375);

  // Increased by 1/2 of (kMaxBoostFactor - current boost factor), that is
  // 1/2 * (0.5 - 0.375).
  cache.Train("app_b");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.4375);
}

// Train() multiple times where the boost_factor decreased twice consecutively.
TEST_F(KeywordCacheTest, ConsecutiveDecreaseInBoostFactorTrain) {
  KeywordCache cache;
  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.5});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.7, 0.5});

  cache.RecordNonMatch(results, ResultType::kFileSearch);
  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);

  // Decreased by 2/3 of current boost_factor.
  cache.Train("local_b");
  Wait();
  EXPECT_NEAR(cache.boost_factor(), 0.083333, kEps);

  // Decreased by 2/3 of current boost_factor.
  cache.Train("local_b");
  Wait();
  EXPECT_NEAR(cache.boost_factor(), 0.0277777, kEps);
}

// Train() multiple times where boost_factor could be increased as well
// decreased.
TEST_F(KeywordCacheTest, RandomTrain) {
  KeywordCache cache;

  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.5});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.7, 0.5});

  cache.RecordNonMatch(results, ResultType::kFileSearch);
  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);

  cache.Train("app_b");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.375);

  cache.Train("local_b");
  Wait();
  EXPECT_EQ(cache.boost_factor(), 0.125);
}

// Train() multiple times where boost_factor_ becomes maximised then Train()
// multiple times where boost_factor_ becomes minimised.
TEST_F(KeywordCacheTest, TrainToMaxThenMin) {
  KeywordCache cache;

  ResultsMap results;

  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.5});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.7, 0.5});

  cache.ScoreAndRecordMatch(results, ResultType::kInstalledApp, 1.25);
  cache.RecordNonMatch(results, ResultType::kFileSearch);

  for (int i = 0; i < 10; ++i) {
    cache.Train("app_b");
  }
  Wait();
  EXPECT_NEAR(cache.boost_factor(), KeywordCache::kMaxBoostFactor, kEps);

  for (int i = 0; i < 10; ++i) {
    cache.Train("local_b");
  }
  Wait();
  EXPECT_NEAR(cache.boost_factor(), KeywordCache::kMinBoostFactor, kEps);
}

}  // namespace app_list::test
