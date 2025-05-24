// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/login_db_deprecation_runner_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class LoginDbDeprecationRunnerFactoryTest : public testing::Test {
 public:
  LoginDbDeprecationRunnerFactoryTest() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_env_;
  TestingProfile testing_profile_;
};

TEST_F(LoginDbDeprecationRunnerFactoryTest, NullServiceIfMigrated) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kLoginDbDeprecationAndroid);
  PrefService* prefs = testing_profile_.GetPrefs();
  prefs->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
  EXPECT_FALSE(
      LoginDbDeprecationRunnerFactory::GetForProfile(&testing_profile_));
}

TEST_F(LoginDbDeprecationRunnerFactoryTest, NullServiceIfFlagOff) {
  scoped_feature_list_.InitAndDisableFeature(
      password_manager::features::kLoginDbDeprecationAndroid);
  PrefService* prefs = testing_profile_.GetPrefs();
  prefs->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  prefs->SetBoolean(password_manager::prefs::kUpmUnmigratedPasswordsExported,
                    true);
  EXPECT_FALSE(
      LoginDbDeprecationRunnerFactory::GetForProfile(&testing_profile_));
}

TEST_F(LoginDbDeprecationRunnerFactoryTest, NullIfAlreadyExported) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kLoginDbDeprecationAndroid);
  PrefService* prefs = testing_profile_.GetPrefs();
  prefs->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  prefs->SetBoolean(password_manager::prefs::kUpmUnmigratedPasswordsExported,
                    true);
  EXPECT_FALSE(
      LoginDbDeprecationRunnerFactory::GetForProfile(&testing_profile_));
}

TEST_F(LoginDbDeprecationRunnerFactoryTest,
       NonNullServiceIfNotEligibleForMigration) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kLoginDbDeprecationAndroid);
  PrefService* prefs = testing_profile_.GetPrefs();
  prefs->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  prefs->SetBoolean(password_manager::prefs::kUpmUnmigratedPasswordsExported,
                    false);
  EXPECT_TRUE(
      LoginDbDeprecationRunnerFactory::GetForProfile(&testing_profile_));
}

TEST_F(LoginDbDeprecationRunnerFactoryTest, NonNullServiceIfMigrationPending) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kLoginDbDeprecationAndroid);
  PrefService* prefs = testing_profile_.GetPrefs();
  prefs->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::
              kOffAndMigrationPending));
  prefs->SetBoolean(password_manager::prefs::kUpmUnmigratedPasswordsExported,
                    false);
  EXPECT_TRUE(
      LoginDbDeprecationRunnerFactory::GetForProfile(&testing_profile_));
}
