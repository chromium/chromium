// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}
}  // namespace

class LocalPasswordsMigrationWarningUtilTest : public testing::Test {
 protected:
  LocalPasswordsMigrationWarningUtilTest() = default;
  ~LocalPasswordsMigrationWarningUtilTest() override = default;

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return profile_.GetTestingPrefService();
  }

  syncer::TestSyncService* sync_service() { return fake_sync_service_; }

  TestingProfile* profile() { return &profile_; }

  base::test::TaskEnvironment* task_env() { return &task_env_; }

  void SetUp() override {
    fake_sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&TestingSyncFactoryFunction)));
  }

 private:
  content::BrowserTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  raw_ptr<syncer::TestSyncService> fake_sync_service_;
};

TEST_F(LocalPasswordsMigrationWarningUtilTest,
       TestShouldNotShowWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  EXPECT_FALSE(local_password_migration::ShouldShowWarning(profile()));
}

TEST_F(LocalPasswordsMigrationWarningUtilTest,
       TestShouldShowWhenMoreThanAMonth) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  pref_service()->SetTime(
      password_manager::prefs::kLocalPasswordsMigrationWarningShownTimestamp,
      base::Time::Now());
  task_env()->FastForwardBy(base::Days(31));
  sync_service()->SetHasSyncConsent(false);
  EXPECT_TRUE(local_password_migration::ShouldShowWarning(profile()));
}

TEST_F(LocalPasswordsMigrationWarningUtilTest,
       TestShouldNotShowWhenLessThanAMonth) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  pref_service()->SetTime(
      password_manager::prefs::kLocalPasswordsMigrationWarningShownTimestamp,
      base::Time::Now());
  task_env()->FastForwardBy(base::Days(29));
  sync_service()->SetHasSyncConsent(false);
  EXPECT_FALSE(local_password_migration::ShouldShowWarning(profile()));
}

TEST_F(LocalPasswordsMigrationWarningUtilTest,
       TestShouldNotShowWhenAcknowledged) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  pref_service()->SetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning,
      true);
  EXPECT_FALSE(local_password_migration::ShouldShowWarning(profile()));
}

TEST_F(LocalPasswordsMigrationWarningUtilTest,
       TestShouldNotShowWhenSyncingOnlyPasswords) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  sync_service()->SetHasSyncConsent(true);
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /* sync_everything = */ false, {syncer::UserSelectableType::kPasswords});
  EXPECT_FALSE(local_password_migration::ShouldShowWarning(profile()));
}

TEST_F(LocalPasswordsMigrationWarningUtilTest,
       TestShouldNotShowWhenSyncingEverything) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  sync_service()->SetHasSyncConsent(true);
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /* sync_everything = */ true, {});
  EXPECT_FALSE(local_password_migration::ShouldShowWarning(profile()));
}

TEST_F(LocalPasswordsMigrationWarningUtilTest,
       TestShouldShowWhenNotSyncingPasswords) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  sync_service()->SetHasSyncConsent(true);
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /* sync_everything = */ false, {syncer::UserSelectableType::kBookmarks});
  EXPECT_TRUE(local_password_migration::ShouldShowWarning(profile()));
}

TEST_F(LocalPasswordsMigrationWarningUtilTest, TestShouldNotShowInIncognito) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /* sync_everything = */ false, {});
  TestingProfile::Builder off_the_record_builder;
  Profile* off_the_record_profile =
      off_the_record_builder.BuildIncognito(profile());

  EXPECT_FALSE(
      local_password_migration::ShouldShowWarning(off_the_record_profile));
}
