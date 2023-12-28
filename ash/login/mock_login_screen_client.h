// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_MOCK_LOGIN_SCREEN_CLIENT_H_
#define ASH_LOGIN_MOCK_LOGIN_SCREEN_CLIENT_H_

#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "ash/public/cpp/login_screen_client.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockLoginScreenClient : public LoginScreenClient {
 public:
  MockLoginScreenClient();

  MockLoginScreenClient(const MockLoginScreenClient&) = delete;
  MockLoginScreenClient& operator=(const MockLoginScreenClient&) = delete;

  ~MockLoginScreenClient() override;

  MOCK_METHOD(void,
              AuthenticateUserWithPasswordOrPin_,
              (const AccountId& account_id,
               const std::string& password,
               bool authenticated_by_pin,
               base::OnceCallback<void(bool)>& callback));
  MOCK_METHOD(void,
              AuthenticateUserWithChallengeResponse_,
              (const AccountId& account_id,
               base::OnceCallback<void(bool)>& callback));
  MOCK_METHOD(ParentCodeValidationResult,
              ValidateParentAccessCode_,
              (const AccountId& account_id,
               const std::string& access_code,
               base::Time validation_time));

  // Set the result that should be passed to |callback| in
  // |AuthenticateUserWithPasswordOrPin|.
  void set_authenticate_user_callback_result(bool value) {
    authenticate_user_callback_result_ = value;
  }

  // Sets the result that should be passed to |callback| in
  // |ValidateParentAccessCode|.
  void set_validate_parent_access_code_result(
      ParentCodeValidationResult value) {
    validate_parent_access_code_result_ = value;
  }

  // If set to non-null, when |AuthenticateUser| is called the callback will be
  // stored in |storage| instead of being executed.
  void set_authenticate_user_with_password_or_pin_callback_storage(
      base::OnceCallback<void(bool)>* storage) {
    authenticate_user_with_password_or_pin_callback_storage_ = storage;
  }

  // LoginScreenClient:
  void AuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) override;
  void AuthenticateUserWithChallengeResponse(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  ParentCodeValidationResult ValidateParentAccessCode(
      const AccountId& account_id,
      const std::string& code,
      base::Time validation_time) override;
  MOCK_METHOD(void,
              AuthenticateUserWithEasyUnlock,
              (const AccountId& account_id),
              (override));
  MOCK_METHOD(void, OnFocusPod, (const AccountId& account_id), (override));
  MOCK_METHOD(void, CancelAddUser, (), (override));
  MOCK_METHOD(void, ShowGuestTosScreen, (), (override));
  MOCK_METHOD(void,
              OnMaxIncorrectPasswordAttempted,
              (const AccountId& account_id),
              (override));
  MOCK_METHOD(void, FocusLockScreenApps, (bool reverse), (override));
  MOCK_METHOD(void,
              ShowGaiaSignin,
              (const AccountId& prefilled_account),
              (override));
  MOCK_METHOD(void,
              StartUserRecovery,
              (const AccountId& account_to_recover),
              (override));
  MOCK_METHOD(void, ShowOsInstallScreen, (), (override));
  MOCK_METHOD(void, OnRemoveUserWarningShown, (), (override));
  MOCK_METHOD(void, RemoveUser, (const AccountId& account_id), (override));
  MOCK_METHOD(void,
              LaunchPublicSession,
              (const AccountId& account_id,
               const std::string& locale,
               const std::string& input_method),
              (override));
  MOCK_METHOD(void,
              RequestPublicSessionKeyboardLayouts,
              (const AccountId& account_id, const std::string& locale),
              (override));
  MOCK_METHOD(void,
              HandleAccelerator,
              (ash::LoginAcceleratorAction action),
              (override));
  MOCK_METHOD(void, ShowAccountAccessHelpApp, (gfx::NativeWindow), (override));
  MOCK_METHOD(void, ShowParentAccessHelpApp, (), (override));
  MOCK_METHOD(void, ShowLockScreenNotificationSettings, (), (override));
  MOCK_METHOD(void, FocusOobeDialog, (), (override));
  MOCK_METHOD(void, OnFocusLeavingSystemTray, (bool reverse), (override));
  MOCK_METHOD(void, OnLoginScreenShown, (), (override));
  MOCK_METHOD(void, OnSystemTrayBubbleShown, (), (override));
  MOCK_METHOD(views::Widget*, GetLoginWindowWidget, (), (override));

 private:
  bool authenticate_user_callback_result_ = true;
  ParentCodeValidationResult validate_parent_access_code_result_ =
      ParentCodeValidationResult::kValid;
  raw_ptr<base::OnceCallback<void(bool)>>
      authenticate_user_with_password_or_pin_callback_storage_ = nullptr;
};

}  // namespace ash

#endif  // ASH_LOGIN_MOCK_LOGIN_SCREEN_CLIENT_H_
