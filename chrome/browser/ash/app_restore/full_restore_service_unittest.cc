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
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

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
      const std::string& gaia_id,
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
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());
  }
  FullRestoreTestHelper(const FullRestoreTestHelper&) = delete;
  FullRestoreTestHelper& operator=(const FullRestoreTestHelper&) = delete;
  ~FullRestoreTestHelper() = default;

  TestingProfile* profile() { return profile_; }
  const AccountId& account_id() const { return account_id_; }

  bool HasNotificationFor(const std::string& notification_id) const {
    return display_service_->GetNotification(notification_id).has_value();
  }

  void VerifyRestoreNotificationTitle(const std::string& notification_id,
                                      bool is_reboot_notification) {
    std::optional<message_center::Notification> message_center_notification =
        display_service_->GetNotification(notification_id);
    ASSERT_TRUE(message_center_notification.has_value());
    const int message_id = is_reboot_notification
                               ? IDS_POLICY_DEVICE_POST_REBOOT_TITLE
                               : IDS_RESTORE_NOTIFICATION_TITLE;
    EXPECT_EQ(message_center_notification->title(),
              l10n_util::GetStringUTF16(message_id));
  }

  void VerifyNotification(bool has_crash_notification,
                          bool has_restore_notification,
                          bool is_reboot_notification = false) {
    EXPECT_EQ(has_crash_notification,
              HasNotificationFor(kRestoreForCrashNotificationId));
    EXPECT_EQ(has_restore_notification,
              HasNotificationFor(kRestoreNotificationId));
    if (has_restore_notification) {
      VerifyRestoreNotificationTitle(kRestoreNotificationId,
                                     is_reboot_notification);
    }
  }

  void SimulateClick(const std::string& notification_id,
                     RestoreNotificationButtonIndex action_index) {
    display_service_->SimulateClick(
        NotificationHandler::Type::TRANSIENT, notification_id,
        static_cast<int>(action_index), std::nullopt);
  }

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
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

// TODO(http://b/326982900): Remove non-forest test suites when the feature flag
// is removed.
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
        "usertest@gmail.com", "1234567890", fake_user_manager_.Get(),
        profile_manager_.get(), testing_pref_service_.get());
    scoped_feature_list_.InitAndDisableFeature(features::kForestFeature);
  }
  FullRestoreServiceTest(const FullRestoreServiceTest&) = delete;
  FullRestoreServiceTest& operator=(const FullRestoreServiceTest&) = delete;
  ~FullRestoreServiceTest() override = default;

  FakeChromeUserManager* fake_user_manager() {
    return fake_user_manager_.Get();
  }

  TestingProfile* profile() { return test_helper_->profile(); }
  const AccountId& account_id() const { return test_helper_->account_id(); }

  void VerifyNotification(bool has_crash_notification,
                          bool has_restore_notification,
                          bool is_reboot_notification = false) {
    test_helper_->VerifyNotification(has_crash_notification,
                                     has_restore_notification,
                                     is_reboot_notification);
  }

  void SimulateClick(const std::string& notification_id,
                     RestoreNotificationButtonIndex action_index) {
    test_helper_->SimulateClick(notification_id, action_index);
  }

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
                                         base::HistogramBase::Count count) {
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
  base::test::ScopedFeatureList scoped_feature_list_;
};

// If the system is crash, and there is no FullRestore file, don't show the
// crash notification, and don't restore.
TEST_F(FullRestoreServiceTest, Crash) {
  ExitTypeService::GetInstanceForProfile(profile())
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  CreateFullRestoreServiceForTesting();

  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
}

// If the OS restore setting is 'Ask every time', and there is no FullRestore
// file, after reboot, don't show the notification, and don't restore
TEST_F(FullRestoreServiceTest, AskEveryTime) {
  SetRestoreOption(RestoreOption::kAskEveryTime);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);

  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
}

class FullRestoreServiceTestHavingFullRestoreFile
    : public FullRestoreServiceTest {
 public:
  FullRestoreServiceTestHavingFullRestoreFile() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kNoFirstRun);
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

// If the system is crash, show the crash notification, and verify the restore
// flag when click the restore button.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile, CrashAndRestore) {
  ExitTypeService::GetInstanceForProfile(profile())
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  CreateFullRestoreServiceForTesting();

  VerifyNotification(true /* has_crash_notification */,
                     false /* has_restore_notification */);

  SimulateClick(kRestoreForCrashNotificationId,
                RestoreNotificationButtonIndex::kRestore);

  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());

  // Verify the set restore notification is not shown.
  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);
}

// If the system is crash, show the crash notification, and verify the restore
// flag when click the cancel button.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile, CrashAndCancel) {
  ExitTypeService::GetInstanceForProfile(profile())
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  CreateFullRestoreServiceForTesting();

  VerifyNotification(true /* has_crash_notification */,
                     false /* has_restore_notification */);

  SimulateClick(kRestoreForCrashNotificationId,
                RestoreNotificationButtonIndex::kCancel);

  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());
}

// If the system is crash, show the crash notification, close the notification,
// and verify the restore flag when click the cancel button.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile, CrashAndCloseNotification) {
  ExitTypeService::GetInstanceForProfile(profile())
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  CreateFullRestoreServiceForTesting();

  VerifyNotification(true /* has_crash_notification */,
                     false /* has_restore_notification */);

  FullRestoreServiceFactory::GetForProfile(profile())->MaybeCloseNotification();
  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);

  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());
}

// For an existing user, if re-image, don't show notifications for the first
// run.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile, ExistingUserReImage) {
  // Set the restore pref setting to simulate sync for the first time.
  SetRestoreOption(RestoreOption::kAskEveryTime);
  first_run::ResetCachedSentinelDataForTesting();
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kForceFirstRun);

  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);

  VerifyNotification(false, false);
  EXPECT_TRUE(allow_save());

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kForceFirstRun);
  first_run::ResetCachedSentinelDataForTesting();
}

// For a brand new user, if sync off, set 'Ask Every Time' as the default value,
// and don't show notifications, don't restore.
TEST_F(FullRestoreServiceTest, NewUserSyncOff) {
  fake_user_manager()->SetIsCurrentUserNew(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 0);

  VerifyNotification(false, false);

  EXPECT_TRUE(CanPerformRestore(account_id()));
}

// For a new Chrome OS user, if the Chrome restore setting is 'Continue where
// you left off', after sync, set 'Always' as the default value, and don't show
// notifications, don't restore.
TEST_F(FullRestoreServiceTest, NewUserSyncChromeRestoreSetting) {
  fake_user_manager()->SetIsCurrentUserNew(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false);

  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Set the Chrome restore setting to simulate sync for the first time.
  SyncPreferences(SessionStartupPref::kPrefValueLast, std::nullopt);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());

  VerifyNotification(false, false);

  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Update the global values to simulate sync from other device.
  ProcessSyncChanges(SessionStartupPref::kPrefValueNewTab,
                     RestoreOption::kDoNotRestore);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(CanPerformRestore(account_id()));
}

// For a new Chrome OS user, if the Chrome restore setting is 'New tab', after
// sync, set 'Ask every time' as the default value, and don't show
// notifications, don't restore.
TEST_F(FullRestoreServiceTest, NewUserSyncChromeNotRestoreSetting) {
  fake_user_manager()->SetIsCurrentUserNew(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false);

  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Set the Chrome restore setting to simulate sync for the first time.
  SyncPreferences(SessionStartupPref::kPrefValueNewTab, std::nullopt);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false);

  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Update the global values to simulate sync from other device.
  ProcessSyncChanges(SessionStartupPref::kPrefValueLast,
                     RestoreOption::kDoNotRestore);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(CanPerformRestore(account_id()));
}

// For a new Chrome OS user, keep the ChromeOS restore setting from sync, and
// don't show notifications, don't restore.
TEST_F(FullRestoreServiceTest, ReImage) {
  fake_user_manager()->SetIsCurrentUserNew(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false);

  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Set the restore pref setting to simulate sync for the first time.
  SyncPreferences(SessionStartupPref::kPrefValueLast,
                  RestoreOption::kAskEveryTime);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false);

  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Update the global values to simulate sync from other device.
  ProcessSyncChanges(SessionStartupPref::kPrefValueNewTab,
                     RestoreOption::kAlways);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
}

// For the current ChromeOS user, when first time upgrading to the full restore
// release, set the default value based on the current Chrome restore setting,
// and don't show notifications, don't restore
TEST_F(FullRestoreServiceTest, Upgrading) {
  profile()->GetPrefs()->SetInteger(
      ::prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueNewTab));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kDoNotRestore, 0);

  VerifyNotification(false, false);

  EXPECT_FALSE(CanPerformRestore(account_id()));

  // Simulate the Chrome restore setting is changed.
  profile()->GetPrefs()->SetInteger(
      ::prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueLast));

  // The OS restore setting should not change.
  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(CanPerformRestore(account_id()));
}

// If the OS restore setting is 'Ask every time', after reboot, show the restore
// notification, and verify the restore flag when click the restore button.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile, AskEveryTimeAndRestore) {
  SetRestoreOption(RestoreOption::kAskEveryTime);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);

  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */);

  SimulateClick(kRestoreNotificationId,
                RestoreNotificationButtonIndex::kRestore);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());

  VerifyNotification(false, false);

  FullRestoreServiceFactory::GetForProfile(profile())->MaybeCloseNotification();
}

// If the OS restore setting is 'Ask every time', after reboot, show the restore
// notification, and verify the restore flag when click the Settings button.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile, AskEveryTimeAndSettings) {
  SetRestoreOption(RestoreOption::kAskEveryTime);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);

  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */);

  // For the restore notification, the cancel button is used to show the full
  // restore settings.
  SimulateClick(kRestoreNotificationId,
                RestoreNotificationButtonIndex::kCancel);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());

  // Click the setting button, the restore notification should not be closed.
  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */);

  FullRestoreServiceFactory::GetForProfile(profile())->MaybeCloseNotification();

  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);
}

// If the OS restore setting is 'Ask every time', after reboot, show the restore
// notification, and close the notification, then verify the restore flag.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile,
       AskEveryTimeAndCloseNotification) {
  SetRestoreOption(RestoreOption::kAskEveryTime);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);

  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */);

  FullRestoreServiceFactory::GetForProfile(profile())->MaybeCloseNotification();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());

  VerifyNotification(false, false);
}

// If the OS restore setting is 'Ask every time' and the reboot occurred due to
// DeviceScheduledReboot policy, after reboot show the restore notification with
// post reboot notification title.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile,
       AskEveryTimeWithPostRebootNotification) {
  SetRestoreOption(RestoreOption::kAskEveryTime);
  profile()->GetPrefs()->SetBoolean(prefs::kShowPostRebootNotification, true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);

  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */,
                     true /* is_reboot_notification*/);
}

// If the OS restore setting is 'Ask every time', after reboot, close the
// notification before showing the notification. Verify the notification is not
// created.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile, CloseNotificationEarly) {
  SetRestoreOption(RestoreOption::kAskEveryTime);
  FullRestoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        return std::make_unique<FullRestoreService>(
            Profile::FromBrowserContext(context));
      }));

  FullRestoreServiceFactory::GetForProfile(profile())->MaybeCloseNotification();

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);

  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());
}

// If the OS restore setting is 'Always', after reboot, don't show any
// notfications, and verify the restore flag.
TEST_F(FullRestoreServiceTest, Always) {
  SetRestoreOption(RestoreOption::kAlways);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAlways, 1);

  VerifyNotification(false, false);

  EXPECT_TRUE(CanPerformRestore(account_id()));
}

// If the OS restore setting is 'Do not restore', after reboot, don't show any
// notfications, and verify the restore flag.
TEST_F(FullRestoreServiceTest, NotRestore) {
  SetRestoreOption(RestoreOption::kDoNotRestore);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kDoNotRestore, 1);

  VerifyNotification(false, false);

  EXPECT_FALSE(CanPerformRestore(account_id()));
}

// TODO(http://b/326982900): Migrate this test suite to test with forest
// enabled.
class FullRestoreServiceMultipleUsersTest
    : public FullRestoreServiceTestHavingFullRestoreFile {
 protected:
  FullRestoreServiceMultipleUsersTest() {
    test_helper2_ = std::make_unique<FullRestoreTestHelper>(
        "user2@gmail.com", "111111", fake_user_manager(),
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

  void VerifyNotificationForProfile2(bool has_crash_notification,
                                     bool has_restore_notification) {
    test_helper2_->VerifyNotification(has_crash_notification,
                                      has_restore_notification,
                                      /*is_reboot_notification=*/false);
  }

  void SimulateClickForProfile2(const std::string& notification_id,
                                RestoreNotificationButtonIndex action_index) {
    test_helper2_->SimulateClick(notification_id, action_index);
  }

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
  std::unique_ptr<NotificationDisplayServiceTester> display_service2_;
};

// Verify the full restore init process when 2 users login at the same time,
// e.g. after the system restart or upgrading.
TEST_F(FullRestoreServiceMultipleUsersTest, TwoUsersLoginAtTheSameTime) {
  // Add `switches::kLoginUser` to the command line to simulate the system
  // restart.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kLoginUser, account_id().GetUserEmail());
  // Set the first user as the last session active user.
  fake_user_manager()->set_last_session_active_account_id(account_id());

  SetRestoreOption(RestoreOption::kAskEveryTime);
  SetRestoreOptionForProfile2(RestoreOption::kAskEveryTime);
  CreateFullRestoreServiceForTesting();
  CreateFullRestoreService2ForTesting();
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);

  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */);

  // The notification for the second user should not be displayed.
  VerifyNotificationForProfile2(false /* has_crash_notification */,
                                false /* has_restore_notification */);

  SimulateClick(kRestoreNotificationId,
                RestoreNotificationButtonIndex::kRestore);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());

  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);

  // Simulate switch to the second user.
  auto* full_restore_service2 =
      FullRestoreServiceFactory::GetForProfile(profile2());
  full_restore_service2->OnTransitionedToNewActiveUser(profile2());

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOptionForProfile2());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 2);

  // The notification for the second user should not be displayed.
  VerifyNotificationForProfile2(false /* has_crash_notification */,
                                true /* has_restore_notification */);

  SimulateClickForProfile2(kRestoreNotificationId,
                           RestoreNotificationButtonIndex::kRestore);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOptionForProfile2());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());

  VerifyNotificationForProfile2(false /* has_crash_notification */,
                                false /* has_restore_notification */);
}

// Verify the full restore init process when 2 users login one by one.
TEST_F(FullRestoreServiceMultipleUsersTest, TwoUsersLoginOneByOne) {
  SetRestoreOption(RestoreOption::kAskEveryTime);
  CreateFullRestoreServiceForTesting();
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */);

  SimulateClick(kRestoreNotificationId,
                RestoreNotificationButtonIndex::kRestore);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());

  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);

  // Simulate switch to the second user.
  SetRestoreOptionForProfile2(RestoreOption::kAskEveryTime);
  CreateFullRestoreService2ForTesting();

  auto* full_restore_service2 =
      FullRestoreServiceFactory::GetForProfile(profile2());
  full_restore_service2->OnTransitionedToNewActiveUser(profile2());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOptionForProfile2());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 2);

  // The notification for the second user should be displayed.
  VerifyNotificationForProfile2(false /* has_crash_notification */,
                                true /* has_restore_notification */);

  SimulateClickForProfile2(kRestoreNotificationId,
                           RestoreNotificationButtonIndex::kRestore);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOptionForProfile2());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());

  VerifyNotificationForProfile2(false /* has_crash_notification */,
                                false /* has_restore_notification */);
}

// Verify the full restore init process when the system restarts.
TEST_F(FullRestoreServiceMultipleUsersTest, TwoUsersLoginWithActiveUserLogin) {
  // Add `switches::kLoginUser` to the command line to simulate the system
  // restart.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kLoginUser, account_id().GetUserEmail());
  // Set the second user as the last session active user.
  fake_user_manager()->set_last_session_active_account_id(account_id2());

  SetRestoreOption(RestoreOption::kAskEveryTime);
  SetRestoreOptionForProfile2(RestoreOption::kAskEveryTime);
  CreateFullRestoreServiceForTesting();
  CreateFullRestoreService2ForTesting();
  content::RunAllTasksUntilIdle();

  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 0);
  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);

  // Simulate switch to the second user.
  auto* full_restore_service2 =
      FullRestoreServiceFactory::GetForProfile(profile2());
  full_restore_service2->OnTransitionedToNewActiveUser(profile2());

  // The notification for the second user should be displayed.
  VerifyNotificationForProfile2(false /* has_crash_notification */,
                                true /* has_restore_notification */);
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);

  SimulateClickForProfile2(kRestoreNotificationId,
                           RestoreNotificationButtonIndex::kRestore);
  EXPECT_TRUE(CanPerformRestore(account_id2()));

  // Simulate switch to the first user.
  auto* full_restore_service =
      FullRestoreServiceFactory::GetForProfile(profile());
  full_restore_service->OnTransitionedToNewActiveUser(profile());

  // The notification for the first user should be displayed.
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 2);
  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */);

  SimulateClick(kRestoreNotificationId,
                RestoreNotificationButtonIndex::kRestore);
  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Simulate switch to the second user, and verify no more init process.
  full_restore_service2->OnTransitionedToNewActiveUser(profile2());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 2);

  // Simulate switch to the first user, and verify no more init process.
  full_restore_service->OnTransitionedToNewActiveUser(profile());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 2);
}

class ForestFullRestoreServiceTest : public FullRestoreServiceTest {
 protected:
  ForestFullRestoreServiceTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(features::kForestFeature);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kNoFirstRun);
  }
  ForestFullRestoreServiceTest(const ForestFullRestoreServiceTest&) = delete;
  ForestFullRestoreServiceTest& operator=(const ForestFullRestoreServiceTest&) =
      delete;
  ~ForestFullRestoreServiceTest() override = default;
};

// If the system is crash, and there is no full restore file, don't show the
// informed restore dialog.
TEST_F(ForestFullRestoreServiceTest, Crash) {
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
TEST_F(ForestFullRestoreServiceTest, AskEveryTime) {
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
TEST_F(ForestFullRestoreServiceTest, Always) {
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
TEST_F(ForestFullRestoreServiceTest, NotRestore) {
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
TEST_F(ForestFullRestoreServiceTest, NewUserSyncOff) {
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
TEST_F(ForestFullRestoreServiceTest, NewUserSyncChromeRestoreSetting) {
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
TEST_F(ForestFullRestoreServiceTest, NewUserSyncChromeNotRestoreSetting) {
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
TEST_F(ForestFullRestoreServiceTest, ReImage) {
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
TEST_F(ForestFullRestoreServiceTest, Upgrading) {
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

class ForestFullRestoreServiceTestHavingFullRestoreFile
    : public FullRestoreServiceTestHavingFullRestoreFile {
 protected:
  ForestFullRestoreServiceTestHavingFullRestoreFile() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(features::kForestFeature);
  }
  ForestFullRestoreServiceTestHavingFullRestoreFile(
      const ForestFullRestoreServiceTestHavingFullRestoreFile&) = delete;
  ForestFullRestoreServiceTestHavingFullRestoreFile& operator=(
      const ForestFullRestoreServiceTestHavingFullRestoreFile&) = delete;
  ~ForestFullRestoreServiceTestHavingFullRestoreFile() override = default;
};

// If the system is crash, the delegate is notified.
TEST_F(ForestFullRestoreServiceTestHavingFullRestoreFile, Crash) {
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

  // The notification should not show up anymore with forest enabled.
  // TODO(http://b/326982900): Remove this check once non-forest cannot be
  // enabled.
  VerifyNotification(/*has_crash_notification=*/false,
                     /*has_restore_notification=*/false);

  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());
}

TEST_F(ForestFullRestoreServiceTestHavingFullRestoreFile,
       AskEveryTimeAndRestore) {
  SetRestoreOption(RestoreOption::kAskEveryTime);
  auto mock_delegate = std::make_unique<MockFullRestoreServiceDelegate>();
  EXPECT_CALL(*mock_delegate,
              MaybeStartInformedRestoreOverviewSession(testing::_))
      .Times(1);
  CreateFullRestoreServiceForTesting(std::move(mock_delegate));

  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);
  // The notification should not show up anymore with forest enabled.
  // TODO(http://b/326982900): Remove this check once non-forest cannot be
  // enabled.
  VerifyNotification(/*has_crash_notification=*/false,
                     /*has_restore_notification=*/false);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());
}

// Test tha for an existing user, if re-image, do not show the informed restore
// dialog for the first run.
TEST_F(ForestFullRestoreServiceTestHavingFullRestoreFile, ExistingUserReImage) {
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

class ForestFullRestoreServiceMultipleUsersTest
    : public FullRestoreServiceMultipleUsersTest {
 protected:
  ForestFullRestoreServiceMultipleUsersTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(features::kForestFeature);

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
