// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/login_db_deprecation_runner_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class LoginDbDeprecationRunnerFactoryTest : public testing::Test {
 public:
  LoginDbDeprecationRunnerFactoryTest() = default;

 protected:
  content::BrowserTaskEnvironment task_env_;
  TestingProfile testing_profile_;
};

TEST_F(LoginDbDeprecationRunnerFactoryTest, NullServiceIfMigrated) {
  PrefService* prefs = testing_profile_.GetPrefs();
  password_manager::SetLegacySplitStoresPrefForTest(prefs, true);
  EXPECT_FALSE(
      LoginDbDeprecationRunnerFactory::GetForProfile(&testing_profile_));
}

TEST_F(LoginDbDeprecationRunnerFactoryTest, NullIfAlreadyExported) {
  PrefService* prefs = testing_profile_.GetPrefs();
  password_manager::SetLegacySplitStoresPrefForTest(prefs, false);
  prefs->SetBoolean(password_manager::prefs::kUpmUnmigratedPasswordsExported,
                    true);
  EXPECT_FALSE(
      LoginDbDeprecationRunnerFactory::GetForProfile(&testing_profile_));
}

TEST_F(LoginDbDeprecationRunnerFactoryTest,
       NonNullServiceIfNotEligibleForMigration) {
  PrefService* prefs = testing_profile_.GetPrefs();
  password_manager::SetLegacySplitStoresPrefForTest(prefs, false);
  prefs->SetBoolean(password_manager::prefs::kUpmUnmigratedPasswordsExported,
                    false);
  EXPECT_TRUE(
      LoginDbDeprecationRunnerFactory::GetForProfile(&testing_profile_));
}
