// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/full_restore_service.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
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
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
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
#include "components/sync/base/model_type.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  SetPrefValue(kRestoreAppsAndPagesPrefName,
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

}  // namespace

class FullRestoreServiceTest : public testing::Test {
 public:
  FullRestoreServiceTest()
      : user_manager_enabler_(std::make_unique<FakeChromeUserManager>()) {}

  ~FullRestoreServiceTest() override = default;

  FullRestoreServiceTest(const FullRestoreServiceTest&) = delete;
  FullRestoreServiceTest& operator=(const FullRestoreServiceTest&) = delete;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user.test@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_ = profile_builder.Build();
    profile_->GetPrefs()->ClearPref(kRestoreAppsAndPagesPrefName);

    account_id_ =
        AccountId::FromUserEmailGaiaId("usertest@gmail.com", "1234567890");
    const auto* user = GetFakeUserManager()->AddUser(account_id_);
    GetFakeUserManager()->LoginUser(account_id_);
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                            profile_.get());

    ::app_restore::AppRestoreInfo::GetInstance()->SetRestorePref(account_id_,
                                                                 false);

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());
  }

  void TearDown() override { profile_.reset(); }

  FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void CreateFullRestoreServiceForTesting() {
    FullRestoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<FullRestoreService>(
              Profile::FromBrowserContext(context));
        }));
    content::RunAllTasksUntilIdle();
  }

  void VerifyRestoreInitSettingHistogram(RestoreOption option,
                                         base::HistogramBase::Count count) {
    histogram_tester_.ExpectUniqueSample("Apps.RestoreInitSetting", option,
                                         count);
  }

  bool HasNotificationFor(const std::string& notification_id) {
    absl::optional<message_center::Notification> message_center_notification =
        display_service()->GetNotification(notification_id);
    return message_center_notification.has_value();
  }

  void VerifyRestoreNotificationTitle(const std::string& notification_id,
                                      bool is_reboot_notification) {
    absl::optional<message_center::Notification> message_center_notification =
        display_service()->GetNotification(notification_id);
    ASSERT_TRUE(message_center_notification.has_value());
    const std::u16string& title = message_center_notification.value().title();
    if (is_reboot_notification) {
      EXPECT_EQ(title,
                l10n_util::GetStringUTF16(IDS_POLICY_DEVICE_POST_REBOOT_TITLE));
    } else {
      EXPECT_EQ(title,
                l10n_util::GetStringUTF16(IDS_RESTORE_NOTIFICATION_TITLE));
    }
  }

  void VerifyNotification(bool has_crash_notification,
                          bool has_restore_notification,
                          bool is_reboot_notification = false) {
    if (has_crash_notification)
      EXPECT_TRUE(HasNotificationFor(kRestoreForCrashNotificationId));
    else
      EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));

    if (has_restore_notification) {
      EXPECT_TRUE(HasNotificationFor(kRestoreNotificationId));
      VerifyRestoreNotificationTitle(kRestoreNotificationId,
                                     is_reboot_notification);
    } else {
      EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));
    }
  }

  void SimulateClick(const std::string& notification_id,
                     RestoreNotificationButtonIndex action_index) {
    display_service()->SimulateClick(
        NotificationHandler::Type::TRANSIENT, notification_id,
        static_cast<int>(action_index), absl::nullopt);
  }

  // Simulates the initial sync of preferences.
  void SyncPreferences(
      SessionStartupPref::PrefValue restore_on_startup_value,
      absl::optional<RestoreOption> maybe_restore_apps_and_pages_value) {
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

  RestoreOption GetRestoreOption() const {
    return static_cast<RestoreOption>(
        profile()->GetPrefs()->GetInteger(kRestoreAppsAndPagesPrefName));
  }

  TestingProfile* profile() const { return profile_.get(); }

  const AccountId& account_id() const { return account_id_; }

  NotificationDisplayServiceTester* display_service() const {
    return display_service_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  user_manager::ScopedUserManager user_manager_enabler_;
  AccountId account_id_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

  base::HistogramTester histogram_tester_;
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
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
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
  // FullRestoreServiceTest:
  void SetUp() override {
    FullRestoreServiceTest::SetUp();

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kNoFirstRun);

    CreateRestoreData(profile());
  }

  void TearDown() override {
    ::full_restore::FullRestoreSaveHandler::GetInstance()->ClearForTesting();
  }

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

  bool allow_save() const {
    return ::full_restore::FullRestoreSaveHandler::GetInstance()->allow_save_;
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

  FullRestoreService::GetForProfile(profile())->MaybeCloseNotification();
  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);

  EXPECT_TRUE(CanPerformRestore(account_id()));
  EXPECT_TRUE(allow_save());
}

// For an existing user, if re-image, don't show notifications for the first
// run.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile, ExsitingUserReImage) {
  // Set the restore pref setting to simulate sync for the first time.
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));

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
  GetFakeUserManager()->set_current_user_new(true);
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
  GetFakeUserManager()->set_current_user_new(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false);

  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Set the Chrome restore setting to simulate sync for the first time.
  SyncPreferences(SessionStartupPref::kPrefValueLast, absl::nullopt);
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
  GetFakeUserManager()->set_current_user_new(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false);

  EXPECT_TRUE(CanPerformRestore(account_id()));

  // Set the Chrome restore setting to simulate sync for the first time.
  SyncPreferences(SessionStartupPref::kPrefValueNewTab, absl::nullopt);
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
  GetFakeUserManager()->set_current_user_new(true);
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
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
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

  FullRestoreService::MaybeCloseNotification(profile());
}

// If the OS restore setting is 'Ask every time', after reboot, show the restore
// notification, and verify the restore flag when click the Settings button.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile, AskEveryTimeAndSettings) {
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
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

  FullRestoreService::MaybeCloseNotification(profile());

  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);
}

// If the OS restore setting is 'Ask every time', after reboot, show the restore
// notification, and close the notification, then verify the restore flag.
TEST_F(FullRestoreServiceTestHavingFullRestoreFile,
       AskEveryTimeAndCloseNotification) {
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);

  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */);

  FullRestoreService::MaybeCloseNotification(profile());

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
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
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
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));

  FullRestoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        return std::make_unique<FullRestoreService>(
            Profile::FromBrowserContext(context));
      }));

  FullRestoreService::MaybeCloseNotification(profile());

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
  profile()->GetPrefs()->SetInteger(kRestoreAppsAndPagesPrefName,
                                    static_cast<int>(RestoreOption::kAlways));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kAlways, 1);

  VerifyNotification(false, false);

  EXPECT_TRUE(CanPerformRestore(account_id()));
}

// If the OS restore setting is 'Do not restore', after reboot, don't show any
// notfications, and verify the restore flag.
TEST_F(FullRestoreServiceTest, NotRestore) {
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kDoNotRestore));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  VerifyRestoreInitSettingHistogram(RestoreOption::kDoNotRestore, 1);

  VerifyNotification(false, false);

  EXPECT_FALSE(CanPerformRestore(account_id()));
}

class FullRestoreServiceMultipleUsersTest
    : public FullRestoreServiceTestHavingFullRestoreFile {
 protected:
  FullRestoreServiceMultipleUsersTest() = default;
  ~FullRestoreServiceMultipleUsersTest() override = default;

  FullRestoreServiceMultipleUsersTest(
      const FullRestoreServiceMultipleUsersTest&) = delete;
  FullRestoreServiceMultipleUsersTest& operator=(
      const FullRestoreServiceMultipleUsersTest&) = delete;

  void SetUp() override {
    FullRestoreServiceTestHavingFullRestoreFile::SetUp();

    EXPECT_TRUE(temp_dir2_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user2@gmail.com");
    profile_builder.SetPath(temp_dir2_.GetPath().AppendASCII("TestProfile2"));
    profile2_ = profile_builder.Build();
    profile2_->GetPrefs()->ClearPref(kRestoreAppsAndPagesPrefName);

    account_id2_ = AccountId::FromUserEmailGaiaId(
        profile2_->GetProfileUserName(), "111111");
    const auto* user = GetFakeUserManager()->AddUser(account_id2_);
    GetFakeUserManager()->LoginUser(account_id2_);
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                            profile2_.get());

    // Reset the restore flag and pref as the default value.
    ::app_restore::AppRestoreInfo::GetInstance()->SetRestorePref(account_id2_,
                                                                 false);

    display_service2_ =
        std::make_unique<NotificationDisplayServiceTester>(profile2_.get());

    CreateRestoreData(profile2());
  }

  void TearDown() override {
    profile2_.reset();
    FullRestoreServiceTest::TearDown();
  }

  void CreateFullRestoreService2ForTesting() {
    FullRestoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile2(), base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
          return std::make_unique<FullRestoreService>(
              Profile::FromBrowserContext(context));
        }));
  }

  RestoreOption GetRestoreOptionForProfile2() const {
    return static_cast<RestoreOption>(
        profile2()->GetPrefs()->GetInteger(kRestoreAppsAndPagesPrefName));
  }

  bool HasNotificationForProfile2(const std::string& notification_id) {
    absl::optional<message_center::Notification> message_center_notification =
        display_service2()->GetNotification(notification_id);
    return message_center_notification.has_value();
  }

  void VerifyNotificationForProfile2(bool has_crash_notification,
                                     bool has_restore_notification) {
    if (has_crash_notification)
      EXPECT_TRUE(HasNotificationForProfile2(kRestoreForCrashNotificationId));
    else
      EXPECT_FALSE(HasNotificationForProfile2(kRestoreForCrashNotificationId));

    if (has_restore_notification)
      EXPECT_TRUE(HasNotificationForProfile2(kRestoreNotificationId));
    else
      EXPECT_FALSE(HasNotificationForProfile2(kRestoreNotificationId));
  }

  void SimulateClickForProfile2(const std::string& notification_id,
                                RestoreNotificationButtonIndex action_index) {
    display_service2()->SimulateClick(
        NotificationHandler::Type::TRANSIENT, notification_id,
        static_cast<int>(action_index), absl::nullopt);
  }

  NotificationDisplayServiceTester* display_service2() const {
    return display_service2_.get();
  }

  TestingProfile* profile2() const { return profile2_.get(); }

  const AccountId& account_id2() const { return account_id2_; }

 private:
  std::unique_ptr<TestingProfile> profile2_;
  base::ScopedTempDir temp_dir2_;
  AccountId account_id2_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service2_;
};

// Verify the full restore init process when 2 users login at the same time,
// e.g. after the system reatart or upgrading.
TEST_F(FullRestoreServiceMultipleUsersTest, TwoUsersLoginAtTheSameTime) {
  // Add `switches::kLoginUser` to the command line to simulate the system
  // restart.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kLoginUser, account_id().GetUserEmail());
  // Set the first user as the last session active user.
  GetFakeUserManager()->set_last_session_active_account_id(account_id());

  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
  profile2()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
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
  auto* full_restore_service2 = FullRestoreService::GetForProfile(profile2());
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
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
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
  profile2()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
  CreateFullRestoreService2ForTesting();

  auto* full_restore_service2 = FullRestoreService::GetForProfile(profile2());
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
  GetFakeUserManager()->set_last_session_active_account_id(account_id2());

  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
  profile2()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
  CreateFullRestoreServiceForTesting();
  CreateFullRestoreService2ForTesting();
  content::RunAllTasksUntilIdle();

  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 0);
  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */);

  // Simulate switch to the second user.
  auto* full_restore_service2 = FullRestoreService::GetForProfile(profile2());
  full_restore_service2->OnTransitionedToNewActiveUser(profile2());

  // The notification for the second user should be displayed.
  VerifyNotificationForProfile2(false /* has_crash_notification */,
                                true /* has_restore_notification */);
  VerifyRestoreInitSettingHistogram(RestoreOption::kAskEveryTime, 1);

  SimulateClickForProfile2(kRestoreNotificationId,
                           RestoreNotificationButtonIndex::kRestore);
  EXPECT_TRUE(CanPerformRestore(account_id2()));

  // Simulate switch to the first user.
  auto* full_restore_service = FullRestoreService::GetForProfile(profile());
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

}  // namespace ash::full_restore
