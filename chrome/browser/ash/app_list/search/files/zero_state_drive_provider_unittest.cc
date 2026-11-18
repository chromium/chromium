// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/zero_state_drive_provider.h"

#include "ash/utility/persistent_proto.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/ranking/removed_results.pb.h"
#include "chrome/browser/ash/app_list/search/test/search_results_changed_waiter.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/scoped_test_mount_point.h"
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

using ::ash::holding_space::ScopedTestMountPoint;
using ::testing::DoubleNear;

constexpr double kEpsilon = 0.000001;

class TestFileSuggestKeyedService : public ash::FileSuggestKeyedService {
 public:
  explicit TestFileSuggestKeyedService(Profile* profile,
                                       const base::FilePath& proto_path)
      : FileSuggestKeyedService(
            TestingBrowserProcess::GetGlobal()->local_state(),
            profile,
            ash::PersistentProto<RemovedResultsProto>(proto_path,
                                                      base::TimeDelta())) {}
  TestFileSuggestKeyedService(const TestFileSuggestKeyedService&) = delete;
  TestFileSuggestKeyedService& operator=(TestFileSuggestKeyedService&) = delete;
  ~TestFileSuggestKeyedService() override = default;
  void GetSuggestFileData(ash::FileSuggestionType type,
                          ash::GetSuggestFileDataCallback callback) override {
    if (!IsProtoInitialized()) {
      std::move(callback).Run(/*suggestions=*/std::nullopt);
      return;
    }

    // Emulate `FileSuggestKeyedService` that returns data asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &TestFileSuggestKeyedService::RunGetSuggestFileDataCallback,
            weak_factory_.GetWeakPtr(), type, std::move(callback)));
  }

  void SetSuggestionsForType(
      ash::FileSuggestionType type,
      const std::optional<std::vector<ash::FileSuggestData>>& suggestions) {
    type_suggestion_mappings_[type] = suggestions;
    OnSuggestionProviderUpdated(type);
  }

 private:
  void RunGetSuggestFileDataCallback(ash::FileSuggestionType type,
                                     ash::GetSuggestFileDataCallback callback) {
    std::optional<std::vector<ash::FileSuggestData>> suggestions;
    auto iter = type_suggestion_mappings_.find(type);
    if (iter != type_suggestion_mappings_.end()) {
      suggestions = iter->second;
    }
    FilterRemovedSuggestions(std::move(callback), suggestions);
  }

  // Caches file suggestions.
  std::map<ash::FileSuggestionType,
           std::optional<std::vector<ash::FileSuggestData>>>
      type_suggestion_mappings_;

  base::WeakPtrFactory<TestFileSuggestKeyedService> weak_factory_{this};
};

std::unique_ptr<KeyedService> BuildTestFileSuggestKeyedService(
    const base::FilePath& proto_dir,
    content::BrowserContext* context) {
  return std::make_unique<TestFileSuggestKeyedService>(
      Profile::FromBrowserContext(context), proto_dir);
}

}  // namespace

class ZeroStateDriveProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            TestingBrowserProcess::GetGlobal(),
            base::BindLambdaForTesting(
                [this] { return GetTestingFactories(); })));

    const AccountId account_id = AccountId::FromUserEmailGaiaId(
        "primary_profile@test", GaiaId("123456"));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(account_id));
    user_session_test_environment_->LogIn(account_id);

    profile_ = Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            account_id));
    ASSERT_TRUE(profile_);

    file_suggest_service_ = static_cast<TestFileSuggestKeyedService*>(
        ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
            profile_));
    auto provider = std::make_unique<ZeroStateDriveProvider>(profile_);
    provider_ = provider.get();
    search_controller_.AddProvider(std::move(provider));

    // Initialize the drive file mount point.
    drive_fs_mount_point_ = std::make_unique<ScopedTestMountPoint>(
        /*name=*/"drivefs-delayed_mount", storage::kFileSystemTypeDriveFs,
        file_manager::VOLUME_TYPE_GOOGLE_DRIVE);
    drive_fs_mount_point_->Mount(profile_);
  }

  TestingProfile::TestingFactories GetTestingFactories() {
    return {TestingProfile::TestingFactory{
        ash::FileSuggestKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestFileSuggestKeyedService,
                            temp_dir_.GetPath())}};
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<Profile> profile_ = nullptr;
  TestSearchController search_controller_;
  raw_ptr<ZeroStateDriveProvider> provider_ = nullptr;
  base::HistogramTester histogram_tester_;
  base::ScopedTempDir temp_dir_;
  raw_ptr<TestFileSuggestKeyedService> file_suggest_service_ = nullptr;
  // The mount point for drive files.
  std::unique_ptr<ScopedTestMountPoint> drive_fs_mount_point_;
};

TEST_F(ZeroStateDriveProviderTest, RespondOnDriveFailure) {
  size_t results_update_count = 0u;
  search_controller_.set_results_changed_callback_for_test(
      base::BindLambdaForTesting([&](ash::AppListSearchResultType result_type) {
        ASSERT_EQ(ash::AppListSearchResultType::kZeroStateDrive, result_type);
        ++results_update_count;
      }));

  search_controller_.StartZeroState(base::DoNothing(), base::TimeDelta());

  // The drive file suggest service is expected to fail because it's not fully
  // initialized - the provider is expected to return empty set of results.
  EXPECT_EQ(1u, results_update_count);
  EXPECT_TRUE(search_controller_.last_results().empty());
}

TEST_F(ZeroStateDriveProviderTest, RespondOnSuggestDataFetched) {
  // Emulate that the launcher is open.
  search_controller_.StartZeroState(base::DoNothing(), base::TimeDelta());

  // Creates files and suggests these files through the file suggest keyed
  // service. Returns paths to these files.
  size_t suggestion_size = 3;
  std::vector<ash::FileSuggestData> suggestions;
  for (size_t i = 0; i < suggestion_size; ++i) {
    base::FilePath suggested_file_path =
        drive_fs_mount_point_.get()->CreateArbitraryFile();
    suggestions.emplace_back(ash::FileSuggestionType::kDriveFile,
                             suggested_file_path,
                             /*title=*/std::nullopt,
                             /*new_prediction_reason=*/std::nullopt,
                             /*modified_time=*/std::nullopt,
                             /*viewed_time=*/std::nullopt,
                             /*shared_time=*/std::nullopt,
                             /*new_score=*/std::nullopt,
                             /*drive_file_id=*/std::nullopt,
                             /*icon_url=*/std::nullopt);
  }

  // Only test this logic if the `file_suggest_service_` is ready for test.
  if (file_suggest_service_->IsReadyForTest()) {
    SearchResultsChangedWaiter results_waiter(
        &search_controller_, {ash::AppListSearchResultType::kZeroStateDrive});
    file_suggest_service_->SetSuggestionsForType(
        ash::FileSuggestionType::kDriveFile, suggestions);
    results_waiter.Wait();

    ASSERT_EQ(search_controller_.last_results().size(), suggestion_size);
    // Check the scores to results are assigned by using their position in the
    // results list.
    for (size_t i = 0; i < suggestion_size; ++i) {
      EXPECT_THAT(
          search_controller_.last_results()[i]->relevance(),
          DoubleNear(1.0 - i / static_cast<double>(suggestion_size), kEpsilon));
    }

    // Check the latency is logged.
    const std::string histogram("Apps.AppList.DriveZeroStateProvider.Latency");
    histogram_tester_.ExpectTotalCount(histogram, 1);
  }
}

}  // namespace app_list::test
