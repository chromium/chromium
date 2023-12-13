// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager_android_util {
class PasswordManagerAndroidUtilTest : public testing::Test {
 public:
  PasswordManagerAndroidUtilTest() {
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
        false);
    pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendFalseWhenNotSyncingAndSplitStoresOff) {
  EXPECT_FALSE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ false, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendFalseWhenNotSyncingAndSplitStoresMigrationPending) {
  pref_service_.SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::
              kOffAndMigrationPending));

  EXPECT_FALSE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ false, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendTrueWhenNotSyncingAndSplitStoresOn) {
  pref_service_.SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_TRUE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ false, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendTrueWhenNotSyncingAndSplitStoresEnabledAndUnenrolled) {
  pref_service_.SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));

  pref_service_.SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  EXPECT_TRUE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ false, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendFalseWhenSyncingAndUnenrolled) {
  pref_service_.SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  EXPECT_FALSE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ true, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendTrueWhenSyncingAndSplitStoresDisabled) {
  EXPECT_TRUE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ true, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendTrueWhenSyncingAndSplitStoresEnabled) {
  pref_service_.SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_TRUE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ true, &pref_service_));
}

}  // namespace password_manager_android_util
