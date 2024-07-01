// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/removed_results_ranker.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/test/test_result.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_test_util.h"
#include "chrome/browser/ash/file_suggest/mock_file_suggest_keyed_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace {

using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

std::unique_ptr<TestResult> MakeResult(const std::string& id) {
  return std::make_unique<TestResult>(id);
}

Results MakeResults(std::vector<std::string> ids) {
  Results results;
  for (const std::string& id : ids) {
    results.push_back(MakeResult(id));
  }
  return results;
}

}  // namespace

class RemovedResultsRankerTest : public testing::Test {
 public:
  // testing::Test:
  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(testing_profile_manager_->SetUp());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_ = testing_profile_manager_->CreateTestingProfile(
        "primary_profile@test",
        {TestingProfile::TestingFactory{
            ash::FileSuggestKeyedServiceFactory::GetInstance(),
            base::BindRepeating(&ash::MockFileSuggestKeyedService::
                                    BuildMockFileSuggestKeyedService,
                                temp_dir_.GetPath().Append("proto"))}});
    ranker_ = std::make_unique<RemovedResultsRanker>(profile_);
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  // Initialize the file suggest service, which holds the underlying proto for
  // all removed results.
  void InitFileService() {
    WaitUntilFileSuggestServiceReady(
        ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
            profile_));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<RemovedResultsRanker> ranker_;
};

TEST_F(RemovedResultsRankerTest, UpdateResultRanks) {
  InitFileService();

  // Request to remove some results.
  ranker_->Remove(MakeResult("A").get());
  ranker_->Remove(MakeResult("C").get());
  ranker_->Remove(MakeResult("E").get());
  Wait();

  ResultsMap results_map;
  results_map[ResultType::kInstalledApp] = MakeResults({"A", "B"});
  results_map[ResultType::kInternalApp] = MakeResults({"C", "D"});
  results_map[ResultType::kOmnibox] = MakeResults({"E"});

  // Installed apps: The 0th result ("A") is marked to be filtered.
  ranker_->UpdateResultRanks(results_map, ResultType::kInstalledApp);
  EXPECT_TRUE(results_map[ResultType::kInstalledApp][0]->scoring().filtered());
  EXPECT_FALSE(results_map[ResultType::kInstalledApp][1]->scoring().filtered());

  // Internal apps: The 0th result ("C") is marked to be filtered.
  ranker_->UpdateResultRanks(results_map, ResultType::kInternalApp);
  EXPECT_TRUE(results_map[ResultType::kInternalApp][0]->scoring().filtered());
  EXPECT_FALSE(results_map[ResultType::kInternalApp][1]->scoring().filtered());

  // Omnibox: The 0th result ("C") is marked to be filtered.
  //
  // TODO(crbug.com/1272361): Ranking here should not affect Omnibox results,
  // after support is added to the autocomplete controller for removal of
  // non-zero state Omnibox results.
  ranker_->UpdateResultRanks(results_map, ResultType::kOmnibox);
  EXPECT_TRUE(results_map[ResultType::kOmnibox][0]->scoring().filtered());
}

TEST_F(RemovedResultsRankerTest, RankEmptyResults) {
  InitFileService();

  ResultsMap results_map;
  results_map[ResultType::kInstalledApp] =
      MakeResults(std::vector<std::string>());

  ranker_->UpdateResultRanks(results_map, ResultType::kInstalledApp);
  EXPECT_TRUE(results_map[ResultType::kInstalledApp].empty());
}

TEST_F(RemovedResultsRankerTest, RankDuplicateResults) {
  InitFileService();

  // Request to remove some results.
  ranker_->Remove(MakeResult("A").get());
  ranker_->Remove(MakeResult("C").get());
  Wait();

  ResultsMap results_map;
  // Include some duplicated results.
  results_map[ResultType::kInstalledApp] = MakeResults({"A", "A", "B"});
  results_map[ResultType::kInternalApp] = MakeResults({"C", "D"});

  // Installed apps: The 0th and 1st results ("A") are marked to be filtered.
  ranker_->UpdateResultRanks(results_map, ResultType::kInstalledApp);
  EXPECT_TRUE(results_map[ResultType::kInstalledApp][0]->scoring().filtered());
  EXPECT_TRUE(results_map[ResultType::kInstalledApp][1]->scoring().filtered());
  EXPECT_FALSE(results_map[ResultType::kInstalledApp][2]->scoring().filtered());

  // Internal apps: The 0th result ("C") is marked to be filtered.
  ranker_->UpdateResultRanks(results_map, ResultType::kInternalApp);
  EXPECT_TRUE(results_map[ResultType::kInternalApp][0]->scoring().filtered());
  EXPECT_FALSE(results_map[ResultType::kInternalApp][1]->scoring().filtered());
}

// Verifies that the ranker removes a result through the file suggest keyed
// service if the result is a file suggestion.
TEST_F(RemovedResultsRankerTest, RemoveFileSuggestions) {
  InitFileService();

  const base::FilePath drive_file_result_path("file_A");
  FileResult drive_file_result(
      "zero_state_drive://" + drive_file_result_path.value(),
      drive_file_result_path, std::nullopt,
      ash::AppListSearchResultType::kZeroStateDrive,
      ash::SearchResultDisplayType::kList, /*relevance=*/0.5f,
      /*query=*/std::u16string(), FileResult::Type::kFile, profile_,
      /*thumbnail_loader=*/nullptr);
  ash::MockFileSuggestKeyedService* mock_service =
      static_cast<ash::MockFileSuggestKeyedService*>(
          ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
              profile_));
  auto drive_file_metadata = drive_file_result.CloneMetadata();
  EXPECT_CALL(*mock_service, RemoveSuggestionBySearchResultAndNotify)
      .WillOnce([&](const ash::SearchResultMetadata& search_result) {
        EXPECT_EQ(search_result.result_type, drive_file_metadata->result_type);
        EXPECT_EQ(search_result.id, drive_file_metadata->id);
      });
  ranker_->Remove(&drive_file_result);

  const base::FilePath local_file_path("file_B");
  FileResult local_file_result(
      "zero_state_file://" + local_file_path.value(), local_file_path,
      std::nullopt, ash::AppListSearchResultType::kZeroStateDrive,
      ash::SearchResultDisplayType::kList, /*relevance=*/0.5f,
      /*query=*/std::u16string(), FileResult::Type::kFile, profile_,
      /*thumbnail_loader=*/nullptr);
  auto local_file_metadata = local_file_result.CloneMetadata();
  EXPECT_CALL(*mock_service, RemoveSuggestionBySearchResultAndNotify)
      .WillOnce([&](const ash::SearchResultMetadata& search_result) {
        EXPECT_EQ(search_result.result_type, local_file_metadata->result_type);
        EXPECT_EQ(search_result.id, local_file_metadata->id);
      });
  ranker_->Remove(&local_file_result);
}

TEST_F(RemovedResultsRankerTest, RemoveBeforeInit) {
  // Don't fully initialize the file service for this test.

  ResultsMap results_map;
  auto apps = MakeResults({"A", "B"});
  apps[0]->SetDisplayType(DisplayType::kRecentApps);
  results_map[ResultType::kInstalledApp] = std::move(apps);
  results_map[ResultType::kOmnibox] = MakeResults({"C", "D"});

  ranker_->UpdateResultRanks(results_map, ResultType::kInstalledApp);
  ranker_->UpdateResultRanks(results_map, ResultType::kOmnibox);

  // All results should be filtered out except for the recent app.
  EXPECT_FALSE(results_map[ResultType::kInstalledApp][0]->scoring().filtered());
  EXPECT_TRUE(results_map[ResultType::kInstalledApp][1]->scoring().filtered());
  EXPECT_TRUE(results_map[ResultType::kOmnibox][0]->scoring().filtered());
  EXPECT_TRUE(results_map[ResultType::kOmnibox][1]->scoring().filtered());
}

}  // namespace app_list::test
