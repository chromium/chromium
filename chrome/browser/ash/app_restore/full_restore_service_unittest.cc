// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/full_restore_service.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::full_restore {

namespace {

constexpr int32_t kWindowId = 100;

void SetPrefValue(const std::string& name,
                  const base::Value& value,
                  sync_pb::PreferenceSpecifics* destination) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  json.Serialize(value);
  destination->set_name(name);
  destination->set_value(serialized);
}

syncer::SyncData CreateRestoreOnStartupPrefSyncData(
    SessionStartupPref::PrefValue value) {
  sync_pb::EntitySpecifics specifics;
  SetPrefValue(::prefs::kRestoreOnStartup, base::Value(static_cast<int>(value)),
               specifics.mutable_preference());
  return syncer::SyncData::CreateRemoteData(
      specifics, syncer::ClientTagHash::FromHashed("unused"));
}

syncer::SyncData CreateRestoreAppsAndPagesPrefSyncData(RestoreOption value) {
  sync_pb::EntitySpecifics specifics;
  SetPrefValue(prefs::kRestoreAppsAndPagesPrefName,
               base::Value(static_cast<int>(value)),
               specifics.mutable_os_preference()->mutable_preference());
  return syncer::SyncData::CreateRemoteData(
      specifics, syncer::ClientTagHash::FromHashed("unused"));
}

// Returns true if the restore pref is 'Always' or 'Ask every time', as we
// could restore apps and pages based on the user's choice from the
// notification for `account_id`. Otherwise, returns false, when the restore
// pref is 'Do not restore'.
bool CanPerformRestore(const AccountId& account_id) {
  return ::app_restore::AppRestoreInfo::GetInstance()->CanPerformRestore(
      account_id);
}

class MockFullRestoreServiceDelegate : public FullRestoreService::Delegate {
 public:
  MockFullRestoreServiceDelegate() = default;
  MockFullRestoreServiceDelegate(const MockFullRestoreServiceDelegate&) =
      delete;
  MockFullRestoreServiceDelegate& operator=(
      const MockFullRestoreServiceDelegate&) = delete;
  ~MockFullRestoreServiceDelegate() override = default;

  MOCK_METHOD(void,
              MaybeStartInformedRestoreOverviewSession,
              (std::unique_ptr<ash::InformedRestoreContentsData> contents_data),
              (override));
  MOCK_METHOD(void, MaybeEndInformedRestoreOverviewSession, (), (override));
  MOCK_METHOD(InformedRestoreContentsData*,
              GetInformedRestoreContentData,
              (),
              (override));
  MOCK_METHOD(void, OnInformedRestoreContentsDataUpdated, (), (override));
};

}  // namespace

class FullRestoreTestHelper {
 public:
  FullRestoreTestHelper(
      const std::string& email,
      const GaiaId& gaia_id,
      FakeChromeUserManager* fake_user_manager,
      TestingProfileManager* profile_manager,
      sync_preferences::TestingPrefServiceSyncable* pref_service) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    account_id_ = AccountId::FromUserEmailGaiaId(email, gaia_id);
    profile_ = profile_manager->CreateTestingProfile(
        email, TestingProfile::TestingFactories());
    fake_user_manager->AddUser(account_id_);
    fake_user_manager->LoginUser(account_id_);
    fake_user_manager->OnUserProfileCreated(account_id_, pref_service);

    ::app_restore::AppRestoreInfo::GetInstance()->SetRestorePref(account_id_,
                                                                 false);
  }
  FullRestoreTestHelper(const FullRestoreTestHelper&) = delete;
  FullRestoreTestHelper& operator=(const FullRestoreTestHelper&) = delete;
  ~FullRestoreTestHelper() = default;

  TestingProfile* profile() { return profile_; }
  const AccountId& account_id() const { return account_id_; }

  void CreateFullRestoreServiceForTesting(
      std::unique_ptr<MockFullRestoreServiceDelegate> mock_delegate) {
    FullRestoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<FullRestoreService>(
              Profile::FromBrowserContext(context));
        }));

    if (mock_delegate) {
      FullRestoreServiceFactory::GetForProfile(profile())->delegate_ =
          std::move(mock_delegate);
    }
  }

  RestoreOption GetRestoreOption() const {
    return static_cast<RestoreOption>(
        profile_->GetPrefs()->GetInteger(prefs::kRestoreAppsAndPagesPrefName));
  }

  void SetRestoreOption(RestoreOption restore_option) {
    profile_->GetPrefs()->SetInteger(prefs::kRestoreAppsAndPagesPrefName,
                                     static_cast<int>(restore_option));
  }

 private:
  raw_ptr<TestingProfile> profile_ = nullptr;
  base::ScopedTempDir temp_dir_;
  AccountId account_id_;
};

class FullRestoreServiceTest : public testing::Test {
 public:
  FullRestoreServiceTest()
      : profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())),
        testing_pref_service_(
            std::make_unique<sync_preferences::TestingPrefServiceSyncable>()) {
    CHECK(profile_manager_->SetUp());
    RegisterUserProfilePrefs(testing_pref_service_->registry());
    test_helper_ = std::make_unique<FullRestoreTestHelper>(
        "usertest@gmail.com", GaiaId("1234567890"), fake_user_manager_.Get(),
        profile_manager_.get(), testing_pref_service_.get());

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kNoFirstRun);
  }
  FullRestoreServiceTest(const FullRestoreServiceTest&) = delete;
  FullRestoreServiceTest& operator=(const FullRestoreServiceTest&) = delete;
  ~FullRestoreServiceTest() override = default;

  FakeChromeUserManager* fake_user_manager() {
    return fake_user_manager_.Get();
  }

  TestingProfile* profile() { return test_helper_->profile(); }
  const AccountId& account_id() const { return test_helper_->account_id(); }

  void CreateFullRestoreServiceForTesting(
      std::unique_ptr<MockFullRestoreServiceDelegate> mock_delegate) {
    test_helper_->CreateFullRestoreServiceForTesting(std::move(mock_delegate));
    content::RunAllTasksUntilIdle();
  }

  void CreateFullRestoreServiceForTesting() {
    CreateFullRestoreServiceForTesting(nullptr);
  }

  RestoreOption GetRestoreOption() const {
    return test_helper_->GetRestoreOption();
  }

  void SetRestoreOption(RestoreOption restore_option) {
    test_helper_->SetRestoreOption(restore_option);
  }

  void VerifyRestoreInitSettingHistogram(RestoreOption option,
                                         base::HistogramBase::Count32 count) {
    histogram_tester_.ExpectUniqueSample("Apps.RestoreInitSetting", option,
                                         count);
  }

  // Simulates the initial sync of preferences.
  void SyncPreferences(
      SessionStartupPref::PrefValue restore_on_startup_value,
      std::optional<RestoreOption> maybe_restore_apps_and_pages_value) {
    syncer::SyncDataList sync_data_list;
    sync_data_list.push_back(
        CreateRestoreOnStartupPrefSyncData(restore_on_startup_value));
    syncer::SyncableService* sync_service =
        profile()->GetTestingPrefService()->GetSyncableService(
            syncer::PREFERENCES);

    if (!maybe_restore_apps_and_pages_value.has_value()) {
      sync_service->MergeDataAndStartSyncing(
          syncer::PREFERENCES, sync_data_list,
          std::make_unique<syncer::FakeSyncChangeProcessor>());

      // OS_PREFERENCES sync should be started separately.
      syncer::SyncableService* os_sync_service =
          profile()->GetTestingPrefService()->GetSyncableService(
              syncer::OS_PREFERENCES);
      os_sync_service->MergeDataAndStartSyncing(
          syncer::OS_PREFERENCES, syncer::SyncDataList(),
          std::make_unique<syncer::FakeSyncChangeProcessor>());
      return;
    }

    syncer::SyncDataList os_sync_data_list;
    os_sync_data_list.push_back(CreateRestoreAppsAndPagesPrefSyncData(
        maybe_restore_apps_and_pages_value.value()));
    syncer::SyncableService* os_sync_service =
        profile()->GetTestingPrefService()->GetSyncableService(
            syncer::OS_PREFERENCES);
    os_sync_service->MergeDataAndStartSyncing(
        syncer::OS_PREFERENCES, os_sync_data_list,
        std::make_unique<syncer::FakeSyncChangeProcessor>());

    sync_service->MergeDataAndStartSyncing(
        syncer::PREFERENCES, sync_data_list,
        std::make_unique<syncer::FakeSyncChangeProcessor>());
  }

  void ProcessSyncChanges(
      SessionStartupPref::PrefValue restore_on_startup_value,
      RestoreOption restore_apps_and_pages_value) {
    syncer::SyncChangeList change_list;
    change_list.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
        CreateRestoreOnStartupPrefSyncData(restore_on_startup_value)));
    syncer::SyncableService* sync_service =
        profile()->GetTestingPrefService()->GetSyncableService(
            syncer::PREFERENCES);
    sync_service->ProcessSyncChanges(FROM_HERE, change_list);

    syncer::SyncChangeList os_change_list;
    os_change_list.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
        CreateRestoreAppsAndPagesPrefSyncData(restore_apps_and_pages_value)));
    syncer::SyncableService* os_sync_service =
        profile()->GetTestingPrefService()->GetSyncableService(
            syncer::OS_PREFERENCES);
    os_sync_service->ProcessSyncChanges(FROM_HERE, os_change_list);
  }

  void TearDown() override {
    fake_user_manager_->OnUserProfileWillBeDestroyed(account_id());
    test_helper_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      testing_pref_service_;
  std::unique_ptr<FullRestoreTestHelper> test_helper_;
  base::HistogramTester histogram_tester_;
};

// If the system is crash, and there is no full restore file, don't show the
// informed restore dialog.
TEST_F(FullRestoreServiceTest, Crash) {
  ExitTypeService::GetInstanceForProfile(profile())
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
}

// If the OS restore setting is 'Ask every time', and there is no full restore
// file, don't show the informed restore dialog.
TEST_F(FullRestoreServiceTest, AskEveryTime) {
  SetRestoreOption(RestoreOption::kAskEveryTime);
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
}

// If the OS restore setting is 'Always', after reboot, don't show the informed
// restore dialog and verify the restore flag.
TEST_F(FullRestoreServiceTest, Always) {
  SetRestoreOption(RestoreOption::kAlways);
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));
  VerifyRestoreInitSettingHistogram(RestoreOption::kAlways, 1);
  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
}

// If the OS restore setting is 'Do not restore', after reboot, don't show the
// informed restore dialog and verify the restore flag.
TEST_F(FullRestoreServiceTest, NotRestore) {
  SetRestoreOption(RestoreOption::kDoNotRestore);
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));
  VerifyRestoreInitSettingHistogram(RestoreOption::kDoNotRestore, 1);
  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(CanPerformRestore(account_id()));
}

// For a brand new user, if sync off, set 'Ask Every Time' as the default value,
// and don't show the informed restore dialog.
TEST_F(FullRestoreServiceTest, NewUserSyncOff) {
  fake_user_manager()->SetIsCurrentUserNew(true);
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 0);
  EXPECT_TRUE(CanPerformRestore(account_id()));
}

// For a new ChromeOS user, if the Chrome restore setting is 'Continue where
// you left off', after sync, set 'Always' as the default value, and don't show
// the informed restore dialog and don't restore.
TEST_F(FullRestoreServiceTest, NewUserSyncChromeRestoreSetting) {
  fake_user_manager()->SetIsCurrentUserNew(true);
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  // Set the Chrome restore setting to simulate sync for the first time.
  SyncPreferences(SessionStartupPref::kPrefValueLast, std::nullopt);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Update the global values to simulate sync from other device.
  ProcessSyncChanges(SessionStartupPref::kPrefValueNewTab,
                     RestoreOption::kDoNotRestore);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(CanPerformRestore(account_id()));
}

// For a new ChromeOS user, if the Chrome restore setting is 'New tab', after
// sync, set 'Ask every time' as the default value, and don't show the informed
// restore dialog and don't restore.
TEST_F(FullRestoreServiceTest, NewUserSyncChromeNotRestoreSetting) {
  fake_user_manager()->SetIsCurrentUserNew(true);
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  // Set the Chrome restore setting to simulate sync for the first time.
  SyncPreferences(SessionStartupPref::kPrefValueNewTab, std::nullopt);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Update the global values to simulate sync from other device.
  ProcessSyncChanges(SessionStartupPref::kPrefValueLast,
                     RestoreOption::kDoNotRestore);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(CanPerformRestore(account_id()));
}

// For a new ChromeOS user, keep the ChromeOS restore setting from sync, and
// don't show the informed restore dialog, and don't restore.
TEST_F(FullRestoreServiceTest, ReImage) {
  fake_user_manager()->SetIsCurrentUserNew(true);
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  // Set the restore pref setting to simulate sync for the first time.
  SyncPreferences(SessionStartupPref::kPrefValueLast,
                  RestoreOption::kAskEveryTime);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Update the global values to simulate sync from other device.
  ProcessSyncChanges(SessionStartupPref::kPrefValueNewTab,
                     RestoreOption::kAlways);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
}

// For the current ChromeOS user, when it is the first time upgrading to the
// full restore release, set the default value based on the current Chrome
// restore setting. Don't show the informed restore dialog and don't restore.
TEST_F(FullRestoreServiceTest, Upgrading) {
  profile()->GetPrefs()->SetInteger(
      ::prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueNewTab));
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kDoNotRestore, 0);
  EXPECT_FALSE(CanPerformRestore(account_id()));

  // Simulate the Chrome restore setting is changed.
  profile()->GetPrefs()->SetInteger(
      ::prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueLast));

  // The OS restore setting should not change.
  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(CanPerformRestore(account_id()));
}

// Full restore is disabled if Floating Workspace is enabled.
TEST_F(FullRestoreServiceTest, NoServiceWithFloatingWorkspace) {
  profile()->GetTestingPrefService()->SetManagedPref(
      ash::prefs::kFloatingWorkspaceV2Enabled,
      std::make_unique<base::Value>(true));
  ASSERT_TRUE(ash::floating_workspace_util::IsFloatingWorkspaceV2Enabled());
  FullRestoreService* service =
      FullRestoreServiceFactory::GetForProfile(profile());
  EXPECT_EQ(nullptr, service);
}

class FullRestoreServiceTestHavingFullRestoreFile
    : public FullRestoreServiceTest {
 public:
  FullRestoreServiceTestHavingFullRestoreFile() {
    CreateRestoreData(profile());
  }
  FullRestoreServiceTestHavingFullRestoreFile(
      const FullRestoreServiceTestHavingFullRestoreFile&) = delete;
  FullRestoreServiceTestHavingFullRestoreFile& operator=(
      const FullRestoreServiceTestHavingFullRestoreFile&) = delete;
  ~FullRestoreServiceTestHavingFullRestoreFile() override {
    ::full_restore::FullRestoreSaveHandler::GetInstance()->ClearForTesting();
  }

  bool allow_save() const {
    return ::full_restore::FullRestoreSaveHandler::GetInstance()->allow_save_;
  }

 protected:
  void CreateRestoreData(Profile* profile) {
    // Add app launch infos.
    ::full_restore::SaveAppLaunchInfo(
        profile->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId));

    ::full_restore::FullRestoreSaveHandler* save_handler =
        ::full_restore::FullRestoreSaveHandler::GetInstance();
    base::OneShotTimer* timer = save_handler->GetTimerForTesting();
    save_handler->AllowSave();

    // Simulate timeout, and the launch info is saved.
    timer->FireNow();

    content::RunAllTasksUntilIdle();

    ::full_restore::FullRestoreReadHandler::GetInstance()
        ->profile_path_to_restore_data_.clear();
  }
};

// If the system is crash, the delegate is notified.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile, Crash) {
  ExitTypeService::GetInstanceForProfile(profile())
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .WillOnce([](std::unique_ptr<InformedRestoreContentsData> data) {
        ASSERT_TRUE(data);
        EXPECT_EQ(InformedRestoreContentsData::DialogType::kCrash,
                  data->dialog_type);
      });
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());
}

// Test that the informed restore dialog is not shown if the previous session
// was crashed, there was full restore data and the restore option is always.
// Regression test for crbug.com/388309832.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile,
       NoInformedRestoreSessionIfCrash) {
  SetRestoreOption(RestoreOption::kAlways);
  ExitTypeService::GetInstanceForProfile(profile())
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));
}

TEST_F(FullRestoreServiceTestHavingFullRestoreFile, AskEveryTimeAndRestore) {
  SetRestoreOption(RestoreOption::kAskEveryTime);
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(1);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());
}

// Test that for an existing user, if re-image, do not show the informed restore
// dialog for the first run.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile, ExistingUserReImage) {
  // Set the restore pref setting to simulate sync for the first time.
  SetRestoreOption(RestoreOption::kAskEveryTime);
  first_run::ResetCachedSentinelDataForTesting();
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kForceFirstRun);

  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(allow_save());

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kForceFirstRun);
  first_run::ResetCachedSentinelDataForTesting();
}

class FullRestoreServiceMultipleUsersTest
    : public FullRestoreServiceTestHavingFullRestoreFile {
 protected:
  FullRestoreServiceMultipleUsersTest() {
    test_helper2_ = std::make_unique<FullRestoreTestHelper>(
        "user2@gmail.com", GaiaId("111111"), fake_user_manager(),
        profile_manager_.get(), testing_pref_service_.get());
    CreateRestoreData(profile2());
  }
  FullRestoreServiceMultipleUsersTest(
      const FullRestoreServiceMultipleUsersTest&) = delete;
  FullRestoreServiceMultipleUsersTest& operator=(
      const FullRestoreServiceMultipleUsersTest&) = delete;
  ~FullRestoreServiceMultipleUsersTest() override = default;

  TestingProfile* profile2() { return test_helper2_->profile(); }
  const AccountId& account_id2() const { return test_helper2_->account_id(); }

  void CreateFullRestoreService2ForTesting() {
    test_helper2_->CreateFullRestoreServiceForTesting(nullptr);
  }

  void CreateFullRestoreService2ForTesting(
      std::unique_ptr<MockFullRestoreServiceDelegate> mock_delegate) {
    test_helper2_->CreateFullRestoreServiceForTesting(std::move(mock_delegate));
    content::RunAllTasksUntilIdle();
  }

  RestoreOption GetRestoreOptionForProfile2() const {
    return test_helper2_->GetRestoreOption();
  }

  void SetRestoreOptionForProfile2(RestoreOption restore_option) {
    test_helper2_->SetRestoreOption(restore_option);
  }

  void TearDown() override {
    fake_user_manager()->OnUserProfileWillBeDestroyed(account_id2());
    test_helper2_.reset();

    FullRestoreServiceTestHavingFullRestoreFile::TearDown();
  }

 private:
  std::unique_ptr<FullRestoreTestHelper> test_helper2_;
};

class ForestFullRestoreServiceMultipleUsersTest
    : public FullRestoreServiceMultipleUsersTest {
 protected:
  ForestFullRestoreServiceMultipleUsersTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kNoFirstRun);
  }
  ForestFullRestoreServiceMultipleUsersTest(
      const ForestFullRestoreServiceMultipleUsersTest&) = delete;
  ForestFullRestoreServiceMultipleUsersTest& operator=(
      const ForestFullRestoreServiceMultipleUsersTest&) = delete;
  ~ForestFullRestoreServiceMultipleUsersTest() override = default;
};

// Verify the full restore init process when 2 users login at the same time,
// e.g. after the system restart or upgrading.
TEST_F(ForestFullRestoreServiceMultipleUsersTest, TwoUsersLoginAtTheSameTime) {
  // Add `switches::kLoginUser` to the command line to simulate the system
  // restart.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kLoginUser, account_id().GetUserEmail());
  // Set the first user as the last session active user.
  fake_user_manager()->set_last_session_active_account_id(account_id());

  SetRestoreOption(RestoreOption::kAskEveryTime);
  SetRestoreOptionForProfile2(RestoreOption::kAskEveryTime);

  // The informed restore dialog is only shown for the first user.
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(1);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  auto mock_delegate_2 = std::make_unique<MockFullRestoreServiceDelegate>();
  auto* mock_delegate_2_ptr = mock_delegate_2.get();
  EXPECT_CALL(*mock_delegate_2,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreService2ForTesting(std::move(mock_delegate_2));

  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());

  // Simulate switch to the second user. The informed restore dialog should be
  // shown for them.
  auto* full_restore_service2 =
      FullRestoreServiceFactory::GetForProfile(profile2());
  EXPECT_CALL(*mock_delegate_2_ptr,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(1);
  full_restore_service2->OnTransitionedToNewActiveUser(profile2());

  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 2);
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOptionForProfile2());
  EXPECT_TRUE(CanPerformRestore(account_id2()));
  EXPECT_TRUE(allow_save());
}

// Verify the full restore init process when 2 users login one by one.
TEST_F(ForestFullRestoreServiceMultipleUsersTest, TwoUsersLoginOneByOne) {
  SetRestoreOption(RestoreOption::kAskEveryTime);
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(1);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());

  SetRestoreOptionForProfile2(RestoreOption::kAskEveryTime);
  auto mock_delegate_2 = std::make_unique<MockFullRestoreServiceDelegate>();
  auto* mock_delegate_2_ptr = mock_delegate_2.get();
  EXPECT_CALL(*mock_delegate_2,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreService2ForTesting(std::move(mock_delegate_2));

  // Simulate switch to the second user. The informed restore dialog should be
  // shown for them.
  auto* full_restore_service2 =
      FullRestoreServiceFactory::GetForProfile(profile2());
  EXPECT_CALL(*mock_delegate_2_ptr,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(1);
  full_restore_service2->OnTransitionedToNewActiveUser(profile2());

  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 2);
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOptionForProfile2());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());
}

// Verify the full restore init process when the system restarts.
TEST_F(ForestFullRestoreServiceMultipleUsersTest,
       TwoUsersLoginWithActiveUserLogin) {
  // Add `switches::kLoginUser` to the command line to simulate the system
  // restart.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kLoginUser, account_id().GetUserEmail());
  // Set the second user as the last session active user.
  fake_user_manager()->set_last_session_active_account_id(account_id2());

  SetRestoreOption(RestoreOption::kAskEveryTime);
  SetRestoreOptionForProfile2(RestoreOption::kAskEveryTime);

  // The informed restore dialog shouldn't be shown for either user yet.
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  auto* mock_delegate_ptr = mock_delegate.get();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  auto mock_delegate_2 = std::make_unique<MockFullRestoreServiceDelegate>();
  auto* mock_delegate_2_ptr = mock_delegate_2.get();
  EXPECT_CALL(*mock_delegate_2,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  CreateFullRestoreService2ForTesting(std::move(mock_delegate_2));

  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 0);

  // Simulate switch to the second user. The informed restore dialog should be
  // shown for them.
  auto* full_restore_service2 =
      FullRestoreServiceFactory::GetForProfile(profile2());
  EXPECT_CALL(*mock_delegate_2_ptr,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(1);
  full_restore_service2->OnTransitionedToNewActiveUser(profile2());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);
  EXPECT_TRUE(CanPerformRestore(account_id2()));

  // Simulate switch to the first user. The informed restore dialog should be
  // shown for them.
  auto* full_restore_service =
      FullRestoreServiceFactory::GetForProfile(profile());
  EXPECT_CALL(*mock_delegate_ptr,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(1);
  full_restore_service->OnTransitionedToNewActiveUser(profile());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 2);
  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Simulate switch to the second user, and verify no more init processes.
  EXPECT_CALL(*mock_delegate_2_ptr,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  full_restore_service2->OnTransitionedToNewActiveUser(profile2());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 2);

  // Simulate switch to the first user, and verify no more init processes.
  EXPECT_CALL(*mock_delegate_ptr,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(0);
  full_restore_service->OnTransitionedToNewActiveUser(profile());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 2);
}

}  // namespace ash::full_restore
