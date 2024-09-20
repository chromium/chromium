// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/drive_recent_file_suggestion_provider.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/file_manager/mount_test_util.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_test_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using drivefs::mojom::QueryParameters;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Pointee;

constexpr char kEmail[] = "test-user@example.com";
constexpr char16_t kEmail16[] = u"test-user@example.com";

struct QueryItemInfo {
  base::FilePath path;

  std::optional<bool> is_folder;

  base::Time last_modified_time;
  std::optional<base::Time> modified_by_me_time;
  std::optional<std::string> last_modifying_user;

  base::Time last_viewed_by_me_time;

  std::optional<base::Time> shared_with_me_time;
  std::optional<std::string> sharing_user;
};

struct SuggestionInfo {
  SuggestionInfo(const base::FilePath& path,
                 const std::u16string& justification)
      : path(path), justification(justification) {}
  explicit SuggestionInfo(const ash::FileSuggestData& suggestion)
      : path(suggestion.file_path),
        justification(suggestion.prediction_reason.value_or(u"null")) {
    EXPECT_EQ(ash::FileSuggestionType::kDriveFile, suggestion.type);
  }

  base::FilePath path;
  std::u16string justification;
};

bool operator==(const SuggestionInfo& lhs, const SuggestionInfo& rhs) {
  return lhs.path == rhs.path && lhs.justification == rhs.justification;
}

std::ostream& operator<<(std::ostream& out, const SuggestionInfo& suggestion) {
  out << "{\" path: " << suggestion.path << ", "
      << "justification: " << suggestion.justification << "}";
  return out;
}

std::vector<drivefs::mojom::QueryItemPtr> CreateQueryItems(
    const std::vector<QueryItemInfo>& items) {
  std::vector<drivefs::mojom::QueryItemPtr> results;
  for (const auto& item : items) {
    auto result = drivefs::mojom::QueryItem::New();
    result->path = item.path;
    result->metadata = drivefs::mojom::FileMetadata::New();
    result->metadata->modification_time = item.last_modified_time;
    result->metadata->modified_by_me_time = item.modified_by_me_time;
    result->metadata->last_viewed_by_me_time = item.last_viewed_by_me_time;
    if (item.last_modifying_user) {
      result->metadata->last_modifying_user = drivefs::mojom::UserInfo::New();
      result->metadata->last_modifying_user->display_name =
          *item.last_modifying_user;
    }
    result->metadata->shared_with_me_time = item.shared_with_me_time;
    if (item.sharing_user) {
      result->metadata->sharing_user = drivefs::mojom::UserInfo::New();
      result->metadata->sharing_user->display_name = *item.sharing_user;
    }
    result->metadata->type =
        item.is_folder.value_or(false)
            ? drivefs::mojom::FileMetadata::Type::kDirectory
            : drivefs::mojom::FileMetadata::Type::kFile;
    result->metadata->capabilities = drivefs::mojom::Capabilities::New();
    results.push_back(std::move(result));
  }
  return results;
}

base::Time GetReferenceTime() {
  base::Time time;
  EXPECT_TRUE(base::Time::FromString("Tue, 5 Dec 2023 13:30:00", &time));
  return time;
}

}  // namespace

namespace ash {

class FakeSearchQuery : public drivefs::mojom::SearchQuery {
 public:
  explicit FakeSearchQuery(std::vector<drivefs::mojom::QueryItemPtr> results)
      : results_(std::move(results)) {}

  FakeSearchQuery(const FakeSearchQuery&) = delete;
  FakeSearchQuery& operator=(const FakeSearchQuery&) = delete;
  ~FakeSearchQuery() override = default;

  void GetNextPage(GetNextPageCallback callback) override {
    if (should_fail_) {
      std::move(callback).Run(drive::FILE_ERROR_FAILED, {});
      return;
    }
    if (next_page_called_) {
      std::move(callback).Run(drive::FILE_ERROR_OK, {});
      return;
    }
    next_page_called_ = true;
    std::move(callback).Run(drive::FILE_ERROR_OK, std::move(results_));
  }

  void SetToFail() { should_fail_ = true; }

 private:
  std::vector<drivefs::mojom::QueryItemPtr> results_;
  bool next_page_called_ = false;
  bool should_fail_ = false;
};

class DriveRecentFileSuggestionProviderTest : public ::testing::Test {
 public:
  DriveRecentFileSuggestionProviderTest() {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kLauncherContinueSectionWithRecentsRollout},
        {ash::features::kShowSharingUserInLauncherContinueSection});
  }
  DriveRecentFileSuggestionProviderTest(
      const DriveRecentFileSuggestionProviderTest&) = delete;
  DriveRecentFileSuggestionProviderTest& operator=(
      const DriveRecentFileSuggestionProviderTest&) = delete;
  ~DriveRecentFileSuggestionProviderTest() override = default;

  // ::testing::Test:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ash::CrosDisksClient::InitializeFake();

    disk_mount_manager_ = new disks::FakeDiskMountManager();
    disks::DiskMountManager::InitializeForTesting(disk_mount_manager_);

    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile(
        kEmail, /*prefs=*/{}, kEmail16,
        /*avatar_id=*/0,
        {TestingProfile::TestingFactory{
            drive::DriveIntegrationServiceFactory::GetInstance(),
            base::BindRepeating(&DriveRecentFileSuggestionProviderTest::
                                    BuildTestDriveIntegrationService,
                                base::Unretained(this))}});

    AccountId account_id =
        AccountId::FromUserEmailGaiaId(profile_->GetProfileUserName(), "12345");
    fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id, /*is_affiliated=*/false, user_manager::UserType::kRegular,
        profile_.get());
    fake_user_manager_->LoginUser(account_id, true);

    WaitUntilFileSuggestServiceReady(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile_));
  }

  void TearDown() override {
    integration_service_ = nullptr;
    fake_drivefs_helper_.reset();
    profile_ = nullptr;
    profile_manager_.reset();
    disk_mount_manager_ = nullptr;
    disks::DiskMountManager::Shutdown();
    CrosDisksClient::Shutdown();
  }

  std::unique_ptr<KeyedService> BuildTestDriveIntegrationService(
      content::BrowserContext* context) {
    const std::string mount_point_name = "drivefs";
    const base::FilePath mount_point_path =
        temp_dir_.GetPath().Append("drivefs");

    disk_mount_manager_->RegisterMountPointForNetworkStorageScheme(
        "drivefs", mount_point_path.value());
    fake_drivefs_helper_ = std::make_unique<drive::FakeDriveFsHelper>(
        Profile::FromBrowserContext(context), mount_point_path);
    auto service = std::make_unique<drive::DriveIntegrationService>(
        Profile::FromBrowserContext(context), mount_point_name,
        base::FilePath(),
        fake_drivefs_helper_->CreateFakeDriveFsListenerFactory());
    integration_service_ = service.get();
    return service;
  }

  void EnableDriveAndWaitForMountPoint() {
    drive::DriveIntegrationServiceFactory::GetInstance()
        ->GetForProfile(profile())
        ->SetEnabled(true);
    file_manager::test_util::WaitUntilDriveMountPointIsAdded(profile());
  }

  void SetUpInvalidDriveMountPoint() {
    ASSERT_TRUE(!integration_service_ || !integration_service_->IsMounted());

    disk_mount_manager_->RegisterMountPointForNetworkStorageScheme("drivefs",
                                                                   "<invalid>");
  }

  Profile* profile() { return profile_; }

  drivefs::FakeDriveFs* fake_drivefs() {
    return &fake_drivefs_helper_->fake_drivefs();
  }

  base::FilePath GetDriveRoot() const {
    return fake_drivefs_helper_->mount_path();
  }

 private:
  base::ScopedTempDir temp_dir_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;

  raw_ptr<disks::FakeDiskMountManager> disk_mount_manager_ = nullptr;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
  raw_ptr<drive::DriveIntegrationService> integration_service_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;
};

class DriveRecentFileSuggestionProviderWithSharingUserTest
    : public DriveRecentFileSuggestionProviderTest {
 public:
  DriveRecentFileSuggestionProviderWithSharingUserTest() {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kLauncherContinueSectionWithRecentsRollout,
         ash::features::kShowSharingUserInLauncherContinueSection},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that file suggest service returns empty drive suggestions when drive
// is disabled.
TEST_F(DriveRecentFileSuggestionProviderTest, DriveDisabled) {
  drive::DriveIntegrationServiceFactory::GetInstance()
      ->GetForProfile(profile())
      ->SetEnabled(false);

  EXPECT_CALL(*fake_drivefs(), StartSearchQuery).Times(0);

  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                EXPECT_TRUE(data.has_value());
                EXPECT_EQ(data->size(), 0u);
                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service returns no suggestions when drive is not
// mounted.
TEST_F(DriveRecentFileSuggestionProviderTest, DriveNotMounted) {
  SetUpInvalidDriveMountPoint();

  drive::DriveIntegrationServiceFactory::GetInstance()
      ->GetForProfile(profile())
      ->SetEnabled(true);

  EXPECT_CALL(*fake_drivefs(), StartSearchQuery).Times(0);

  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                EXPECT_FALSE(data);
                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service returns recently modified or viewed files
// sorted by their timestamp, and with correct justification string.
TEST_F(DriveRecentFileSuggestionProviderTest,
       SearchRecentlyModifiedAndViewedFiles) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last item 1"),
              .last_modified_time = GetReferenceTime(),
              .modified_by_me_time = GetReferenceTime() - base::Hours(12),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1)},
             {.path = base::FilePath("/Folder last modified"),
              .is_folder = true,
              .last_modified_time = GetReferenceTime() - base::Hours(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1)},
             {.path = base::FilePath("/Modified and viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1)},
             {.path = base::FilePath("/Modified last item 2"),
              .last_modified_time = GetReferenceTime() - base::Days(3),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(3)}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item 1"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Hours(6)},
             {.path = base::FilePath("/Viewed folder"),
              .is_folder = true,
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Hours(7)},
             {.path = base::FilePath("/Modified and viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Hours(12),
              .last_viewed_by_me_time = GetReferenceTime() - base::Hours(12)},
             {.path = base::FilePath("/Viewed last item 2"),
              .last_modified_time = GetReferenceTime() - base::Days(4),
              .last_viewed_by_me_time =
                  GetReferenceTime() - base::Hours(60)}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<FakeSearchQuery>(CreateQueryItems({})),
            std::move(receiver));
      });

  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                ASSERT_TRUE(data);

                std::vector<SuggestionInfo> actual_suggestions;
                for (const auto& suggestion : data.value()) {
                  actual_suggestions.emplace_back(suggestion);
                }

                const base::FilePath root = GetDriveRoot();
                EXPECT_THAT(
                    actual_suggestions,
                    ElementsAre(
                        SuggestionInfo(root.Append("Modified last item 1"),
                                       u"Edited · just now"),
                        SuggestionInfo(root.Append("Viewed last item 1"),
                                       u"You opened · 7:30 AM"),
                        SuggestionInfo(
                            root.Append("Modified and viewed last item"),
                            u"Edited · Dec 4"),
                        SuggestionInfo(root.Append("Viewed last item 2"),
                                       u"You opened · Dec 3"),
                        SuggestionInfo(root.Append("Modified last item 2"),
                                       u"Edited · Dec 2")));

                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service returns recently modified or viewed files
// sorted by their timestamp, and with correct justification string in case
// recently modified files metadata contains modifying user information.
TEST_F(DriveRecentFileSuggestionProviderTest, ModifyingUserInfo) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last by user"),
              .last_modified_time = GetReferenceTime(),
              .modified_by_me_time = GetReferenceTime(),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Hours(1)},
             {.path = base::FilePath("/Modified last by someone else"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .modified_by_me_time = GetReferenceTime() - base::Days(2),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1)},
             {.path = base::FilePath("/No modified by me time"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(2)},
             {.path = base::FilePath("/No last modifying user info"),
              .last_modified_time = GetReferenceTime() - base::Days(3),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(3)}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .modified_by_me_time = GetReferenceTime() - base::Days(1),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Hours(1)}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<FakeSearchQuery>(CreateQueryItems({})),
            std::move(receiver));
      });

  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                ASSERT_TRUE(data);

                std::vector<SuggestionInfo> actual_suggestions;
                for (const auto& suggestion : data.value()) {
                  actual_suggestions.emplace_back(suggestion);
                }

                const base::FilePath root = GetDriveRoot();
                EXPECT_THAT(
                    actual_suggestions,
                    ElementsAre(
                        SuggestionInfo(root.Append("Modified last by user"),
                                       u"You edited · just now"),
                        SuggestionInfo(root.Append("Viewed last item"),
                                       u"You opened · 12:30 PM"),
                        SuggestionInfo(
                            root.Append("Modified last by someone else"),
                            u"Test User edited · Dec 4"),
                        SuggestionInfo(root.Append("No modified by me time"),
                                       u"Test User edited · Dec 4"),
                        SuggestionInfo(
                            root.Append("No last modifying user info"),
                            u"Edited · Dec 2")));

                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service correctly classifies files shared with
// user that have never been viewed by the user as "shared". This test has the
// feature flag to surface sharing user info disabled, so justification strings
// do not contain sharing user information.
TEST_F(DriveRecentFileSuggestionProviderTest, SharedItems) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last, viewed by user"),
              .last_modified_time = GetReferenceTime(),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1),
              .shared_with_me_time = GetReferenceTime() - base::Days(2)},
             {.path = base::FilePath("/Modified last by user"),
              .last_modified_time = GetReferenceTime() - base::Minutes(2),
              .modified_by_me_time = GetReferenceTime() - base::Minutes(2),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1),
              .shared_with_me_time = GetReferenceTime() - base::Hours(56)},
             {.path = base::FilePath("/Modified last, not viewed by user"),
              .last_modified_time = GetReferenceTime() - base::Minutes(3),
              .last_modifying_user = "Test User",
              .shared_with_me_time = GetReferenceTime() - base::Minutes(5)},
             {.path = base::FilePath("/Shared with sharing user info"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"},
             {.path = base::FilePath("/Old shared file"),
              .last_modified_time = GetReferenceTime() - base::Days(100),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(100),
              .sharing_user = "Test User 2"},
             {.path = base::FilePath("/Old shared file viewed by user"),
              .last_modified_time = GetReferenceTime() - base::Days(100),
              .last_modifying_user = "Test User 1",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(99),
              .shared_with_me_time = GetReferenceTime() - base::Days(100),
              .sharing_user = "Test User 2"}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .modified_by_me_time = GetReferenceTime() - base::Days(1),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Minutes(5),
              .shared_with_me_time = GetReferenceTime() - base::Days(2)}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last, viewed by user"),
              .last_modified_time = GetReferenceTime(),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1),
              .shared_with_me_time = GetReferenceTime() - base::Days(2)},
             {.path = base::FilePath("/Shared"),
              .last_modified_time = GetReferenceTime() - base::Minutes(2),
              .last_modifying_user = "Test User",
              .shared_with_me_time = GetReferenceTime() - base::Hours(50)},
             {.path = base::FilePath("/Shared folder"),
              .is_folder = true,
              .last_modified_time = GetReferenceTime() - base::Minutes(2),
              .last_modifying_user = "Test User",
              .shared_with_me_time = GetReferenceTime() - base::Hours(50)},
             {.path = base::FilePath("/Modified last by user"),
              .last_modified_time = GetReferenceTime() - base::Minutes(2),
              .modified_by_me_time = GetReferenceTime() - base::Minutes(2),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1),
              .shared_with_me_time = GetReferenceTime() - base::Hours(56)},
             {.path = base::FilePath("/Shared with sharing user info"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });

  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                ASSERT_TRUE(data);

                std::vector<SuggestionInfo> actual_suggestions;
                for (const auto& suggestion : data.value()) {
                  actual_suggestions.emplace_back(suggestion);
                }

                const base::FilePath root = GetDriveRoot();
                EXPECT_THAT(
                    actual_suggestions,
                    ElementsAre(
                        SuggestionInfo(root.Append("Modified last by user"),
                                       u"You edited · just now"),
                        SuggestionInfo(root.Append("Viewed last item"),
                                       u"You opened · just now"),
                        SuggestionInfo(
                            root.Append("Modified last, viewed by user"),
                            u"Test User edited · just now"),
                        SuggestionInfo(
                            root.Append("Modified last, not viewed by user"),
                            u"Shared with you · just now"),
                        SuggestionInfo(root.Append("Shared"),
                                       u"Shared with you · Dec 3"),
                        SuggestionInfo(
                            root.Append("Shared with sharing user info"),
                            u"Shared with you · Dec 2")));

                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service correctly classifies files shared with
// user that have never been viewed by the user as "shared". This test has the
// feature flag to surface sharing user info enabled, so justification strings
// should contain sharing user information.
TEST_F(DriveRecentFileSuggestionProviderWithSharingUserTest, SharedItems) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last, viewed by user"),
              .last_modified_time = GetReferenceTime(),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1),
              .shared_with_me_time = GetReferenceTime() - base::Days(2)},
             {.path = base::FilePath("/Modified last by user"),
              .last_modified_time = GetReferenceTime() - base::Minutes(2),
              .modified_by_me_time = GetReferenceTime() - base::Minutes(2),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1),
              .shared_with_me_time = GetReferenceTime() - base::Hours(56)},
             {.path = base::FilePath("/Modified last, not viewed by user"),
              .last_modified_time = GetReferenceTime() - base::Minutes(3),
              .last_modifying_user = "Test User",
              .shared_with_me_time = GetReferenceTime() - base::Minutes(5)},
             {.path = base::FilePath("/Shared with sharing user info"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .modified_by_me_time = GetReferenceTime() - base::Days(1),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Minutes(5),
              .shared_with_me_time = GetReferenceTime() - base::Days(2)}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last, viewed by user"),
              .last_modified_time = GetReferenceTime(),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1),
              .shared_with_me_time = GetReferenceTime() - base::Days(2)},
             {.path = base::FilePath("/Shared"),
              .last_modified_time = GetReferenceTime() - base::Minutes(2),
              .last_modifying_user = "Test User",
              .shared_with_me_time = GetReferenceTime() - base::Hours(50)},
             {.path = base::FilePath("/Modified last by user"),
              .last_modified_time = GetReferenceTime() - base::Minutes(2),
              .modified_by_me_time = GetReferenceTime() - base::Minutes(2),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1),
              .shared_with_me_time = GetReferenceTime() - base::Hours(56)},
             {.path = base::FilePath("/Shared with sharing user info"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                ASSERT_TRUE(data);

                std::vector<SuggestionInfo> actual_suggestions;
                for (const auto& suggestion : data.value()) {
                  actual_suggestions.emplace_back(suggestion);
                }

                const base::FilePath root = GetDriveRoot();
                EXPECT_THAT(
                    actual_suggestions,
                    ElementsAre(
                        SuggestionInfo(root.Append("Modified last by user"),
                                       u"You edited · just now"),
                        SuggestionInfo(root.Append("Viewed last item"),
                                       u"You opened · just now"),
                        SuggestionInfo(
                            root.Append("Modified last, viewed by user"),
                            u"Test User edited · just now"),
                        SuggestionInfo(
                            root.Append("Modified last, not viewed by user"),
                            u"Shared with you · just now"),
                        SuggestionInfo(root.Append("Shared"),
                                       u"Shared with you · Dec 3"),
                        SuggestionInfo(
                            root.Append("Shared with sharing user info"),
                            u"Test User 2 shared · Dec 2")));

                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service returns recently modified or viewed files
// sorted by their timestamp, and with correct justification string. Verifies
// that missing last viewed time, or modification time timestamps are handled
// correctly.
TEST_F(DriveRecentFileSuggestionProviderTest,
       SearchResultsWithSomeTimestampsMissing) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last item 1"),
              .last_modified_time = GetReferenceTime(),
              .modified_by_me_time = GetReferenceTime()},
             {.path = base::FilePath("/Modified and viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item 1"),
              .last_viewed_by_me_time = GetReferenceTime() - base::Hours(16)},
             {.path = base::FilePath("/Modified and viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1)},
             {.path = base::FilePath("/Viewed last item 2"),
              .last_viewed_by_me_time =
                  GetReferenceTime() - base::Hours(60)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<FakeSearchQuery>(CreateQueryItems({})),
            std::move(receiver));
      });

  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                ASSERT_TRUE(data);

                std::vector<SuggestionInfo> actual_suggestions;
                for (const auto& suggestion : data.value()) {
                  actual_suggestions.emplace_back(suggestion);
                }

                const base::FilePath root = GetDriveRoot();
                EXPECT_THAT(
                    actual_suggestions,
                    ElementsAre(
                        SuggestionInfo(root.Append("Modified last item 1"),
                                       u"You edited · just now"),
                        SuggestionInfo(root.Append("Viewed last item 1"),
                                       u"You opened · Dec 4"),
                        SuggestionInfo(
                            root.Append("Modified and viewed last item"),
                            u"Edited · Dec 4"),
                        SuggestionInfo(root.Append("Viewed last item 2"),
                                       u"You opened · Dec 3")));

                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service can handle recent drive files with
// modification/view timestamps from the future.
TEST_F(DriveRecentFileSuggestionProviderTest, TimestampsInFuture) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last by user"),
              .last_modified_time = GetReferenceTime() + base::Minutes(1),
              .modified_by_me_time = GetReferenceTime() + base::Minutes(1),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() + base::Hours(1)},
             {.path = base::FilePath("/Modified last by someone else"),
              .last_modified_time = GetReferenceTime() + base::Days(2),
              .modified_by_me_time = GetReferenceTime() + base::Days(1),
              .last_modifying_user = "Test User 1",
              .last_viewed_by_me_time = GetReferenceTime() + base::Days(2)},
             {.path = base::FilePath("/No modified by me time"),
              .last_modified_time = GetReferenceTime() + base::Minutes(1),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() + base::Minutes(2)},
             {.path = base::FilePath("/No last modifying user info"),
              .last_modified_time = GetReferenceTime() + base::Days(3),
              .last_viewed_by_me_time = GetReferenceTime() + base::Days(3)}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item"),
              .last_modified_time = GetReferenceTime() + base::Hours(26),
              .modified_by_me_time = GetReferenceTime() + base::Hours(26),
              .last_modifying_user = "Test User",
              .last_viewed_by_me_time = GetReferenceTime() + base::Days(2)}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<FakeSearchQuery>(CreateQueryItems({})),
            std::move(receiver));
      });

  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                ASSERT_TRUE(data);

                std::vector<SuggestionInfo> actual_suggestions;
                for (const auto& suggestion : data.value()) {
                  actual_suggestions.emplace_back(suggestion);
                }

                const base::FilePath root = GetDriveRoot();
                EXPECT_THAT(
                    actual_suggestions,
                    ElementsAre(
                        SuggestionInfo(root.Append("Viewed last item"),
                                       u"You opened · Dec 7"),
                        SuggestionInfo(
                            root.Append("Modified last by someone else"),
                            u"Test User 1 edited · Dec 7"),
                        SuggestionInfo(root.Append("Modified last by user"),
                                       u"You opened · 2:30 PM"),
                        SuggestionInfo(
                            root.Append("No last modifying user info"),
                            u"Edited · Dec 8"),
                        SuggestionInfo(root.Append("No modified by me time"),
                                       u"You opened · just now")));
                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service returns empty suggestions if drive search
// requests fail.
TEST_F(DriveRecentFileSuggestionProviderTest, DriveFailedSearch) {
  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(), StartSearchQuery)
      .Times(3)
      .WillRepeatedly(
          [&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
              drivefs::mojom::QueryParametersPtr query_params) {
            EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                      query_params->query_source);
            EXPECT_EQ(
                drivefs::mojom::QueryParameters::SortDirection::kDescending,
                query_params->sort_direction);

            auto search_query = std::make_unique<FakeSearchQuery>(
                std::vector<drivefs::mojom::QueryItemPtr>());
            search_query->SetToFail();
            mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                        std::move(receiver));
          });

  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                ASSERT_TRUE(data);
                EXPECT_TRUE(data->empty());
                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service returns recently modified files if only
// search for recently viewed files fails.
TEST_F(DriveRecentFileSuggestionProviderTest, LastViewedSearchFailed) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last item 1"),
              .last_modified_time = GetReferenceTime(),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1)},
             {.path = base::FilePath("/Modified and viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(2)},
             {.path = base::FilePath("/Modified last item 2"),
              .last_modified_time = GetReferenceTime() - base::Days(3),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(3)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(
            std::vector<drivefs::mojom::QueryItemPtr>());
        search_query->SetToFail();

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Shared with sharing user info"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });

  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                ASSERT_TRUE(data);

                std::vector<SuggestionInfo> actual_suggestions;
                for (const auto& suggestion : data.value()) {
                  actual_suggestions.emplace_back(suggestion);
                }

                const base::FilePath root = GetDriveRoot();
                EXPECT_THAT(
                    actual_suggestions,
                    ElementsAre(
                        SuggestionInfo(root.Append("Modified last item 1"),
                                       u"Edited · just now"),
                        SuggestionInfo(
                            root.Append("Modified and viewed last item"),
                            u"Edited · Dec 4"),
                        SuggestionInfo(root.Append("Modified last item 2"),
                                       u"Edited · Dec 2"),
                        SuggestionInfo(
                            root.Append("Shared with sharing user info"),
                            u"Shared with you · Dec 2")));

                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service returns recently viewed files if only
// search for recently modified files fails.
TEST_F(DriveRecentFileSuggestionProviderTest, ModifiedTimeSearchFailed) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(
            std::vector<drivefs::mojom::QueryItemPtr>());
        search_query->SetToFail();
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item 1"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Hours(12)},
             {.path = base::FilePath("/Modified and viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(1)},
             {.path = base::FilePath("/Viewed last item 2"),
              .last_modified_time = GetReferenceTime() - base::Days(4),
              .last_viewed_by_me_time =
                  GetReferenceTime() - base::Hours(60)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Shared with sharing user info"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });

  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                ASSERT_TRUE(data);

                std::vector<SuggestionInfo> actual_suggestions;
                for (const auto& suggestion : data.value()) {
                  actual_suggestions.emplace_back(suggestion);
                }

                const base::FilePath root = GetDriveRoot();
                EXPECT_THAT(
                    actual_suggestions,
                    ElementsAre(
                        SuggestionInfo(root.Append("Viewed last item 1"),
                                       u"You opened · 1:30 AM"),
                        SuggestionInfo(
                            root.Append("Modified and viewed last item"),
                            u"Edited · Dec 4"),
                        SuggestionInfo(root.Append("Viewed last item 2"),
                                       u"You opened · Dec 3"),
                        SuggestionInfo(
                            root.Append("Shared with sharing user info"),
                            u"Shared with you · Dec 2")));

                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service returns recently viewed files if only
// search for files recently shared with user fails.
TEST_F(DriveRecentFileSuggestionProviderTest, SharedWithMeSearchFailed) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Hours(36)},
             {.path = base::FilePath("/Modified and viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(2),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(2)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Hours(12)},
             {.path = base::FilePath("/Modified and viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(2),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(2)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(
            std::vector<drivefs::mojom::QueryItemPtr>());
        search_query->SetToFail();

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });

  base::RunLoop result_waiter;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce(base::BindLambdaForTesting(
              [&](const std::optional<std::vector<FileSuggestData>>& data) {
                ASSERT_TRUE(data);

                std::vector<SuggestionInfo> actual_suggestions;
                for (const auto& suggestion : data.value()) {
                  actual_suggestions.emplace_back(suggestion);
                }

                const base::FilePath root = GetDriveRoot();
                EXPECT_THAT(
                    actual_suggestions,
                    ElementsAre(
                        SuggestionInfo(root.Append("Viewed last item"),
                                       u"You opened · 1:30 AM"),
                        SuggestionInfo(root.Append("Modified last item"),
                                       u"Edited · Dec 4"),
                        SuggestionInfo(
                            root.Append("Modified and viewed last item"),
                            u"Edited · Dec 3")));

                result_waiter.Quit();
              })));
  result_waiter.Run();
}

// Verifies that file suggest service issues only one set of search requests if
// suggetions are requested if no file changes are detected after the first
// request.
TEST_F(DriveRecentFileSuggestionProviderTest, SequentialRequests) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(4)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime()}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Shared"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });

  auto* const suggest_service =
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile());

  base::RunLoop result_waiter_1;
  suggest_service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindOnce(base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& data) {
            ASSERT_TRUE(data);

            std::vector<SuggestionInfo> actual_suggestions;
            for (const auto& suggestion : data.value()) {
              actual_suggestions.emplace_back(suggestion);
            }

            const base::FilePath root = GetDriveRoot();
            EXPECT_THAT(
                actual_suggestions,
                ElementsAre(SuggestionInfo(root.Append("Viewed last item"),
                                           u"You opened · just now"),
                            SuggestionInfo(root.Append("Modified last item"),
                                           u"Edited · Dec 4"),
                            SuggestionInfo(root.Append("Shared"),
                                           u"Shared with you · Dec 2")));

            result_waiter_1.Quit();
          })));

  result_waiter_1.Run();

  base::RunLoop result_waiter_2;
  suggest_service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindOnce(base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& data) {
            ASSERT_TRUE(data);

            std::vector<SuggestionInfo> actual_suggestions;
            for (const auto& suggestion : data.value()) {
              actual_suggestions.emplace_back(suggestion);
            }

            const base::FilePath root = GetDriveRoot();
            EXPECT_THAT(
                actual_suggestions,
                ElementsAre(SuggestionInfo(root.Append("Viewed last item"),
                                           u"You opened · just now"),
                            SuggestionInfo(root.Append("Modified last item"),
                                           u"Edited · Dec 4"),
                            SuggestionInfo(root.Append("Shared"),
                                           u"Shared with you · Dec 2")));

            result_waiter_2.Quit();
          })));

  result_waiter_2.Run();
}
// Verifies that file suggest service can be called repeatedly. It verifies that
// suggestions are refreshed if requested after previous request finished if
// there was a file change between requests.
TEST_F(DriveRecentFileSuggestionProviderTest,
       SequentialSearchesAfterFileChange) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last item 1"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(2)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      })
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last item 2"),
              .last_modified_time = GetReferenceTime() - base::Days(2),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(4)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item 1"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime()}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      })
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item 2"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime()}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Shared 1"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      })
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Shared 2"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });

  auto* const suggest_service =
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile());

  base::RunLoop result_waiter_1;
  suggest_service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindOnce(base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& data) {
            ASSERT_TRUE(data);

            std::vector<SuggestionInfo> actual_suggestions;
            for (const auto& suggestion : data.value()) {
              actual_suggestions.emplace_back(suggestion);
            }

            const base::FilePath root = GetDriveRoot();
            EXPECT_THAT(
                actual_suggestions,
                ElementsAre(SuggestionInfo(root.Append("Viewed last item 1"),
                                           u"You opened · just now"),
                            SuggestionInfo(root.Append("Modified last item 1"),
                                           u"Edited · Dec 4"),
                            SuggestionInfo(root.Append("Shared 1"),
                                           u"Shared with you · Dec 2")));

            result_waiter_1.Quit();
          })));
  result_waiter_1.Run();

  // Notify a drive fs change so the cached results get invalidated.
  std::vector<drivefs::mojom::FileChangePtr> changes;
  changes.emplace_back(std::in_place, base::FilePath("/Viewed last item 2"),
                       drivefs::mojom::FileChange::Type::kCreate);
  changes.emplace_back(std::in_place, base::FilePath("/Viewed last item 1"),
                       drivefs::mojom::FileChange::Type::kDelete);
  fake_drivefs()->delegate()->OnFilesChanged(mojo::Clone(changes));
  fake_drivefs()->delegate().FlushForTesting();

  base::RunLoop result_waiter_2;
  suggest_service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindOnce(base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& data) {
            ASSERT_TRUE(data);

            std::vector<SuggestionInfo> actual_suggestions;
            for (const auto& suggestion : data.value()) {
              actual_suggestions.emplace_back(suggestion);
            }

            const base::FilePath root = GetDriveRoot();
            EXPECT_THAT(
                actual_suggestions,
                ElementsAre(SuggestionInfo(root.Append("Viewed last item 2"),
                                           u"You opened · just now"),
                            SuggestionInfo(root.Append("Modified last item 2"),
                                           u"Edited · Dec 3"),
                            SuggestionInfo(root.Append("Shared 2"),
                                           u"Shared with you · Dec 2")));

            result_waiter_2.Quit();
          })));
  result_waiter_2.Run();
}

// Verifies that file suggest service can be called repeatedly. It verifies that
// suggestions are refreshed if requested after previous request finished drive
// fs was reset between requests.
TEST_F(DriveRecentFileSuggestionProviderTest, SequentialSearchesAfterRemount) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last item 1"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(2)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      })
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last item 2"),
              .last_modified_time = GetReferenceTime() - base::Days(2),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(4)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item 1"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime()}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      })
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item 2"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime()}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Shared 1"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      })
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Shared 2"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });

  auto* const suggest_service =
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile());

  base::RunLoop result_waiter_1;
  suggest_service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindOnce(base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& data) {
            ASSERT_TRUE(data);

            std::vector<SuggestionInfo> actual_suggestions;
            for (const auto& suggestion : data.value()) {
              actual_suggestions.emplace_back(suggestion);
            }

            const base::FilePath root = GetDriveRoot();
            EXPECT_THAT(
                actual_suggestions,
                ElementsAre(SuggestionInfo(root.Append("Viewed last item 1"),
                                           u"You opened · just now"),
                            SuggestionInfo(root.Append("Modified last item 1"),
                                           u"Edited · Dec 4"),
                            SuggestionInfo(root.Append("Shared 1"),
                                           u"Shared with you · Dec 2")));

            result_waiter_1.Quit();
          })));
  result_waiter_1.Run();

  drive::DriveIntegrationServiceFactory::GetInstance()
      ->GetForProfile(profile())
      ->SetEnabled(false);
  EnableDriveAndWaitForMountPoint();

  base::RunLoop result_waiter_2;
  suggest_service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindOnce(base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& data) {
            ASSERT_TRUE(data);

            std::vector<SuggestionInfo> actual_suggestions;
            for (const auto& suggestion : data.value()) {
              actual_suggestions.emplace_back(suggestion);
            }

            const base::FilePath root = GetDriveRoot();
            EXPECT_THAT(
                actual_suggestions,
                ElementsAre(SuggestionInfo(root.Append("Viewed last item 2"),
                                           u"You opened · just now"),
                            SuggestionInfo(root.Append("Modified last item 2"),
                                           u"Edited · Dec 3"),
                            SuggestionInfo(root.Append("Shared 2"),
                                           u"Shared with you · Dec 2")));

            result_waiter_2.Quit();
          })));
  result_waiter_2.Run();
}
// Verifies that file suggest service issues only one set of search requests if
// suggetions are requested while the last request is still in progress.
TEST_F(DriveRecentFileSuggestionProviderTest, ConcurrentRequests) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  EnableDriveAndWaitForMountPoint();

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Modified last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(4)}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });
  EXPECT_CALL(
      *fake_drivefs(),
      StartSearchQuery(
          _, Pointee(Field(&QueryParameters::sort_field,
                           QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Viewed last item"),
              .last_modified_time = GetReferenceTime() - base::Days(1),
              .last_viewed_by_me_time = GetReferenceTime()}}));

        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });

  EXPECT_CALL(*fake_drivefs(),
              StartSearchQuery(
                  _, Pointee(Field(&QueryParameters::sort_field,
                                   QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        EXPECT_EQ(drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
                  query_params->query_source);
        EXPECT_EQ(drivefs::mojom::QueryParameters::SortDirection::kDescending,
                  query_params->sort_direction);

        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = base::FilePath("/Shared"),
              .last_modified_time = GetReferenceTime() - base::Hours(26),
              .last_modifying_user = "Test User 1",
              .shared_with_me_time = GetReferenceTime() - base::Days(3),
              .sharing_user = "Test User 2"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(receiver));
      });

  auto* const suggest_service =
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile());

  base::RunLoop result_waiter_1;
  suggest_service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindOnce(base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& data) {
            ASSERT_TRUE(data);

            std::vector<SuggestionInfo> actual_suggestions;
            for (const auto& suggestion : data.value()) {
              actual_suggestions.emplace_back(suggestion);
            }

            const base::FilePath root = GetDriveRoot();
            EXPECT_THAT(
                actual_suggestions,
                ElementsAre(SuggestionInfo(root.Append("Viewed last item"),
                                           u"You opened · just now"),
                            SuggestionInfo(root.Append("Modified last item"),
                                           u"Edited · Dec 4"),
                            SuggestionInfo(root.Append("Shared"),
                                           u"Shared with you · Dec 2")));

            result_waiter_1.Quit();
          })));

  base::RunLoop result_waiter_2;
  suggest_service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindOnce(base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& data) {
            ASSERT_TRUE(data);

            std::vector<SuggestionInfo> actual_suggestions;
            for (const auto& suggestion : data.value()) {
              actual_suggestions.emplace_back(suggestion);
            }

            const base::FilePath root = GetDriveRoot();
            EXPECT_THAT(
                actual_suggestions,
                ElementsAre(SuggestionInfo(root.Append("Viewed last item"),
                                           u"You opened · just now"),
                            SuggestionInfo(root.Append("Modified last item"),
                                           u"Edited · Dec 4"),
                            SuggestionInfo(root.Append("Shared"),
                                           u"Shared with you · Dec 2")));

            result_waiter_2.Quit();
          })));

  result_waiter_1.Run();
  result_waiter_2.Run();
}

}  // namespace ash
