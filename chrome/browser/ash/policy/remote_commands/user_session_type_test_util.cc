// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/user_session_type_test_util.h"
#include "base/check_deref.h"
#include "base/notreached.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::test {

namespace {

constexpr char kTestAccountEmail[] = "test.email@example.com";

const user_manager::User* CreateUserOfType(
    TestSessionType session_type,
    ash::FakeChromeUserManager& user_manager) {
  AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

  switch (session_type) {
    case TestSessionType::kManuallyLaunchedWebKioskSession:
      CHECK_DEREF(ash::WebKioskAppManager::Get())
          .set_current_app_was_auto_launched_with_zero_delay_for_testing(false);
      return user_manager.AddWebKioskAppUser(account_id);
    case TestSessionType::kManuallyLaunchedKioskSession:
      CHECK_DEREF(ash::KioskChromeAppManager::Get())
          .set_current_app_was_auto_launched_with_zero_delay_for_testing(false);
      return user_manager.AddKioskAppUser(account_id);
    case TestSessionType::kAutoLaunchedWebKioskSession:
      CHECK_DEREF(ash::WebKioskAppManager::Get())
          .set_current_app_was_auto_launched_with_zero_delay_for_testing(true);
      return user_manager.AddWebKioskAppUser(account_id);
    case TestSessionType::kAutoLaunchedKioskSession:
      CHECK_DEREF(ash::KioskChromeAppManager::Get())
          .set_current_app_was_auto_launched_with_zero_delay_for_testing(true);
      return user_manager.AddKioskAppUser(account_id);
    case TestSessionType::kManagedGuestSession:
      return user_manager.AddPublicAccountUser(account_id);
    case TestSessionType::kGuestSession:
      return user_manager.AddGuestUser();
    case TestSessionType::kAffiliatedUserSession:
      return user_manager.AddUserWithAffiliation(account_id,
                                                 /*is_affiliated=*/true);
    case TestSessionType::kUnaffiliatedUserSession:
      return user_manager.AddUserWithAffiliation(account_id,
                                                 /*is_affiliated=*/false);
    case TestSessionType::kNoSession:
      ADD_FAILURE();
      return nullptr;
  }

  NOTREACHED_IN_MIGRATION();
}

}  // namespace

const char* SessionTypeToString(TestSessionType session_type) {
#define CASE(type)            \
  case TestSessionType::type: \
    return #type

  switch (session_type) {
    CASE(kManuallyLaunchedWebKioskSession);
    CASE(kManuallyLaunchedKioskSession);
    CASE(kAutoLaunchedWebKioskSession);
    CASE(kAutoLaunchedKioskSession);
    CASE(kManagedGuestSession);
    CASE(kGuestSession);
    CASE(kAffiliatedUserSession);
    CASE(kUnaffiliatedUserSession);
    CASE(kNoSession);
  }

#undef CASE
}

void StartSessionOfType(TestSessionType session_type,
                        ash::FakeChromeUserManager& user_manager) {
  if (session_type == TestSessionType::kNoSession) {
    // Nothing to do if we don't need a session.
    return;
  }

  user_manager.LoginUser(
      CreateUserOfType(session_type, user_manager)->GetAccountId());
}

TestingProfile* StartSessionOfTypeWithProfile(
    TestSessionType session_type,
    ash::FakeChromeUserManager& user_manager,
    TestingProfileManager& profile_manager) {
  if (session_type == TestSessionType::kNoSession) {
    // Nothing to do if we don't need a session.
    return nullptr;
  }

  const user_manager::User* user = CreateUserOfType(session_type, user_manager);
  user_manager.LoginUser(user->GetAccountId());

  TestingProfile* profile = session_type == TestSessionType::kGuestSession
                                ? profile_manager.CreateGuestProfile()
                                : profile_manager.CreateTestingProfile(
                                      user->GetAccountId().GetUserEmail(),
                                      /*is_main_profile=*/true);
  ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);
  return profile;
}

}  // namespace policy::test
