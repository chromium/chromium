// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_service.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_features.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/full_restore/full_restore_prefs.h"
#include "chrome/browser/chromeos/full_restore/full_restore_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/model/fake_sync_change_processor.h"
#include "components/sync/test/model/sync_error_factory_mock.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

// TODO(crbug.com/909794): Verify apps restoration.

namespace chromeos {
namespace full_restore {

namespace {

syncer::SyncData CreatePrefSyncData(const std::string& name,
                                    const base::Value& value) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  json.Serialize(value);
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref =
      features::IsSplitSettingsSyncEnabled()
          ? specifics.mutable_os_preference()->mutable_preference()
          : specifics.mutable_preference();
  pref->set_name(name);
  pref->set_value(serialized);
  return syncer::SyncData::CreateRemoteData(specifics);
}

}  // namespace

class FullRestoreServiceTest : public testing::Test {
 protected:
  FullRestoreServiceTest()
      : user_manager_enabler_(std::make_unique<FakeChromeUserManager>()) {}

  ~FullRestoreServiceTest() override = default;

  FullRestoreServiceTest(const FullRestoreServiceTest&) = delete;
  FullRestoreServiceTest& operator=(const FullRestoreServiceTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kFullRestore);

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_ = profile_builder.Build();
    profile_->GetPrefs()->ClearPref(kRestoreAppsAndPagesPrefName);

    account_id_ = AccountId::FromUserEmailGaiaId(profile_->GetProfileUserName(),
                                                 "1234567890");
    GetFakeUserManager()->AddUser(account_id_);
    GetFakeUserManager()->LoginUser(account_id_);

    // Reset the restore flag as the default value.
    ::full_restore::FullRestoreInfo::GetInstance()->SetRestoreFlag(account_id_,
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

  bool HasNotificationFor(const std::string& notification_id) {
    base::Optional<message_center::Notification> message_center_notification =
        display_service()->GetNotification(notification_id);
    return message_center_notification.has_value();
  }

  void VerifyNotification(bool has_crash_notification,
                          bool has_restore_notification,
                          bool has_set_restore_notification) {
    if (has_crash_notification)
      EXPECT_TRUE(HasNotificationFor(kRestoreForCrashNotificationId));
    else
      EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));

    if (has_restore_notification)
      EXPECT_TRUE(HasNotificationFor(kRestoreNotificationId));
    else
      EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));

    if (has_set_restore_notification)
      EXPECT_TRUE(HasNotificationFor(kSetRestorePrefNotificationId));
    else
      EXPECT_FALSE(HasNotificationFor(kSetRestorePrefNotificationId));
  }

  void SimulateClick(const std::string& notification_id,
                     RestoreNotificationButtonIndex action_index) {
    display_service()->SimulateClick(
        NotificationHandler::Type::TRANSIENT, notification_id,
        static_cast<int>(action_index), base::nullopt);
  }

  // Simulates the initial sync of preferences.
  syncer::SyncableService* SyncPreferences(
      const syncer::SyncDataList& sync_data_list) {
    syncer::ModelType model_type = features::IsSplitSettingsSyncEnabled()
                                       ? syncer::OS_PREFERENCES
                                       : syncer::PREFERENCES;
    syncer::SyncableService* sync =
        profile()->GetTestingPrefService()->GetSyncableService(model_type);
    sync->MergeDataAndStartSyncing(
        model_type, sync_data_list,
        std::make_unique<syncer::FakeSyncChangeProcessor>(),
        std::make_unique<syncer::SyncErrorFactoryMock>());
    return sync;
  }

  RestoreOption GetRestoreOption() const {
    return static_cast<RestoreOption>(
        profile()->GetPrefs()->GetInteger(kRestoreAppsAndPagesPrefName));
  }

  int GetRestoreSelectedCount() const {
    return profile()->GetPrefs()->GetInteger(kRestoreSelectedCountPrefName);
  }

  TestingProfile* profile() const { return profile_.get(); }

  const AccountId& account_id() const { return account_id_; }

  NotificationDisplayServiceTester* display_service() const {
    return display_service_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  user_manager::ScopedUserManager user_manager_enabler_;
  AccountId account_id_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

// If the system is crash, show the crash notification, and verify the restore
// flag when click the restore button.
TEST_F(FullRestoreServiceTest, CrashAndRestore) {
  profile()->set_last_session_exited_cleanly(false);
  CreateFullRestoreServiceForTesting();

  VerifyNotification(true /* has_crash_notification */,
                     false /* has_restore_notification */,
                     false /* has_set_restore_notification */);

  SimulateClick(kRestoreForCrashNotificationId,
                RestoreNotificationButtonIndex::kRestore);

  EXPECT_TRUE(::full_restore::ShouldRestore(account_id()));
}

// If the system is crash, show the crash notification, and verify the restore
// flag when click the cancel button.
TEST_F(FullRestoreServiceTest, CrashAndCancel) {
  profile()->set_last_session_exited_cleanly(false);
  CreateFullRestoreServiceForTesting();

  VerifyNotification(true /* has_crash_notification */,
                     false /* has_restore_notification */,
                     false /* has_set_restore_notification */);

  SimulateClick(kRestoreForCrashNotificationId,
                RestoreNotificationButtonIndex::kCancel);

  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));
}

// For a brand new user, if sync off, set 'Ask Every Time' as the default value,
// and don't show notifications, don't restore.
TEST_F(FullRestoreServiceTest, NewUserSyncOff) {
  GetFakeUserManager()->set_current_user_new(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false, false);

  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));
}

// For a new Chrome OS user, if the Chrome restore setting is 'Continue where
// you left off', after sync, set 'Always' as the default value, and don't show
// notifications, don't restore.
TEST_F(FullRestoreServiceTest, NewUserSyncChromeRestoreSetting) {
  GetFakeUserManager()->set_current_user_new(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false, false);

  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));

  // Set the Chrome restore setting to simulate sync for the first time.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kRestoreOnStartup,
      base::Value(static_cast<int>(SessionStartupPref::kPrefValueLast))));
  syncer::SyncableService* sync = SyncPreferences(sync_data_list);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());

  VerifyNotification(false, false, false);

  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));

  // Update the global values to simulate sync from other device.
  syncer::SyncChangeList change_list;
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreatePrefSyncData(prefs::kRestoreOnStartup,
                         base::Value(static_cast<int>(
                             SessionStartupPref::kPrefValueNewTab)))));
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreatePrefSyncData(
          kRestoreAppsAndPagesPrefName,
          base::Value(static_cast<int>(RestoreOption::kDoNotRestore)))));
  sync->ProcessSyncChanges(FROM_HERE, change_list);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));
}

// For a new Chrome OS user, if the Chrome restore setting is 'New tab', after
// sync, set 'Ask every time' as the default value, and don't show
// notifications, don't restore.
TEST_F(FullRestoreServiceTest, NewUserSyncChromeNotRestoreSetting) {
  GetFakeUserManager()->set_current_user_new(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false, false);

  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));

  // Set the Chrome restore setting to simulate sync for the first time.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kRestoreOnStartup,
      base::Value(static_cast<int>(SessionStartupPref::kPrefValueNewTab))));
  syncer::SyncableService* sync = SyncPreferences(sync_data_list);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false, false);

  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));

  // Update the global values to simulate sync from other device.
  syncer::SyncChangeList change_list;
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreatePrefSyncData(
          prefs::kRestoreOnStartup,
          base::Value(static_cast<int>(SessionStartupPref::kPrefValueLast)))));
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreatePrefSyncData(
          kRestoreAppsAndPagesPrefName,
          base::Value(static_cast<int>(RestoreOption::kDoNotRestore)))));
  sync->ProcessSyncChanges(FROM_HERE, change_list);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));
}

// For a new Chrome OS user, keep the ChromeOS restore setting from sync, and
// don't show notifications, don't restore.
TEST_F(FullRestoreServiceTest, ReImage) {
  GetFakeUserManager()->set_current_user_new(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false, false);

  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));

  // Set the restore pref setting to simulate sync for the first time.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kRestoreOnStartup,
      base::Value(static_cast<int>(SessionStartupPref::kPrefValueLast))));
  sync_data_list.push_back(CreatePrefSyncData(
      kRestoreAppsAndPagesPrefName,
      base::Value(static_cast<int>(RestoreOption::kAskEveryTime))));
  syncer::SyncableService* sync = SyncPreferences(sync_data_list);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false, false, false);

  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));

  // Update the global values to simulate sync from other device.
  syncer::SyncChangeList change_list;
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreatePrefSyncData(prefs::kRestoreOnStartup,
                         base::Value(static_cast<int>(
                             SessionStartupPref::kPrefValueNewTab)))));
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreatePrefSyncData(
          kRestoreAppsAndPagesPrefName,
          base::Value(static_cast<int>(RestoreOption::kAlways)))));
  sync->ProcessSyncChanges(FROM_HERE, change_list);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());
  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));
}

// For the current ChromeOS user, when first time upgrading to the full restore
// release, set the default value based on the current Chrome restore setting,
// and don't show notifications, don't restore
TEST_F(FullRestoreServiceTest, Upgrading) {
  profile()->GetPrefs()->SetInteger(
      prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueNewTab));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());

  VerifyNotification(false, false, false);

  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));

  // Simulate the Chrome restore setting is changed.
  profile()->GetPrefs()->SetInteger(
      prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueLast));

  // The OS restore setting should not change.
  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));
}

// If the OS restore setting is 'Ask every time', after reboot, show the restore
// notification, and verify the restore flag when click the restore button.
TEST_F(FullRestoreServiceTest, AskEveryTimeAndRestore) {
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */,
                     false /* has_set_restore_notification */);

  SimulateClick(kRestoreNotificationId,
                RestoreNotificationButtonIndex::kRestore);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(::full_restore::ShouldRestore(account_id()));

  VerifyNotification(false, false, false);
}

// If the OS restore setting is 'Ask every time', after reboot, show the restore
// notfication, and verify the restore flag when click the cancel button.
TEST_F(FullRestoreServiceTest, AskEveryTimeAndCancel) {
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */,
                     false /* has_set_restore_notification */);

  SimulateClick(kRestoreNotificationId,
                RestoreNotificationButtonIndex::kCancel);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));

  VerifyNotification(false, false, false);
}

// If the OS restore setting is 'Always', after reboot, don't show any
// notfications, and verify the restore flag.
TEST_F(FullRestoreServiceTest, Always) {
  profile()->GetPrefs()->SetInteger(kRestoreAppsAndPagesPrefName,
                                    static_cast<int>(RestoreOption::kAlways));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());

  VerifyNotification(false, false, false);

  EXPECT_TRUE(::full_restore::ShouldRestore(account_id()));
}

// If the OS restore setting is 'Do not restore', after reboot, don't show any
// notfications, and verify the restore flag.
TEST_F(FullRestoreServiceTest, NotRestore) {
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kDoNotRestore));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());

  VerifyNotification(false, false, false);

  EXPECT_FALSE(::full_restore::ShouldRestore(account_id()));
}

// If the restore option has been selected 3 times, show the set restore
// notification.
TEST_F(FullRestoreServiceTest, SetRestorePrefNotification) {
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));

  // Set |kRestoreSelectedCountPrefName| = 2 to simulate the restore option has
  // been selected twice.
  profile()->GetPrefs()->SetInteger(kRestoreSelectedCountPrefName, 2);

  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  VerifyNotification(false /* has_crash_notification */,
                     true /* has_restore_notification */,
                     false /* has_set_restore_notification */);

  // The restore option has been selected the 3rd times.
  SimulateClick(kRestoreNotificationId,
                RestoreNotificationButtonIndex::kRestore);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(::full_restore::ShouldRestore(account_id()));

  // Verify the set restore notification is shown.
  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */,
                     true /* has_set_restore_notification */);

  EXPECT_EQ(3, GetRestoreSelectedCount());
}

// When |kRestoreSelectedCountPrefName| = 3, if the restore option is selected
// again, |kRestoreSelectedCountPrefName| should not change.
TEST_F(FullRestoreServiceTest, RestoreSelectedCount) {
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));

  // Set |kRestoreSelectedCountPrefName| = 3 to simulate the restore option has
  // been selected 3 times locally.
  profile()->GetPrefs()->SetInteger(kRestoreSelectedCountPrefName, 3);

  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  // The restore option is selected.
  SimulateClick(kRestoreNotificationId,
                RestoreNotificationButtonIndex::kRestore);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(::full_restore::ShouldRestore(account_id()));

  // Verify the set restore notification is shown.
  VerifyNotification(false /* has_crash_notification */,
                     false /* has_restore_notification */,
                     true /* has_set_restore_notification */);

  EXPECT_EQ(3, GetRestoreSelectedCount());
}

}  // namespace full_restore
}  // namespace chromeos
