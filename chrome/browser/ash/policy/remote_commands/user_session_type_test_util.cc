// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/user_session_type_test_util.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::test {

namespace {

constexpr char kTestAccountEmail[] = "test.email@example.com";

AccountId CreateUserOfType(TestSessionType session_type,
                           ash::FakeChromeUserManager& user_manager) {
  AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

  switch (session_type) {
    case TestSessionType::kManuallyLaunchedArcKioskSession:
      user_manager.AddArcKioskAppUser(account_id);
      CHECK_DEREF(ash::ArcKioskAppManager::Get())
          .set_current_app_was_auto_launched_with_zero_delay_for_testing(false);
      break;
    case TestSessionType::kManuallyLaunchedWebKioskSession:
      user_manager.AddWebKioskAppUser(account_id);
      CHECK_DEREF(ash::WebKioskAppManager::Get())
          .set_current_app_was_auto_launched_with_zero_delay_for_testing(false);
      break;
    case TestSessionType::kManuallyLaunchedKioskSession:
      user_manager.AddKioskAppUser(account_id);
      CHECK_DEREF(ash::KioskAppManager::Get())
          .set_current_app_was_auto_launched_with_zero_delay_for_testing(false);
      break;
    case TestSessionType::kAutoLaunchedArcKioskSession:
      user_manager.AddArcKioskAppUser(account_id);
      CHECK_DEREF(ash::ArcKioskAppManager::Get())
          .set_current_app_was_auto_launched_with_zero_delay_for_testing(true);
      break;
    case TestSessionType::kAutoLaunchedWebKioskSession:
      user_manager.AddWebKioskAppUser(account_id);
      CHECK_DEREF(ash::WebKioskAppManager::Get())
          .set_current_app_was_auto_launched_with_zero_delay_for_testing(true);
      break;
    case TestSessionType::kAutoLaunchedKioskSession:
      user_manager.AddKioskAppUser(account_id);
      CHECK_DEREF(ash::KioskAppManager::Get())
          .set_current_app_was_auto_launched_with_zero_delay_for_testing(true);
      break;
    case TestSessionType::kManagedGuestSession:
      user_manager.AddPublicAccountUser(account_id);
      break;
    case TestSessionType::kGuestSession:
      account_id = user_manager.AddGuestUser()->GetAccountId();
      break;
    case TestSessionType::kAffiliatedUserSession:
      user_manager.AddUserWithAffiliation(account_id,
                                          /*is_affiliated=*/true);
      break;
    case TestSessionType::kUnaffiliatedUserSession:
      user_manager.AddUserWithAffiliation(account_id,
                                          /*is_affiliated=*/false);
      break;
    case TestSessionType::kNoSession:
      ADD_FAILURE();
      break;
  }

  return account_id;
}

}  // namespace

const char* SessionTypeToString(TestSessionType session_type) {
#define CASE(type)            \
  case TestSessionType::type: \
    return #type

  switch (session_type) {
    CASE(kManuallyLaunchedArcKioskSession);
    CASE(kManuallyLaunchedWebKioskSession);
    CASE(kManuallyLaunchedKioskSession);
    CASE(kAutoLaunchedArcKioskSession);
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

  user_manager.LoginUser(CreateUserOfType(session_type, user_manager));
}

TestingProfile* StartSessionOfTypeWithProfile(
    TestSessionType session_type,
    ash::FakeChromeUserManager& user_manager,
    TestingProfileManager& profile_manager) {
  if (session_type == TestSessionType::kNoSession) {
    // Nothing to do if we don't need a session.
    return nullptr;
  }

  AccountId account_id = CreateUserOfType(session_type, user_manager);
  user_manager.LoginUser(account_id);
  return profile_manager.CreateTestingProfile(account_id.GetUserEmail(),
                                              /*is_main_profile=*/true);
}

}  // namespace policy::test
