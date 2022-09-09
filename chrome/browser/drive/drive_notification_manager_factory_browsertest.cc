// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_notification_manager_factory.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

class DriveNotificationManagerFactoryLoginScreenBrowserTest
    : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
  }
};

// Verify that no DriveNotificationManager is instantiated for the sign-in
// profile on the login screen.
IN_PROC_BROWSER_TEST_F(DriveNotificationManagerFactoryLoginScreenBrowserTest,
                       NoDriveNotificationManager) {
  Profile* signin_profile =
      ash::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
  EXPECT_FALSE(DriveNotificationManagerFactory::FindForBrowserContext(
      signin_profile));
}

class DriveNotificationManagerFactoryGuestBrowserTest
    : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(
        ash::switches::kLoginUser,
        user_manager::GuestAccountId().GetUserEmail());
  }
};

// Verify that no DriveNotificationManager is instantiated for the sign-in
// profile or the guest profile while a guest session is in progress.
IN_PROC_BROWSER_TEST_F(DriveNotificationManagerFactoryGuestBrowserTest,
                       NoDriveNotificationManager) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());
  Profile* guest_profile = ash::ProfileHelper::Get()
                               ->GetProfileByUser(user_manager->GetActiveUser())
                               ->GetOriginalProfile();
  Profile* signin_profile =
      ash::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
  EXPECT_FALSE(DriveNotificationManagerFactory::FindForBrowserContext(
      guest_profile));
  EXPECT_FALSE(DriveNotificationManagerFactory::FindForBrowserContext(
      signin_profile));
}

}  // namespace drive
