// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_util.h"

#include <set>
#include <string>

#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/test/ash_test_base.h"
#include "ash/user_education/user_education_types.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::user_education_util {
namespace {

// Aliases.
using session_manager::SessionState;

}  // namespace

// UserEducationUtilTest -------------------------------------------------------

// Base class for tests of user education utilities.
using UserEducationUtilTest = NoSessionAshTestBase;

// Tests -----------------------------------------------------------------------

// Verifies that `GetAccountId()` is working as intended.
TEST_F(UserEducationUtilTest, GetAccountId) {
  // Case: null `UserSession`.
  AccountId account_id;
  EXPECT_EQ(GetAccountId(/*user_session=*/nullptr), account_id);

  // Case: non-null `UserSession`.
  account_id = AccountId::FromUserEmail("user@test");
  UserSession user_session;
  user_session.user_info.account_id = account_id;
  EXPECT_EQ(GetAccountId(&user_session), account_id);
}

// Verifies that `IsPrimaryAccountActive()` is working as intended.
TEST_F(UserEducationUtilTest, IsPrimaryAccountActive) {
  AccountId primary_account_id = AccountId::FromUserEmail("primary@test");
  AccountId secondary_account_id = AccountId::FromUserEmail("secondary@test");

  // Case: no user sessions added.
  EXPECT_FALSE(IsPrimaryAccountActive());

  // Case: primary user session added but inactive.
  auto* session_controller_client = GetSessionControllerClient();
  session_controller_client->AddUserSession(primary_account_id.GetUserEmail());
  EXPECT_FALSE(IsPrimaryAccountActive());

  // Case: primary user session activated.
  session_controller_client->SetSessionState(SessionState::ACTIVE);
  EXPECT_TRUE(IsPrimaryAccountActive());

  // Case: primary user session locked and then unlocked.
  session_controller_client->SetSessionState(SessionState::LOCKED);
  EXPECT_FALSE(IsPrimaryAccountActive());
  session_controller_client->SetSessionState(SessionState::ACTIVE);
  EXPECT_TRUE(IsPrimaryAccountActive());

  // Case: secondary user session added but inactive.
  session_controller_client->AddUserSession(
      secondary_account_id.GetUserEmail());
  EXPECT_TRUE(IsPrimaryAccountActive());

  // Case: secondary user activated and then deactivated.
  session_controller_client->SwitchActiveUser(secondary_account_id);
  EXPECT_FALSE(IsPrimaryAccountActive());
  session_controller_client->SwitchActiveUser(primary_account_id);
  EXPECT_TRUE(IsPrimaryAccountActive());
}

// Verifies that `IsPrimaryAccountId()` is working as intended.
TEST_F(UserEducationUtilTest, IsPrimaryAccountId) {
  AccountId primary_account_id = AccountId::FromUserEmail("primary@test");
  AccountId secondary_account_id = AccountId::FromUserEmail("secondary@test");

  // Case: no user sessions added.
  EXPECT_FALSE(IsPrimaryAccountId(AccountId()));
  EXPECT_FALSE(IsPrimaryAccountId(primary_account_id));
  EXPECT_FALSE(IsPrimaryAccountId(secondary_account_id));

  auto* session_controller_client = GetSessionControllerClient();
  session_controller_client->AddUserSession(primary_account_id.GetUserEmail());
  session_controller_client->AddUserSession(
      secondary_account_id.GetUserEmail());

  // Case: multiple user sessions added.
  EXPECT_FALSE(IsPrimaryAccountId(AccountId()));
  EXPECT_TRUE(IsPrimaryAccountId(primary_account_id));
  EXPECT_FALSE(IsPrimaryAccountId(secondary_account_id));
}

// Verifies that `ToString()` is working as intended.
TEST_F(UserEducationUtilTest, ToString) {
  std::set<std::string> tutorial_id_strs;
  for (size_t i = static_cast<size_t>(TutorialId::kMinValue);
       i <= static_cast<size_t>(TutorialId::kMaxValue); ++i) {
    // Currently the only constraint on `ToString()` is that it returns a unique
    // value for each distinct tutorial ID.
    auto tutorial_id_str = ToString(static_cast<TutorialId>(i));
    EXPECT_TRUE(tutorial_id_strs.emplace(std::move(tutorial_id_str)).second);
  }
}

}  // namespace ash::user_education_util
