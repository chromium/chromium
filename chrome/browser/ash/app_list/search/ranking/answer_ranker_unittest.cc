// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/answer_ranker.h"

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/test/test_result.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

Results MakeOmniboxCandidates(std::vector<double> relevances) {
  Results results;
  for (const double relevance : relevances) {
    // |id| and |normalized_relevance| must be set but are not used.
    results.push_back(std::make_unique<TestResult>(
        /*id=*/"", relevance,
        /*normalized_relevance=*/0.0, ash::SearchResultDisplayType::kAnswerCard,
        false));
  }
  return results;
}

Results MakeShortcutCandidates(std::vector<bool> best_matches) {
  Results results;
  for (const double best_match : best_matches) {
    // |id| and |normalized_relevance| must be set but are not used.
    results.push_back(std::make_unique<TestResult>(
        /*id=*/"",
        /*relevance=*/1, /*normalized_relevance=*/0.0,
        ash::SearchResultDisplayType::kList, best_match));
  }
  return results;
}

bool AnswerFieldsAreSet(const std::unique_ptr<ChromeSearchResult>& result) {
  return result->display_type() == ash::SearchResultDisplayType::kAnswerCard &&
         result->multiline_title() &&
         result->icon().dimension == kAnswerCardIconDimension &&
         !result->scoring().filter;
}

}  // namespace

// Tests that the best Omnibox answer is selected and all others are filtered
// out.
TEST(AnswerRankerTest, SelectAndFilterOmnibox) {
  ResultsMap results_map;
  results_map[ResultType::kOmnibox] = MakeOmniboxCandidates({0.3, 0.5, 0.4});

  AnswerRanker ranker;
  ranker.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ranker.OnBurnInPeriodElapsed();

  const auto& results = results_map[ResultType::kOmnibox];
  ASSERT_EQ(results.size(), 3u);

  // The highest scoring Omnibox answer is selected.
  EXPECT_TRUE(AnswerFieldsAreSet(results[1]));

  // Others are filtered out.
  EXPECT_TRUE(results[0]->scoring().filter);
  EXPECT_TRUE(results[2]->scoring().filter);
}

// Tests that a best match shortcut is selected.
TEST(AnswerRankerTest, SelectBestShortcut) {
  ResultsMap results_map;
  results_map[ResultType::kKeyboardShortcut] =
      MakeShortcutCandidates({false, true});

  AnswerRanker ranker;
  ranker.UpdateResultRanks(results_map, ProviderType::kKeyboardShortcut);
  ranker.OnBurnInPeriodElapsed();

  const auto& results = results_map[ResultType::kKeyboardShortcut];
  ASSERT_EQ(results.size(), 2u);

  // The best match shortcut is selected.
  EXPECT_TRUE(AnswerFieldsAreSet(results[1]));

  EXPECT_NE(results[0]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);
}

// Tests that no shortcut answers are selected if there are multiple best
// matches.
TEST(AnswerRankerTest, OnlySelectIfOneBestShortcut) {
  ResultsMap results_map;
  results_map[ResultType::kKeyboardShortcut] =
      MakeShortcutCandidates({true, true});

  AnswerRanker ranker;
  ranker.UpdateResultRanks(results_map, ProviderType::kKeyboardShortcut);
  ranker.OnBurnInPeriodElapsed();

  const auto& results = results_map[ResultType::kKeyboardShortcut];
  ASSERT_EQ(results.size(), 2u);

  // No shortcuts should be selected.
  for (const auto& result : results) {
    EXPECT_NE(result->display_type(),
              ash::SearchResultDisplayType::kAnswerCard);
  }
}

// Tests that Omnibox answers take priority over Shortcuts.
TEST(AnswerRankerTest, OmniboxOverShortcuts) {
  ResultsMap results_map;
  results_map[ResultType::kOmnibox] = MakeOmniboxCandidates({0.4});
  results_map[ResultType::kKeyboardShortcut] = MakeShortcutCandidates({true});

  AnswerRanker ranker;
  ranker.UpdateResultRanks(results_map, ProviderType::kKeyboardShortcut);
  ranker.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ranker.OnBurnInPeriodElapsed();

  // Shortcut candidate should not be selected.
  const auto& shortcut_results = results_map[ResultType::kKeyboardShortcut];
  ASSERT_EQ(shortcut_results.size(), 1u);
  EXPECT_NE(shortcut_results[0]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);

  // Omnibox candidate should be selected.
  const auto& omnibox_results = results_map[ResultType::kOmnibox];
  ASSERT_EQ(omnibox_results.size(), 1u);
  EXPECT_TRUE(AnswerFieldsAreSet(omnibox_results[0]));
}

// Tests that a chosen answer is not changed after burn-in.
TEST(AnswerRankerTest, SelectedAnswerNotChangedAfterBurnIn) {
  ResultsMap results_map;
  results_map[ResultType::kKeyboardShortcut] = MakeShortcutCandidates({true});

  AnswerRanker ranker;
  ranker.UpdateResultRanks(results_map, ProviderType::kKeyboardShortcut);
  ranker.OnBurnInPeriodElapsed();

  // The shortcut answer is selected.
  const auto& shortcut_results = results_map[ResultType::kKeyboardShortcut];
  ASSERT_EQ(shortcut_results.size(), 1u);
  EXPECT_TRUE(AnswerFieldsAreSet(shortcut_results[0]));

  // New Omnibox candidates should still be filtered out.
  results_map[ResultType::kOmnibox] = MakeOmniboxCandidates({0.5});
  ranker.UpdateResultRanks(results_map, ProviderType::kOmnibox);

  const auto& omnibox_results = results_map[ResultType::kOmnibox];
  ASSERT_EQ(omnibox_results.size(), 1u);
  EXPECT_TRUE(omnibox_results[0]->scoring().filter);
}

}  // namespace app_list::test
