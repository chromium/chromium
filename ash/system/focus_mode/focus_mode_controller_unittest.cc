// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@focusmode";
constexpr char kUser2Email[] = "user2@focusmode";

}  // namespace

class FocusModeControllerMultiUserTest : public NoSessionAshTestBase {
 public:
  FocusModeControllerMultiUserTest() : scoped_feature_(features::kFocusMode) {}
  ~FocusModeControllerMultiUserTest() override = default;

  TestingPrefServiceSimple* user_1_prefs() { return user_1_prefs_; }
  TestingPrefServiceSimple* user_2_prefs() { return user_2_prefs_; }

  // NoSessionAshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();

    // Inject our own PrefServices for each user which enables us to setup the
    // Focus Mode restore data before the user signs in.
    auto user_1_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_1_prefs_ = user_1_prefs.get();
    RegisterUserProfilePrefs(user_1_prefs_->registry(), /*country=*/"",
                             /*for_test=*/true);
    auto user_2_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_2_prefs_ = user_2_prefs.get();
    RegisterUserProfilePrefs(user_2_prefs_->registry(), /*country=*/"",
                             /*for_test=*/true);
    session_controller->AddUserSession(kUser1Email,
                                       user_manager::USER_TYPE_REGULAR,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser1AccountId(),
                                           std::move(user_1_prefs));
    session_controller->AddUserSession(kUser2Email,
                                       user_manager::USER_TYPE_REGULAR,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser2AccountId(),
                                           std::move(user_2_prefs));
  }

  void TearDown() override {
    user_1_prefs_ = nullptr;
    user_2_prefs_ = nullptr;
    NoSessionAshTestBase::TearDown();
  }

  AccountId GetUser1AccountId() const {
    return AccountId::FromUserEmail(kUser1Email);
  }

  AccountId GetUser2AccountId() const {
    return AccountId::FromUserEmail(kUser2Email);
  }

  void SwitchActiveUser(const AccountId& account_id) {
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  void SimulateUserLogin(const AccountId& account_id) {
    SwitchActiveUser(account_id);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_;
  raw_ptr<TestingPrefServiceSimple> user_1_prefs_ = nullptr;
  raw_ptr<TestingPrefServiceSimple> user_2_prefs_ = nullptr;
};

// Tests that the default Focus Mode prefs are registered, and that they are
// read correctly by `FocusModeController`. Also test that switching users will
// load new user prefs.
TEST_F(FocusModeControllerMultiUserTest, LoadUserPrefsAndSwitchUsers) {
  constexpr base::TimeDelta kDefaultSessionDuration = base::Minutes(25);
  constexpr bool kDefaultDNDState = true;
  constexpr base::TimeDelta kUser2SessionDuration = base::Minutes(200);
  constexpr bool kUser2DNDState = false;

  // Set the secondary user2's Focus Mode prefs.
  user_2_prefs()->SetTimeDelta(prefs::kFocusModeSessionDuration,
                               kUser2SessionDuration);
  user_2_prefs()->SetBoolean(prefs::kFocusModeDoNotDisturb, kUser2DNDState);

  // Log in and check to see that the user1 prefs are the default values, since
  // there should have been nothing previously.
  SimulateUserLogin(GetUser1AccountId());
  EXPECT_EQ(kDefaultSessionDuration,
            user_1_prefs()->GetTimeDelta(prefs::kFocusModeSessionDuration));
  EXPECT_EQ(kDefaultDNDState,
            user_1_prefs()->GetBoolean(prefs::kFocusModeDoNotDisturb));

  // Verify that `FocusModeController` has loaded the user prefs.
  auto* controller = FocusModeController::Get();
  EXPECT_EQ(kDefaultSessionDuration, controller->session_duration());
  EXPECT_EQ(kDefaultDNDState, controller->turn_on_do_not_disturb());

  // Switch users and verify that `FocusModeController` has loaded the new user
  // prefs.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_EQ(kUser2SessionDuration, controller->session_duration());
  EXPECT_EQ(kUser2DNDState, controller->turn_on_do_not_disturb());
}

}  // namespace ash
