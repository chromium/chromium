// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/mrfu_ranker.h"

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/test/ranking_test_util.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

using testing::ElementsAre;

}  // namespace

// MrfuResultRanker ------------------------------------------------------------

class MrfuResultRankerTest : public RankerTestBase {};

TEST_F(MrfuResultRankerTest, TrainAndRank) {
  MrfuResultRanker ranker(MrfuCache::Params(),
                          MrfuCache::Proto(GetPath(), base::Seconds(0)));
  Wait();

  // Train on some results.
  ranker.Train(MakeLaunchData("a"));
  ranker.Train(MakeLaunchData("b"));
  ranker.Train(MakeLaunchData("c"));
  ranker.Train(MakeLaunchData("a"));

  ResultsMap results;
  results[ResultType::kInstalledApp] = MakeResults({"a", "b"});
  results[ResultType::kOsSettings] = MakeResults({"c", "d"});
  CategoriesList categories;

  // Rank them, expecting ordering a > c > b > d.
  ranker.Start(u"query", categories);
  auto ab_scores = ranker.GetResultRanks(results, ResultType::kInstalledApp);
  auto cd_scores = ranker.GetResultRanks(results, ResultType::kOsSettings);
  EXPECT_GT(ab_scores[0], cd_scores[0]);
  EXPECT_GT(cd_scores[0], ab_scores[1]);
  EXPECT_GT(ab_scores[1], cd_scores[1]);
}

// MrfuCategoryRanker ----------------------------------------------------------

class MrfuCategoryRankerTest : public RankerTestBase {};

TEST_F(MrfuCategoryRankerTest, DefaultCategoryScores) {
  MrfuCategoryRanker ranker(MrfuCache::Params(),
                            MrfuCache::Proto(GetPath(), base::Seconds(0)));
  Wait();

  ResultsMap results;
  // The following categories of results have default category scores.
  results[ResultType::kInstalledApp] =
      MakeResults({"apps"}, ResultType::kInstalledApp, Category::kApps);
  results[ResultType::kPlayStoreApp] = MakeResults(
      {"playstore"}, ResultType::kPlayStoreApp, Category::kPlayStore);
  results[ResultType::kOsSettings] =
      MakeResults({"settings"}, ResultType::kOsSettings, Category::kSettings);
  results[ResultType::kOmnibox] =
      MakeResults({"omnibox"}, ResultType::kOmnibox, Category::kWeb);
  results[ResultType::kFileSearch] =
      MakeResults({"files"}, ResultType::kFileSearch, Category::kFiles);
  results[ResultType::kKeyboardShortcut] = MakeResults(
      {"shortcuts"}, ResultType::kKeyboardShortcut, Category::kHelp);

  // The following categories of results do not have default category scores.
  results[ResultType::kAssistantText] = MakeResults(
      {"g"}, ResultType::kAssistantText, Category::kSearchAndAssistant);
  results[ResultType::kArcAppShortcut] =
      MakeResults({"h"}, ResultType::kArcAppShortcut, Category::kAppShortcuts);

  CategoriesList categories({{.category = Category::kApps},
                             {.category = Category::kPlayStore},
                             {.category = Category::kSettings},
                             {.category = Category::kWeb},
                             {.category = Category::kFiles},
                             {.category = Category::kHelp},
                             {.category = Category::kSearchAndAssistant},
                             {.category = Category::kAppShortcuts}});
  ranker.Start(u"query", categories);
  auto scores =
      ranker.GetCategoryRanks(results, categories, ResultType::kInstalledApp);
  ASSERT_EQ(scores.size(), 8u);

  // Expect the first six categories of |categories| to be ranked in descending
  // order.
  EXPECT_GT(scores[0], scores[1]);
  EXPECT_GT(scores[1], scores[2]);
  EXPECT_GT(scores[2], scores[3]);
  EXPECT_GT(scores[3], scores[4]);
  EXPECT_GT(scores[4], scores[5]);

  // Expect the remaining two categories to be ranked below the first six. Note
  // that the default ranking between these two is not defined.
  EXPECT_GT(scores[5], scores[6]);
  EXPECT_GT(scores[5], scores[7]);
}

TEST_F(MrfuCategoryRankerTest, TrainAndRank) {
  MrfuCategoryRanker ranker(MrfuCache::Params(),
                            MrfuCache::Proto(GetPath(), base::Seconds(0)));
  Wait();

  // Train so that settings should be first, followed by apps.
  ranker.Train(MakeLaunchData("a", Category::kApps));
  ranker.Train(MakeLaunchData("c", Category::kSettings));
  ranker.Train(MakeLaunchData("d", Category::kSettings));
  ranker.Train(MakeLaunchData("b", Category::kApps));

  ResultsMap results;
  results[ResultType::kInstalledApp] =
      MakeResults({"a", "b"}, ResultType::kInstalledApp, Category::kApps);
  results[ResultType::kOsSettings] =
      MakeResults({"c", "d"}, ResultType::kOsSettings, Category::kSettings);
  results[ResultType::kFileSearch] =
      MakeResults({"e", "f"}, ResultType::kFileSearch, Category::kFiles);
  CategoriesList categories({{.category = Category::kApps},
                             {.category = Category::kSettings},
                             {.category = Category::kFiles}});

  // Expect a ranking of kInstalledApp > kOsSettings > kFileSearch.
  ranker.Start(u"query", categories);
  auto scores =
      ranker.GetCategoryRanks(results, categories, ResultType::kInstalledApp);
  ASSERT_EQ(scores.size(), 3u);
  EXPECT_GT(scores[0], scores[1]);
  EXPECT_GT(scores[1], scores[2]);
}

}  // namespace app_list::test
