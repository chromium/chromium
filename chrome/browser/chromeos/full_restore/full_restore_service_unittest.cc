// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_service_factory.h"

#include "ash/public/cpp/ash_features.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/full_restore/full_restore_prefs.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
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
      : user_manager_enabler_(
            std::make_unique<chromeos::FakeChromeUserManager>()) {}

  ~FullRestoreServiceTest() override = default;

  FullRestoreServiceTest(const FullRestoreServiceTest&) = delete;
  FullRestoreServiceTest& operator=(const FullRestoreServiceTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kFullRestore);

    // Reset the restore flag as the default value.
    ::full_restore::SetRestoreFlag(false);

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_ = profile_builder.Build();
    profile_->GetPrefs()->ClearPref(kRestoreAppsAndPagesPrefName);

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());
  }

  void TearDown() override { profile_.reset(); }

  FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void CreateFullRestoreServiceForTesting() {
    FullRestoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<FullRestoreService>(
              Profile::FromBrowserContext(context));
        }));
  }

  bool HasNotificationFor(const std::string& notification_id) {
    base::Optional<message_center::Notification> message_center_notification =
        display_service()->GetNotification(notification_id);
    return message_center_notification.has_value();
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

  TestingProfile* profile() const { return profile_.get(); }

  NotificationDisplayServiceTester* display_service() const {
    return display_service_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  user_manager::ScopedUserManager user_manager_enabler_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

// If the system is crash, show the crash notification, and verify the restore
// flag when click the restore button.
TEST_F(FullRestoreServiceTest, CrashAndRestore) {
  profile()->set_last_session_exited_cleanly(false);
  CreateFullRestoreServiceForTesting();

  EXPECT_TRUE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));

  SimulateClick(kRestoreForCrashNotificationId,
                RestoreNotificationButtonIndex::kRestore);

  EXPECT_TRUE(::full_restore::ShouldRestore());
}

// If the system is crash, show the crash notification, and verify the restore
// flag when click the cancel button.
TEST_F(FullRestoreServiceTest, CrashAndCancel) {
  profile()->set_last_session_exited_cleanly(false);
  CreateFullRestoreServiceForTesting();

  EXPECT_TRUE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));

  SimulateClick(kRestoreForCrashNotificationId,
                RestoreNotificationButtonIndex::kCancel);

  EXPECT_FALSE(::full_restore::ShouldRestore());
}

// For a brand new user, if sync off, set 'Ask Every Time' as the default value,
// and don't show notifications, don't restore.
TEST_F(FullRestoreServiceTest, NewUserSyncOff) {
  GetFakeUserManager()->set_current_user_new(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));
  EXPECT_FALSE(::full_restore::ShouldRestore());
}

// For a new Chrome OS user, if the Chrome restore setting is 'Continue where
// you left off', after sync, set 'Always' as the default value, and don't show
// notifications, don't restore.
TEST_F(FullRestoreServiceTest, NewUserSyncChromeRestoreSetting) {
  GetFakeUserManager()->set_current_user_new(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));
  EXPECT_FALSE(::full_restore::ShouldRestore());

  // Set the Chrome restore setting to simulate sync for the first time.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kRestoreOnStartup,
      base::Value(static_cast<int>(SessionStartupPref::kPrefValueLast))));
  syncer::SyncableService* sync = SyncPreferences(sync_data_list);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));
  EXPECT_FALSE(::full_restore::ShouldRestore());

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
  EXPECT_FALSE(::full_restore::ShouldRestore());
}

// For a new Chrome OS user, if the Chrome restore setting is 'New tab', after
// sync, set 'Ask every time' as the default value, and don't show
// notifications, don't restore.
TEST_F(FullRestoreServiceTest, NewUserSyncChromeNotRestoreSetting) {
  GetFakeUserManager()->set_current_user_new(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));
  EXPECT_FALSE(::full_restore::ShouldRestore());

  // Set the Chrome restore setting to simulate sync for the first time.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kRestoreOnStartup,
      base::Value(static_cast<int>(SessionStartupPref::kPrefValueNewTab))));
  syncer::SyncableService* sync = SyncPreferences(sync_data_list);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));
  EXPECT_FALSE(::full_restore::ShouldRestore());

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
  EXPECT_FALSE(::full_restore::ShouldRestore());
}

// For a new Chrome OS user, keep the ChromeOS restore setting from sync, and
// don't show notifications, don't restore.
TEST_F(FullRestoreServiceTest, ReImage) {
  GetFakeUserManager()->set_current_user_new(true);
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));
  EXPECT_FALSE(::full_restore::ShouldRestore());

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
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));
  EXPECT_FALSE(::full_restore::ShouldRestore());

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
  EXPECT_FALSE(::full_restore::ShouldRestore());
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
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));
  EXPECT_FALSE(::full_restore::ShouldRestore());

  // Simulate the Chrome restore setting is changed.
  profile()->GetPrefs()->SetInteger(
      prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueLast));

  // The OS restore setting should not change.
  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(::full_restore::ShouldRestore());
}

// If the OS restore setting is 'Ask every time', after reboot, show the restore
// notfication, and verify the restore flag when click the restore button.
TEST_F(FullRestoreServiceTest, AskEveryTimeAndRestore) {
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_TRUE(HasNotificationFor(kRestoreNotificationId));

  SimulateClick(kRestoreNotificationId,
                RestoreNotificationButtonIndex::kRestore);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(::full_restore::ShouldRestore());
}

// If the OS restore setting is 'Ask every time', after reboot, show the restore
// notfication, and verify the restore flag when click the cancel button.
TEST_F(FullRestoreServiceTest, AskEveryTimeAndCancel) {
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_TRUE(HasNotificationFor(kRestoreNotificationId));

  SimulateClick(kRestoreNotificationId,
                RestoreNotificationButtonIndex::kCancel);

  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_FALSE(::full_restore::ShouldRestore());
}

// If the OS restore setting is 'Always', after reboot, don't show any
// notfications, and verify the restore flag.
TEST_F(FullRestoreServiceTest, Always) {
  profile()->GetPrefs()->SetInteger(kRestoreAppsAndPagesPrefName,
                                    static_cast<int>(RestoreOption::kAlways));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));
  EXPECT_TRUE(::full_restore::ShouldRestore());
}

// If the OS restore setting is 'Do not restore', after reboot, don't show any
// notfications, and verify the restore flag.
TEST_F(FullRestoreServiceTest, NotRestore) {
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kDoNotRestore));
  CreateFullRestoreServiceForTesting();

  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(HasNotificationFor(kRestoreForCrashNotificationId));
  EXPECT_FALSE(HasNotificationFor(kRestoreNotificationId));
  EXPECT_FALSE(::full_restore::ShouldRestore());
}

}  // namespace full_restore
}  // namespace chromeos
