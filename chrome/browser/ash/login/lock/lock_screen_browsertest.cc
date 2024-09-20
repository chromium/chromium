// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {
namespace {

class LockScreenBaseTest : public LoginManagerTest {
 public:
  LockScreenBaseTest() = default;

  LockScreenBaseTest(const LockScreenBaseTest&) = delete;
  LockScreenBaseTest& operator=(const LockScreenBaseTest&) = delete;

  ~LockScreenBaseTest() override = default;

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    input_method::InputMethodManager::Get()->GetMigratedInputMethodIDs(
        &user_input_methods_);
  }

 protected:
  std::vector<std::string> user_input_methods_;
  LoginManagerMixin login_manager_{&mixin_host_};
};

class LockScreenInputsTest : public LockScreenBaseTest {
 public:
  LockScreenInputsTest() {
    login_manager_.AppendRegularUsers(2);
    user_input_methods_.push_back("xkb:fr::fra");
    user_input_methods_.push_back("xkb:de::ger");
  }
};

IN_PROC_BROWSER_TEST_F(LockScreenInputsTest, CheckIMESwitches) {
  const auto& users = login_manager_.users();
  SetExpectedCredentialsWithDbusClient(users[0].account_id,
                                       LoginManagerTest::kPassword);
  SetExpectedCredentialsWithDbusClient(users[1].account_id,
                                       LoginManagerTest::kPassword);
  LoginUserWithDbusClient(users[0].account_id, LoginManagerTest::kPassword);
  scoped_refptr<input_method::InputMethodManager::State> ime_states[2] = {
      nullptr, nullptr};
  input_method::InputMethodManager* input_manager =
      input_method::InputMethodManager::Get();
  ime_states[0] = input_manager->GetActiveIMEState();
  ASSERT_TRUE(ime_states[0]->EnableInputMethod(user_input_methods_[0]));
  ime_states[0]->ChangeInputMethod(user_input_methods_[0], false);
  EXPECT_EQ(ime_states[0]->GetCurrentInputMethod().id(),
            user_input_methods_[0]);

  UserAddingScreen::Get()->Start();
  AddUserWithDbusClient(users[1].account_id, LoginManagerTest::kPassword);
  EXPECT_EQ(users[1].account_id,
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  ime_states[1] = input_manager->GetActiveIMEState();
  ASSERT_TRUE(ime_states[1]->EnableInputMethod(user_input_methods_[1]));
  ime_states[1]->ChangeInputMethod(user_input_methods_[1], false);
  EXPECT_EQ(ime_states[1]->GetCurrentInputMethod().id(),
            user_input_methods_[1]);

  ASSERT_NE(ime_states[0], ime_states[1]);

  ScreenLockerTester locker_tester;
  locker_tester.Lock();
  EXPECT_EQ(2, LoginScreenTestApi::GetUsersCount());
  // IME state should be lock screen specific.
  EXPECT_NE(ime_states[0], input_manager->GetActiveIMEState());
  EXPECT_NE(ime_states[1], input_manager->GetActiveIMEState());

  EXPECT_EQ(users[0].account_id, LoginScreenTestApi::GetFocusedUser());
  EXPECT_EQ(input_manager->GetActiveIMEState()->GetCurrentInputMethod().id(),
            user_input_methods_[0]);
  locker_tester.UnlockWithPassword(users[0].account_id,
                                   LoginManagerTest::kPassword);
  locker_tester.WaitForUnlock();
  EXPECT_EQ(users[0].account_id,
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  EXPECT_EQ(ime_states[0], input_manager->GetActiveIMEState());
  EXPECT_EQ(ime_states[0]->GetCurrentInputMethod().id(),
            user_input_methods_[0]);

  locker_tester.Lock();
  EXPECT_EQ(2, LoginScreenTestApi::GetUsersCount());
  // IME state should be lock screen specific.
  EXPECT_NE(ime_states[0], input_manager->GetActiveIMEState());
  EXPECT_NE(ime_states[1], input_manager->GetActiveIMEState());

  EXPECT_EQ(users[0].account_id, LoginScreenTestApi::GetFocusedUser());
  EXPECT_EQ(input_manager->GetActiveIMEState()->GetCurrentInputMethod().id(),
            user_input_methods_[0]);
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(users[1].account_id));
  EXPECT_EQ(input_manager->GetActiveIMEState()->GetCurrentInputMethod().id(),
            user_input_methods_[1]);
  locker_tester.UnlockWithPassword(users[1].account_id,
                                   LoginManagerTest::kPassword);
  EXPECT_EQ(users[1].account_id,
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  EXPECT_EQ(ime_states[1], input_manager->GetActiveIMEState());
  EXPECT_EQ(ime_states[1]->GetCurrentInputMethod().id(),
            user_input_methods_[1]);
}

class LockScreenFilterInputTest : public LockScreenBaseTest {
 public:
  LockScreenFilterInputTest() {
    login_manager_.AppendRegularUsers(1);
    // Lock screen input metnod.
    user_input_methods_.push_back("xkb:fr::fra");

    // Input method not valid on the lock screen - not latin.
    user_input_methods_.push_back("xkb:ru::rus");
  }

  // LockScreenBaseTest:
  void SetUpOnMainThread() override {
    // Migrate user_input_methods_ first.
    LockScreenBaseTest::SetUpOnMainThread();

    valid_lock_screen_method_ = user_input_methods_[0];
    not_valid_lock_screen_method_ = user_input_methods_[1];
  }
  std::string valid_lock_screen_method_;
  std::string not_valid_lock_screen_method_;
};

IN_PROC_BROWSER_TEST_F(LockScreenFilterInputTest, Basic) {
  const AccountId test_account_id = login_manager_.users().front().account_id;
  LoginUser(test_account_id);
  input_method::InputMethodManager* input_manager =
      input_method::InputMethodManager::Get();

  auto user_ime_state = input_manager->GetActiveIMEState();
  for (const auto& method : user_input_methods_)
    ASSERT_TRUE(user_ime_state->EnableInputMethod(method));
  // We need to change input method to propagate it to InputMethodPersistence ->
  // lock_screen_utils::SetUserInputMethod
  user_ime_state->ChangeInputMethod(valid_lock_screen_method_, false);

  EXPECT_EQ(user_ime_state->GetNumEnabledInputMethods(), 3u);

  ScreenLockerTester locker_tester;
  locker_tester.Lock();
  auto lock_screen_ime_state = input_manager->GetActiveIMEState();
  EXPECT_NE(user_ime_state, lock_screen_ime_state);
  // Not valid method should be filtered out.
  EXPECT_EQ(lock_screen_ime_state->GetNumEnabledInputMethods(), 2u);

  EXPECT_TRUE(base::Contains(lock_screen_ime_state->GetEnabledInputMethodIds(),
                             valid_lock_screen_method_));
  EXPECT_FALSE(base::Contains(lock_screen_ime_state->GetEnabledInputMethodIds(),
                              not_valid_lock_screen_method_));

  // Check that input methods are restored in the session.
  locker_tester.UnlockWithPassword(test_account_id,
                                   LoginManagerTest::kPassword);
  locker_tester.WaitForUnlock();
  EXPECT_EQ(input_manager->GetActiveIMEState(), user_ime_state);

  EXPECT_EQ(user_ime_state->GetNumEnabledInputMethods(), 3u);
  EXPECT_TRUE(base::Contains(user_ime_state->GetEnabledInputMethodIds(),
                             valid_lock_screen_method_));
  EXPECT_TRUE(base::Contains(user_ime_state->GetEnabledInputMethodIds(),
                             not_valid_lock_screen_method_));
}

// DeviceLoginScreenInputMethods policy should not affect lock screen.
class LockScreenDevicePolicyInputsTest : public LockScreenBaseTest {
 public:
  LockScreenDevicePolicyInputsTest() {
    login_manager_.AppendRegularUsers(1);
    // Lock screen input metnod.
    user_input_methods_.push_back("xkb:fr::fra");
  }

  // LockScreenBaseTest:
  void SetUpOnMainThread() override {
    LockScreenBaseTest::SetUpOnMainThread();

    // Setup device policy.
    namespace em = enterprise_management;
    em::ChromeDeviceSettingsProto& proto(
        policy_helper_.device_policy()->payload());
    proto.mutable_login_screen_input_methods()->add_login_screen_input_methods(
        allowed_input_method.front());
    policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {kDeviceLoginScreenInputMethods});

    input_method::InputMethodManager::Get()->GetMigratedInputMethodIDs(
        &allowed_input_method);
  }

 protected:
  std::vector<std::string> allowed_input_method{"xkb:de::ger"};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  policy::DevicePolicyCrosTestHelper policy_helper_;
};

IN_PROC_BROWSER_TEST_F(LockScreenDevicePolicyInputsTest, PolicyNotHonored) {
  const AccountId test_account_id = login_manager_.users().front().account_id;
  input_method::InputMethodManager* input_manager =
      input_method::InputMethodManager::Get();
  // Check that policy applies on the login screen.
  EXPECT_EQ(input_manager->GetActiveIMEState()->GetEnabledInputMethodIds(),
            allowed_input_method);

  LoginUser(test_account_id);
  auto user_ime_state = input_manager->GetActiveIMEState();
  for (const auto& method : user_input_methods_)
    ASSERT_TRUE(user_ime_state->EnableInputMethod(method));
  // We need to change input method to propagate it to InputMethodPersistence ->
  // lock_screen_utils::SetUserInputMethod
  user_ime_state->ChangeInputMethod(user_input_methods_[0], false);

  ScreenLockerTester locker_tester;
  locker_tester.Lock();

  // Inputs should stay the same as inside the session.
  EXPECT_EQ(input_manager->GetActiveIMEState()->GetEnabledInputMethodIds(),
            user_ime_state->GetEnabledInputMethodIds());

  EXPECT_EQ(
      input_manager->GetActiveIMEState()->GetAllowedInputMethodIds().size(),
      0u);
}

class LockScreenLocalPasswordTest : public LockScreenBaseTest {
 public:
  LockScreenLocalPasswordTest() {
    login_manager_.AppendRegularUsers(1);
    user_input_methods_.push_back("xkb:de::ger");
  }
};

IN_PROC_BROWSER_TEST_F(LockScreenLocalPasswordTest, UnlockWithCorrectPassword) {
  const AccountId test_account_id = login_manager_.users().front().account_id;
  LoginUserWithLocalPassword(test_account_id);

  ScreenLockerTester locker_tester;
  locker_tester.Lock();

  // Unlock with Local password, the same as was used for login.
  locker_tester.UnlockWithPassword(test_account_id,
                                   LoginManagerTest::kLocalPassword);
  locker_tester.WaitForUnlock();
  EXPECT_EQ(test_account_id,
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
}

IN_PROC_BROWSER_TEST_F(LockScreenLocalPasswordTest, UnlockWithWrongPassword) {
  const AccountId test_account_id = login_manager_.users().front().account_id;
  LoginUserWithLocalPassword(test_account_id);

  ScreenLockerTester locker_tester;
  locker_tester.Lock();

  // Unlock with a bad password.
  locker_tester.UnlockWithPassword(test_account_id,
                                   LoginManagerTest::kPassword);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(locker_tester.IsLocked());
}

}  // namespace
}  // namespace ash
