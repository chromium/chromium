// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/keyword_ranker.h"

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/scoring.h"
#include "chrome/browser/ash/app_list/search/test/ranking_test_util.h"
#include "chrome/browser/ash/app_list/search/test/test_result.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

constexpr double kEps = 1e-3;

// Helper function that check if the results' multiplier is right.
void ExpectKeywordMultiplier(const ResultsMap& results,
                             ProviderType provider,
                             double keyword_multiplier) {
  const auto it = results.find(provider);
  if (it == results.end()) {
    return;
  }

  for (const auto& result : it->second) {
    EXPECT_NEAR(result->scoring().keyword_multiplier(), keyword_multiplier,
                kEps);
  }
}

}  // namespace

class KeywordRankerTest : public RankerTestBase {};

/*********************** Exact String Matching Tests ***********************/

// Test the input query does not contains keyword that match any providers.
TEST_F(KeywordRankerTest, NoMatchedKeywords) {
  // Simulate a query starting.
  ResultsMap results;
  CategoriesList categories;

  // Input the results scores and UpdateResultRanks.
  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  KeywordRanker ranker;
  ranker.Start(u"ABC", categories);
  ranker.UpdateResultRanks(results, ProviderType::kInstalledApp);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results[ResultType::kInstalledApp].size(), 2u);

  // As no detection of keywords for this result type, therefore the keyword
  // multiplier is down-weighted.
  ExpectKeywordMultiplier(results, ResultType::kInstalledApp, 1.0);
}

// Test the input query only match one provider.
TEST_F(KeywordRankerTest, OneMatchedKeyword) {
  // Simulate a query starting.
  ResultsMap results;
  CategoriesList categories;

  // Input the results scores and UpdateResultRanks.
  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kHelpApp] =
      MakeScoredResults({"help_a", "help_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"explore", categories);
  ranker.UpdateResultRanks(results, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results, ProviderType::kHelpApp);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results.size(), 2u);
  ASSERT_EQ(results[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results[ResultType::kHelpApp].size(), 2u);

  ExpectKeywordMultiplier(results, ResultType::kInstalledApp, 1.0);
  ExpectKeywordMultiplier(results, ResultType::kHelpApp, 1.25);
}

// Test the input query match multiple provider.
TEST_F(KeywordRankerTest, KeywordMatchesMultipleProviders) {
  // Simulate a query starting.
  ResultsMap results;
  CategoriesList categories;

  // Input the results scores and UpdateResultRanks.
  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});
  results[ResultType::kDriveSearch] =
      MakeScoredResults({"drive_a", "drive_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"file", categories);
  ranker.UpdateResultRanks(results, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results, ProviderType::kFileSearch);
  ranker.UpdateResultRanks(results, ProviderType::kDriveSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results.size(), 3u);
  ASSERT_EQ(results[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results[ResultType::kFileSearch].size(), 2u);
  ASSERT_EQ(results[ResultType::kDriveSearch].size(), 2u);

  ExpectKeywordMultiplier(results, ResultType::kInstalledApp, 1.0);
  ExpectKeywordMultiplier(results, ResultType::kFileSearch, 1.25);
  ExpectKeywordMultiplier(results, ResultType::kDriveSearch, 1.25);
}

/*********************** Fuzzy String Matching Tests ***********************/

// Test that when the input query fuzzy matched one provider's keyword.
TEST_F(KeywordRankerTest, OneTokenMatchOneProvider) {
  // Simulate a query starting.
  ResultsMap results;
  CategoriesList categories;

  // Input the results scores and UpdateResultRanks.
  results[ResultType::kOmnibox] = MakeScoredResults({"omnibox_a"}, {0.7});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"seach", categories);
  ranker.UpdateResultRanks(results, ProviderType::kOmnibox);
  ranker.UpdateResultRanks(results, ProviderType::kFileSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results.size(), 2u);
  ASSERT_EQ(results[ResultType::kOmnibox].size(), 1u);
  ASSERT_EQ(results[ResultType::kFileSearch].size(), 2u);

  // Since fuzzy matching, kOmnibox keyword_multiplier will be more than 1 but
  // less than 1.25.
  ExpectKeywordMultiplier(results, ResultType::kOmnibox, 1.199);
  ExpectKeywordMultiplier(results, ResultType::kFileSearch, 1.0);
}

// Test that when one provider have multiple matched token, it will take the max
// score.
TEST_F(KeywordRankerTest, MultipleTokensMatchOneProvider) {
  // Simulate a query starting.
  ResultsMap results;
  CategoriesList categories;

  // Input the results scores and UpdateResultRanks.
  results[ResultType::kOmnibox] = MakeScoredResults({"omnibox_a"}, {0.7});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"seach search", categories);
  ranker.UpdateResultRanks(results, ProviderType::kOmnibox);
  ranker.UpdateResultRanks(results, ProviderType::kFileSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results.size(), 2u);
  ASSERT_EQ(results[ResultType::kOmnibox].size(), 1u);
  ASSERT_EQ(results[ResultType::kFileSearch].size(), 2u);

  // Since there exist exact match, therefore kOmnibox's keyword_multiplier will
  // be 1.25.
  ExpectKeywordMultiplier(results, ResultType::kOmnibox, 1.25);
  ExpectKeywordMultiplier(results, ResultType::kFileSearch, 1.0);
}

// Test that the one token fuzzy matched multiple providers.
TEST_F(KeywordRankerTest, OneTokenMatchMultipleProviders) {
  // Simulate a query starting.
  ResultsMap results;
  CategoriesList categories;

  // Input the results scores and UpdateResultRanks.
  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});
  results[ResultType::kDriveSearch] =
      MakeScoredResults({"drive_a", "drive_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"fil", categories);
  ranker.UpdateResultRanks(results, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results, ProviderType::kFileSearch);
  ranker.UpdateResultRanks(results, ProviderType::kDriveSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results.size(), 3u);
  ASSERT_EQ(results[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results[ResultType::kFileSearch].size(), 2u);
  ASSERT_EQ(results[ResultType::kDriveSearch].size(), 2u);

  // The misspell of "file" will affect both kFileSearch and kDriveSearch.
  ExpectKeywordMultiplier(results, ResultType::kInstalledApp, 1.0);
  ExpectKeywordMultiplier(results, ResultType::kFileSearch, 1.212);
  ExpectKeywordMultiplier(results, ResultType::kDriveSearch, 1.212);
}

// Test that multiple tokens fuzzy matched/exact matched multiple providers.
TEST_F(KeywordRankerTest, MultipleTokensMatchMultipleProviders) {
  // Simulate a query starting.
  ResultsMap results;
  CategoriesList categories;

  // Input the results scores and UpdateResultRanks.
  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});
  results[ResultType::kDriveSearch] =
      MakeScoredResults({"drive_a", "drive_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"fil drive app", categories);
  ranker.UpdateResultRanks(results, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results, ProviderType::kFileSearch);
  ranker.UpdateResultRanks(results, ProviderType::kDriveSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results.size(), 3u);
  ASSERT_EQ(results[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results[ResultType::kFileSearch].size(), 2u);
  ASSERT_EQ(results[ResultType::kDriveSearch].size(), 2u);

  // The misspell of "file" will affect both kFileSearch and kDriveSearch.
  // However, token "drive" is exact match for kDriveSearch, hence kDriveSearch
  // will have a higher keyword_multiplier.
  ExpectKeywordMultiplier(results, ResultType::kInstalledApp, 1.25);
  ExpectKeywordMultiplier(results, ResultType::kFileSearch, 1.212);
  ExpectKeywordMultiplier(results, ResultType::kDriveSearch, 1.25);
}

/*********************** Train Tests ***********************/

// Test Train() by checking if the keyword_multiplier_ has been boosted up to a
// certain number as the boost_factor_ increased.
TEST_F(KeywordRankerTest, TestTrainBoostFactorIncrease) {
  // Simulate a query starting.
  ResultsMap results;
  CategoriesList categories;

  // Input the results scores and UpdateResultRanks.
  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"app", categories);

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.5});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.7, 0.5});

  ranker.UpdateResultRanks(results, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results, ProviderType::kFileSearch);

  // Check the number of results and the keyword_multiplier of the results.
  ASSERT_EQ(results.size(), 2u);
  ASSERT_EQ(results[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results[ResultType::kFileSearch].size(), 2u);

  // Only kInstalledApp's keyword matched in the input. Train() does not happen
  // yet.
  ExpectKeywordMultiplier(results, ResultType::kInstalledApp, 1.25);
  ExpectKeywordMultiplier(results, ResultType::kFileSearch, 1.0);

  // Train with a boosted result being selected. There are 2 results which score
  // higher than the selected result. Out of the 2 results only 1 is
  // non-boosting. The boost_factor_ will be increase by half of itself.
  ranker.Train(MakeLaunchData("app_b"));
  Wait();

  ranker.Start(u"app", categories);
  ranker.UpdateResultRanks(results, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results, ProviderType::kFileSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results.size(), 2u);
  ASSERT_EQ(results[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results[ResultType::kFileSearch].size(), 2u);

  // Since boost factor being boost up, keyword_multiplier_ should be higher
  // too. That is 1.25 + (0.5 - 0.25)*0.5 = 1.375.
  ExpectKeywordMultiplier(results, ResultType::kInstalledApp, 1.375);
  ExpectKeywordMultiplier(results, ResultType::kFileSearch, 1.0);
}

// Test Train() by checking if the keyword_multiplier_ has been decreased to a
// certain number as the boost_factor_ decreased.
TEST_F(KeywordRankerTest, TestTrainBoostFactorDecrease) {
  // Simulate a query starting.
  ResultsMap results;
  CategoriesList categories;

  // Input the results scores and UpdateResultRanks.
  results[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.4});
  results[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"app", categories);

  // Set the ftrl score of each result from the providers.
  SetFtrlScore(results, ProviderType::kInstalledApp, {0.7, 0.4});
  SetFtrlScore(results, ProviderType::kFileSearch, {0.7, 0.5});

  ranker.UpdateResultRanks(results, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results, ProviderType::kFileSearch);

  // Check the number of results and the keyword_multiplier of the results.
  ASSERT_EQ(results.size(), 2u);
  ASSERT_EQ(results[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results[ResultType::kFileSearch].size(), 2u);

  // Only kInstalledApp's keyword matched in the input. Train() does not happen
  // yet.
  ExpectKeywordMultiplier(results, ResultType::kInstalledApp, 1.25);
  ExpectKeywordMultiplier(results, ResultType::kFileSearch, 1.0);

  // Train with a non boosted result being selected. There are 2 results which
  // score higher than the selected result. Out of the 2 results only 1 is
  // boosting. The boost_factor_ will be decrease by half of itself.
  ranker.Train(MakeLaunchData("local_b"));
  Wait();

  ranker.Start(u"app", categories);
  ranker.UpdateResultRanks(results, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results, ProviderType::kFileSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results.size(), 2u);
  ASSERT_EQ(results[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results[ResultType::kFileSearch].size(), 2u);

  // Since boost factor being boost up, keyword_multiplier_ should be higher
  // too. That is 1.25 - 0.25*0.5 = 1.125.
  ExpectKeywordMultiplier(results, ResultType::kInstalledApp, 1.125);
  ExpectKeywordMultiplier(results, ResultType::kFileSearch, 1.0);
}

}  // namespace app_list::test
