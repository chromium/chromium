// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/full_restore_prefs.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/chrome_pref_names.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::full_restore {

// Unit tests for full_restore_prefs.
class FullRestorePrefsTest : public testing::Test {
 public:
  FullRestorePrefsTest() = default;

  void SetUp() override {
    test_user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(
            TestingBrowserProcess::GetGlobal()->local_state());
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  }

  void TearDown() override {
    pref_service_.reset();
    test_user_session_manager_.reset();
  }

  user_prefs::PrefRegistrySyncable* registry() {
    return pref_service_->registry();
  }

  RestoreOption GetRestoreOption() const {
    return static_cast<RestoreOption>(
        pref_service_->GetInteger(prefs::kRestoreAppsAndPagesPrefName));
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return pref_service_.get();
  }

 private:
  std::unique_ptr<ash::test::TestUserSessionManager> test_user_session_manager_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
};

// For a brand new user, set 'ask every time' as the default value.
TEST_F(FullRestorePrefsTest, NewUser) {
  user_manager::UserManager::Get()->SetIsCurrentUserNew(true);
  RegisterProfilePrefsFullRestore(registry());

  SetDefaultRestorePrefIfNecessary(pref_service());
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(pref_service()));
}

// When a user upgrades to the full restore release, set 'ask every time' as the
// default value if the browser setting is 'continue where you left off'.
TEST_F(FullRestorePrefsTest, UpgradingFromRestore) {
  SessionStartupPref::RegisterProfilePrefs(registry());
  pref_service()->SetInteger(
      ash::chrome_prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueLast));

  RegisterProfilePrefsFullRestore(registry());
  SetDefaultRestorePrefIfNecessary(pref_service());
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(pref_service()));
}

// When a user upgrades to the full restore release, set 'do not restore' as the
// default value if the browser setting is 'new tab'.
TEST_F(FullRestorePrefsTest, UpgradingFromNotRestore) {
  SessionStartupPref::RegisterProfilePrefs(registry());
  pref_service()->SetInteger(
      ash::chrome_prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueNewTab));

  RegisterProfilePrefsFullRestore(registry());
  SetDefaultRestorePrefIfNecessary(pref_service());
  EXPECT_EQ(RestoreOption::kDoNotRestore, GetRestoreOption());
  EXPECT_FALSE(CanPerformRestore(pref_service()));
}

// For a new Chrome OS user, set 'always restore' as the default value if the
// browser setting is 'continue where you left off'.
TEST_F(FullRestorePrefsTest, NewChromeOSUserFromRestore) {
  user_manager::UserManager::Get()->SetIsCurrentUserNew(true);

  RegisterProfilePrefsFullRestore(registry());
  SetDefaultRestorePrefIfNecessary(pref_service());
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(pref_service()));

  SessionStartupPref::RegisterProfilePrefs(registry());
  pref_service()->SetInteger(
      ash::chrome_prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueLast));

  UpdateRestorePrefIfNecessary(pref_service());
  EXPECT_EQ(RestoreOption::kAlways, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(pref_service()));
}

// For a new Chrome OS user, set 'ask every time' as the default value if the
// browser setting is 'new tab'.
TEST_F(FullRestorePrefsTest, NewChromeOSUserFromNotRestore) {
  user_manager::UserManager::Get()->SetIsCurrentUserNew(true);

  RegisterProfilePrefsFullRestore(registry());
  SetDefaultRestorePrefIfNecessary(pref_service());
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(pref_service()));

  SessionStartupPref::RegisterProfilePrefs(registry());
  pref_service()->SetInteger(
      ash::chrome_prefs::kRestoreOnStartup,
      static_cast<int>(SessionStartupPref::kPrefValueNewTab));

  UpdateRestorePrefIfNecessary(pref_service());
  EXPECT_EQ(RestoreOption::kAskEveryTime, GetRestoreOption());
  EXPECT_TRUE(CanPerformRestore(pref_service()));
}

}  // namespace ash::full_restore
