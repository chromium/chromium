// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/filtering_ranker.h"

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/test/test_result.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom-forward.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace {

using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;
using AnswerType = ::crosapi::mojom::SearchResult::AnswerType;

class TestDriveIdResult : public TestResult {
 public:
  TestDriveIdResult(const std::string& id,
                    ResultType type,
                    const absl::optional<std::string>& drive_id)
      : TestResult(id, type), drive_id_(drive_id) {}

  ~TestDriveIdResult() override = default;

  // ChromeSearchResult:
  void Open(int event_flags) override {}

  absl::optional<std::string> DriveId() const override { return drive_id_; }

 private:
  absl::optional<std::string> drive_id_;
};

Results MakeDriveIdResults(
    const std::vector<std::string>& ids,
    const std::vector<ResultType>& types,
    const std::vector<absl::optional<std::string>>& drive_ids) {
  CHECK_EQ(ids.size(), types.size());
  CHECK_EQ(ids.size(), drive_ids.size());

  Results res;
  for (size_t i = 0; i < ids.size(); ++i) {
    res.push_back(
        std::make_unique<TestDriveIdResult>(ids[i], types[i], drive_ids[i]));
  }
  return res;
}

Results MakeOmniboxResults(const std::vector<std::string>& ids,
                           const std::vector<ResultType>& types,
                           const std::vector<AnswerType>& answer_types) {
  CHECK_EQ(ids.size(), types.size());
  CHECK_EQ(ids.size(), answer_types.size());

  Results res;
  for (size_t i = 0; i < ids.size(); ++i) {
    res.push_back(std::make_unique<TestResult>(
        ids[i], types[i], answer_types[i], DisplayType::kAnswerCard));
  }
  return res;
}

}  // namespace

class FilteringRankerTest : public testing::Test {};

TEST_F(FilteringRankerTest, DeduplicateDriveFilesAndTabs) {
  auto drive = ResultType::kDriveSearch;
  auto web = ResultType::kOmnibox;
  auto tab = ResultType::kOpenTab;

  ResultsMap results;
  results[web] = MakeDriveIdResults({"a", "b", "c", "d"}, {web, tab, tab, tab},
                                    {absl::nullopt, "B", "C", absl::nullopt});
  results[drive] = MakeDriveIdResults({"a", "b", "d", "e", "f"},
                                      {drive, drive, drive, drive, drive},
                                      {"A", "B", "D", "E", absl::nullopt});

  FilteringRanker ranker;
  CategoriesList categories;
  ranker.Start(u"query", results, categories);
  ranker.UpdateResultRanks(results, ProviderType::kKeyboardShortcut);

  EXPECT_FALSE(results[drive][0]->scoring().filter);
  EXPECT_TRUE(results[drive][1]->scoring().filter);
  EXPECT_FALSE(results[drive][2]->scoring().filter);
  EXPECT_FALSE(results[drive][3]->scoring().filter);
  EXPECT_FALSE(results[drive][4]->scoring().filter);
}

// Test that answers of certain kinds (that tend to over-trigger) aren't shown
// on very short queries.
TEST_F(FilteringRankerTest, FilterOmniboxResults) {
  auto web = ResultType::kOmnibox;
  auto tab = ResultType::kOpenTab;
  ResultsMap results;

  results[web] = MakeOmniboxResults(
      {"a", "b", "c", "d", "e"}, {web, web, tab, web, web},
      {AnswerType::kFinance, AnswerType::kTranslation, AnswerType::kUnset,
       AnswerType::kDictionary, AnswerType::kCalculator});

  FilteringRanker ranker;
  CategoriesList categories;

  // Start with a query that is one character too short.
  ranker.Start(std::u16string(kMinQueryLengthForCommonAnswers - 1, 'a'),
               results, categories);
  ranker.UpdateResultRanks(results, ProviderType::kOmnibox);

  // All results except dictionary and translate answers are allowed.
  EXPECT_FALSE(results[web][0]->scoring().filter);
  EXPECT_TRUE(results[web][1]->scoring().filter);
  EXPECT_FALSE(results[web][2]->scoring().filter);
  EXPECT_TRUE(results[web][3]->scoring().filter);
  EXPECT_FALSE(results[web][4]->scoring().filter);
}

}  // namespace app_list::test
