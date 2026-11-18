// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/removed_results_ranker.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/check_deref.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/test/test_result.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_test_util.h"
#include "chrome/browser/ash/file_suggest/mock_file_suggest_keyed_service.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace {

constexpr char kEmail[] = "primary_profile@test";

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
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            TestingBrowserProcess::GetGlobal(), base::BindLambdaForTesting([&] {
              return TestingProfile::TestingFactories{
                  TestingProfile::TestingFactory{
                      ash::FileSuggestKeyedServiceFactory::GetInstance(),
                      base::BindRepeating(
                          &ash::MockFileSuggestKeyedService::
                              BuildMockFileSuggestKeyedService,
                          temp_dir_.GetPath().Append("proto"))}};
            })));

    AccountId account_id =
        AccountId::FromUserEmailGaiaId(kEmail, GaiaId("123456"));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(account_id));
    user_session_test_environment_->LogIn(account_id);

    profile_ = Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            account_id));
    ASSERT_TRUE(profile_);

    ranker_ = std::make_unique<RemovedResultsRanker>(&user());
  }

  void TearDown() override {
    ranker_.reset();
    profile_ = nullptr;
    user_session_test_environment_.reset();
    testing::Test::TearDown();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  // Initialize the file suggest service, which holds the underlying proto for
  // all removed results.
  void InitFileService() {
    WaitUntilFileSuggestServiceReady(
        ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
            profile_));
  }

  user_manager::User& user() {
    return CHECK_DEREF(
        ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile_));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  // TODO(crbug.com/569954060): Replace with a stored user_manager::User*
  // (returned by user_session_test_environment_->AddRegularUser()) now
  // that this test's User is no longer derived from `profile_` via a
  // reverse lookup.
  raw_ptr<Profile> profile_ = nullptr;
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
  results_map[ResultType::kOmnibox] = MakeResults({"E"});

  // Installed apps: The 0th result ("A") is marked to be filtered.
  ranker_->UpdateResultRanks(results_map, ResultType::kInstalledApp);
  EXPECT_TRUE(results_map[ResultType::kInstalledApp][0]->scoring().filtered());
  EXPECT_FALSE(results_map[ResultType::kInstalledApp][1]->scoring().filtered());

  // Omnibox: The 0th result ("C") is marked to be filtered.
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

  // Installed apps: The 0th and 1st results ("A") are marked to be filtered.
  ranker_->UpdateResultRanks(results_map, ResultType::kInstalledApp);
  EXPECT_TRUE(results_map[ResultType::kInstalledApp][0]->scoring().filtered());
  EXPECT_TRUE(results_map[ResultType::kInstalledApp][1]->scoring().filtered());
  EXPECT_FALSE(results_map[ResultType::kInstalledApp][2]->scoring().filtered());
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
