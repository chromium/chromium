// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_prefs.h"

#include "ash/public/cpp/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace full_restore {

// Unit tests for full_restore_prefs.
class FullRestorePrefsTest : public testing::Test {
 public:
  FullRestorePrefsTest()
      : user_manager_enabler_(std::make_unique<FakeChromeUserManager>()) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kFullRestore);
    pref_service_.reset(new sync_preferences::TestingPrefServiceSyncable);
  }

  user_prefs::PrefRegistrySyncable* registry() {
    return pref_service_->registry();
  }

  FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  RestoreOption GetRestoreOption() const {
    return static_cast<RestoreOption>(
        pref_service_->GetInteger(kRestoreAppsAndPagesPrefName));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  user_manager::ScopedUserManager user_manager_enabler_;
};

// For a brand new user, set 'ask every time' as the default value.
TEST_F(FullRestorePrefsTest, NewUser) {
  GetFakeUserManager()->set_current_user_new(true);
  RegisterProfilePrefs(registry());

  SetDefaultRestorePrefIfNecessary(pref_service_.get());
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
}

// When a user upgrades to the full restore release, set 'ask every time' as the
// default value if the browser setting is 'continue where you left off'.
TEST_F(FullRestorePrefsTest, UpgradingFromRestore) {
  SessionStartupPref::RegisterProfilePrefs(registry());
  pref_service_->SetInteger(
      prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueLast));

  RegisterProfilePrefs(registry());
  SetDefaultRestorePrefIfNecessary(pref_service_.get());
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
}

// When a user upgrades to the full restore release, set 'do not restore' as the
// default value if the browser setting is 'new tab'.
TEST_F(FullRestorePrefsTest, UpgradingFromNotRestore) {
  SessionStartupPref::RegisterProfilePrefs(registry());
  pref_service_->SetInteger(
      prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueNewTab));

  RegisterProfilePrefs(registry());
  SetDefaultRestorePrefIfNecessary(pref_service_.get());
  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
}

// For a new Chrome OS user, set 'always restore' as the default value if the
// browser setting is 'continue where you left off'.
TEST_F(FullRestorePrefsTest, NewChromeOSUserFromRestore) {
  GetFakeUserManager()->set_current_user_new(true);

  RegisterProfilePrefs(registry());
  SetDefaultRestorePrefIfNecessary(pref_service_.get());
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  SessionStartupPref::RegisterProfilePrefs(registry());
  pref_service_->SetInteger(
      prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueLast));

  UpdateRestorePrefIfNecessary(pref_service_.get());
  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());
}

// For a new Chrome OS user, set 'ask every time' as the default value if the
// browser setting is 'new tab'.
TEST_F(FullRestorePrefsTest, NewChromeOSUserFromNotRestore) {
  GetFakeUserManager()->set_current_user_new(true);

  RegisterProfilePrefs(registry());
  SetDefaultRestorePrefIfNecessary(pref_service_.get());
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());

  SessionStartupPref::RegisterProfilePrefs(registry());
  pref_service_->SetInteger(
      prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueNewTab));

  UpdateRestorePrefIfNecessary(pref_service_.get());
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
}

}  // namespace full_restore
}  // namespace chromeos
