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

constexpr float kEps = 1e-3f;

// Helper function that check if the results' multiplier is right.
void ExpectKeywordMultiplier(const ResultsMap& results,
                             ProviderType provider,
                             double keyword_multiplier) {
  const auto it = results.find(provider);
  if (it == results.end()) {
    return;
  }

  for (auto& result : it->second) {
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
  ResultsMap results_1;
  CategoriesList categories_1;

  // Input the results scores and UpdateResultRanks.
  results_1[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  KeywordRanker ranker;
  ranker.Start(u"ABC", results_1, categories_1);
  ranker.UpdateResultRanks(results_1, ProviderType::kInstalledApp);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results_1[ResultType::kInstalledApp].size(), 2u);

  // As no detection of keywords for this result type,
  // therefore the keyword multiplier is down-weighted.
  ExpectKeywordMultiplier(results_1, ResultType::kInstalledApp, 1.0);
}

// Test the input query only match one provider.
TEST_F(KeywordRankerTest, OneMatchedKeyword) {
  // Simulate a query starting.
  ResultsMap results_1;
  CategoriesList categories_1;

  // Input the results scores and UpdateResultRanks.
  results_1[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results_1[ResultType::kHelpApp] =
      MakeScoredResults({"help_a", "help_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"explore", results_1, categories_1);
  ranker.UpdateResultRanks(results_1, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results_1, ProviderType::kHelpApp);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results_1.size(), 2u);
  ASSERT_EQ(results_1[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results_1[ResultType::kHelpApp].size(), 2u);

  ExpectKeywordMultiplier(results_1, ResultType::kInstalledApp, 1.0);
  ExpectKeywordMultiplier(results_1, ResultType::kHelpApp, 1.5);
}

// Test the input query match multiple provider.
TEST_F(KeywordRankerTest, KeywordMatchesMultipleProviders) {
  // Simulate a query starting.
  ResultsMap results_1;
  CategoriesList categories_1;

  // Input the results scores and UpdateResultRanks.
  results_1[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results_1[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});
  results_1[ResultType::kDriveSearch] =
      MakeScoredResults({"drive_a", "drive_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"file", results_1, categories_1);
  ranker.UpdateResultRanks(results_1, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results_1, ProviderType::kFileSearch);
  ranker.UpdateResultRanks(results_1, ProviderType::kDriveSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results_1.size(), 3u);
  ASSERT_EQ(results_1[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results_1[ResultType::kFileSearch].size(), 2u);
  ASSERT_EQ(results_1[ResultType::kDriveSearch].size(), 2u);

  ExpectKeywordMultiplier(results_1, ResultType::kInstalledApp, 1.0);
  ExpectKeywordMultiplier(results_1, ResultType::kFileSearch, 1.5);
  ExpectKeywordMultiplier(results_1, ResultType::kDriveSearch, 1.5);
}

/*********************** Fuzzy String Matching Tests ***********************/

// Test that when the input query fuzzy matched one provider's keyword.
TEST_F(KeywordRankerTest, OneTokenMatchOneProvider) {
  // Simulate a query starting.
  ResultsMap results_1;
  CategoriesList categories_1;

  // Input the results scores and UpdateResultRanks.
  results_1[ResultType::kOmnibox] = MakeScoredResults({"omnibox_a"}, {0.7});
  results_1[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"seach", results_1, categories_1);
  ranker.UpdateResultRanks(results_1, ProviderType::kOmnibox);
  ranker.UpdateResultRanks(results_1, ProviderType::kFileSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results_1.size(), 2u);
  ASSERT_EQ(results_1[ResultType::kOmnibox].size(), 1u);
  ASSERT_EQ(results_1[ResultType::kFileSearch].size(), 2u);

  // Since fuzzy matching, kOmnibox keyword_multiplier will be
  // more than 1 but less than 1.5.
  ExpectKeywordMultiplier(results_1, ResultType::kOmnibox, 1.399);
  ExpectKeywordMultiplier(results_1, ResultType::kFileSearch, 1.0);
}

// Test that when one provider have multiple matched token, it will take
// the max score.
TEST_F(KeywordRankerTest, MultipleTokensMatchOneProvider) {
  // Simulate a query starting.
  ResultsMap results_1;
  CategoriesList categories_1;

  // Input the results scores and UpdateResultRanks.
  results_1[ResultType::kOmnibox] = MakeScoredResults({"omnibox_a"}, {0.7});
  results_1[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"seach search", results_1, categories_1);
  ranker.UpdateResultRanks(results_1, ProviderType::kOmnibox);
  ranker.UpdateResultRanks(results_1, ProviderType::kFileSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results_1.size(), 2u);
  ASSERT_EQ(results_1[ResultType::kOmnibox].size(), 1u);
  ASSERT_EQ(results_1[ResultType::kFileSearch].size(), 2u);

  // Since there exist exact match, therefore kOmnibox's
  // keyword_multiplier will be 1.5.
  ExpectKeywordMultiplier(results_1, ResultType::kOmnibox, 1.5);
  ExpectKeywordMultiplier(results_1, ResultType::kFileSearch, 1.0);
}

// Test that the one token fuzzy matched multiple providers.
TEST_F(KeywordRankerTest, OneTokenMatchMultipleProviders) {
  // Simulate a query starting.
  ResultsMap results_1;
  CategoriesList categories_1;

  // Input the results scores and UpdateResultRanks.
  results_1[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results_1[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});
  results_1[ResultType::kDriveSearch] =
      MakeScoredResults({"drive_a", "drive_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"fil", results_1, categories_1);
  ranker.UpdateResultRanks(results_1, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results_1, ProviderType::kFileSearch);
  ranker.UpdateResultRanks(results_1, ProviderType::kDriveSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results_1.size(), 3u);
  ASSERT_EQ(results_1[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results_1[ResultType::kFileSearch].size(), 2u);
  ASSERT_EQ(results_1[ResultType::kDriveSearch].size(), 2u);

  // The misspell of "file" will affect both kFileSearch and kDriveSearch.
  ExpectKeywordMultiplier(results_1, ResultType::kInstalledApp, 1.0);
  ExpectKeywordMultiplier(results_1, ResultType::kFileSearch, 1.424);
  ExpectKeywordMultiplier(results_1, ResultType::kDriveSearch, 1.424);
}

// Test that multiple tokens fuzzy matched/exact matched multiple providers.
TEST_F(KeywordRankerTest, MultipleTokensMatchMultipleProviders) {
  // Simulate a query starting.
  ResultsMap results_1;
  CategoriesList categories_1;

  // Input the results scores and UpdateResultRanks.
  results_1[ResultType::kInstalledApp] =
      MakeScoredResults({"app_a", "app_b"}, {0.7, 0.5});
  results_1[ResultType::kFileSearch] =
      MakeScoredResults({"local_a", "local_b"}, {0.7, 0.5});
  results_1[ResultType::kDriveSearch] =
      MakeScoredResults({"drive_a", "drive_b"}, {0.7, 0.5});

  KeywordRanker ranker;
  ranker.Start(u"fil drive app", results_1, categories_1);
  ranker.UpdateResultRanks(results_1, ProviderType::kInstalledApp);
  ranker.UpdateResultRanks(results_1, ProviderType::kFileSearch);
  ranker.UpdateResultRanks(results_1, ProviderType::kDriveSearch);

  // Check the number of result and the keyword_multiplier of the result.
  ASSERT_EQ(results_1.size(), 3u);
  ASSERT_EQ(results_1[ResultType::kInstalledApp].size(), 2u);
  ASSERT_EQ(results_1[ResultType::kFileSearch].size(), 2u);
  ASSERT_EQ(results_1[ResultType::kDriveSearch].size(), 2u);

  // The misspell of "file" will affect both kFileSearch and kDriveSearch.
  // However, token "drive" is exact match for kDriveSearch, hence kDriveSearch
  // will have a higher keyword_multiplier.
  ExpectKeywordMultiplier(results_1, ResultType::kInstalledApp, 1.5);
  ExpectKeywordMultiplier(results_1, ResultType::kFileSearch, 1.424);
  ExpectKeywordMultiplier(results_1, ResultType::kDriveSearch, 1.5);
}

}  // namespace app_list::test
