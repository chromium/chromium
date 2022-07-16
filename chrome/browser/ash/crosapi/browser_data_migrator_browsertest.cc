// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

namespace ash {
class BrowserDataMigratorRestartTest : public ash::LoginManagerTest {
 public:
  BrowserDataMigratorRestartTest() = default;
  BrowserDataMigratorRestartTest(BrowserDataMigratorRestartTest&) = delete;
  BrowserDataMigratorRestartTest& operator=(BrowserDataMigratorRestartTest&) =
      delete;
  ~BrowserDataMigratorRestartTest() override = default;

  // ash::LoginManagerTest:
  void SetUp() override {
    if (content::IsPreTest()) {
      feature_list_.InitAndDisableFeature(chromeos::features::kLacrosSupport);
    } else {
      feature_list_.InitAndEnableFeature(chromeos::features::kLacrosSupport);
    }

    login_manager_.AppendRegularUsers(1);
    // This allows chrome to startup with the session info from
    // `PRE_MigrateOnRestart` without actually needing to go through the login
    // screen on `MigrateOnRestart`.
    login_manager_.set_session_restore_enabled();

    ash::LoginManagerTest::SetUp();
  }

  void LoginAsRegularUser() {
    const auto& users = login_manager_.users();

    LoginUser(users[0].account_id);
  }

 private:
  LoginManagerMixin login_manager_{&mixin_host_};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserDataMigratorRestartTest, PRE_MigrateOnRestart) {
  LoginAsRegularUser();
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    const base::FilePath new_user_data_directory =
        profile->GetPath().Append(kLacrosDir);
    // Make sure that lacros directory does not exist before migration.
    ASSERT_FALSE(base::DirectoryExists(new_user_data_directory));
    ASSERT_FALSE(base::PathExists(
        new_user_data_directory.Append(chrome::kPreferencesFilename)));
  }
}

IN_PROC_BROWSER_TEST_F(BrowserDataMigratorRestartTest, MigrateOnRestart) {
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    const base::FilePath new_user_data_directory =
        profile->GetPath().Append(kLacrosDir);
    // Check that the new profile data directory is created.
    ASSERT_TRUE(base::DirectoryExists(new_user_data_directory));
    ASSERT_TRUE(
        base::PathExists(new_user_data_directory.Append(kLacrosProfilePath)
                             .Append(chrome::kPreferencesFilename)));
  }
}
}  // namespace ash
