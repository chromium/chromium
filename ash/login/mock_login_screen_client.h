// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_MOCK_LOGIN_SCREEN_CLIENT_H_
#define ASH_LOGIN_MOCK_LOGIN_SCREEN_CLIENT_H_

#include "ash/public/interfaces/kiosk_app_info.mojom.h"
#include "ash/public/interfaces/login_screen.mojom.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockLoginScreenClient : public mojom::LoginScreenClient {
 public:
  MockLoginScreenClient();
  ~MockLoginScreenClient() override;

  mojom::LoginScreenClientPtr CreateInterfacePtrAndBind();

  MOCK_METHOD4(AuthenticateUserWithPasswordOrPin_,
               void(const AccountId& account_id,
                    const std::string& password,
                    bool authenticated_by_pin,
                    AuthenticateUserWithPasswordOrPinCallback& callback));
  MOCK_METHOD2(AuthenticateUserWithExternalBinary_,
               void(const AccountId& account_id,
                    AuthenticateUserWithExternalBinaryCallback& callback));
  MOCK_METHOD1(EnrollUserWithExternalBinary_,
               void(EnrollUserWithExternalBinaryCallback& callback));

  // Set the result that should be passed to |callback| in
  // |AuthenticateUserWithPasswordOrPin| or
  // |AuthenticateUserWithExternalBinary|.
  void set_authenticate_user_callback_result(bool value) {
    authenticate_user_callback_result_ = value;
  }

  // If set to non-null, when |AuthenticateUser| is called the callback will be
  // stored in |storage| instead of being executed.
  void set_authenticate_user_with_password_or_pin_callback_storage(
      AuthenticateUserWithPasswordOrPinCallback* storage) {
    authenticate_user_with_password_or_pin_callback_storage_ = storage;
  }
  void set_authenticate_user_with_external_binary_storage(
      AuthenticateUserWithPasswordOrPinCallback* storage) {
    authenticate_user_with_external_binary_callback_storage_ = storage;
  }
  void set_enroll_user_with_external_binary_storage(
      EnrollUserWithExternalBinaryCallback* storage) {
    enroll_user_with_external_binary_callback_storage_ = storage;
  }

  // mojom::LoginScreenClient:
  void AuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      AuthenticateUserWithPasswordOrPinCallback callback) override;
  void AuthenticateUserWithExternalBinary(
      const AccountId& account_id,
      AuthenticateUserWithExternalBinaryCallback callback) override;
  void EnrollUserWithExternalBinary(
      EnrollUserWithExternalBinaryCallback callback) override;
  MOCK_METHOD1(AuthenticateUserWithEasyUnlock,
               void(const AccountId& account_id));
  MOCK_METHOD1(HardlockPod, void(const AccountId& account_id));
  MOCK_METHOD1(OnFocusPod, void(const AccountId& account_id));
  MOCK_METHOD0(OnNoPodFocused, void());
  MOCK_METHOD1(LoadWallpaper, void(const AccountId& account_id));
  MOCK_METHOD0(SignOutUser, void());
  MOCK_METHOD0(CancelAddUser, void());
  MOCK_METHOD0(LoginAsGuest, void());
  MOCK_METHOD1(OnMaxIncorrectPasswordAttempted,
               void(const AccountId& account_id));
  MOCK_METHOD1(FocusLockScreenApps, void(bool reverse));
  MOCK_METHOD2(ShowGaiaSignin,
               void(bool can_close,
                    const base::Optional<AccountId>& prefilled_account));
  MOCK_METHOD0(OnRemoveUserWarningShown, void());
  MOCK_METHOD1(RemoveUser, void(const AccountId& account_id));
  MOCK_METHOD3(LaunchPublicSession,
               void(const AccountId& account_id,
                    const std::string& locale,
                    const std::string& input_method));
  MOCK_METHOD2(RequestPublicSessionKeyboardLayouts,
               void(const AccountId& account_id, const std::string& locale));
  MOCK_METHOD0(ShowFeedback, void());
  MOCK_METHOD1(LaunchKioskApp, void(const std::string& app_id));
  MOCK_METHOD1(LaunchArcKioskApp, void(const AccountId& account_id));
  MOCK_METHOD0(ShowResetScreen, void());
  MOCK_METHOD0(ShowAccountAccessHelpApp, void());
  MOCK_METHOD0(FocusOobeDialog, void());

 private:
  bool authenticate_user_callback_result_ = true;
  AuthenticateUserWithPasswordOrPinCallback*
      authenticate_user_with_password_or_pin_callback_storage_ = nullptr;
  AuthenticateUserWithExternalBinaryCallback*
      authenticate_user_with_external_binary_callback_storage_ = nullptr;
  EnrollUserWithExternalBinaryCallback*
      enroll_user_with_external_binary_callback_storage_ = nullptr;

  mojo::Binding<mojom::LoginScreenClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockLoginScreenClient);
};

// Helper method to bind a login screen client so it receives all mojo calls.
std::unique_ptr<MockLoginScreenClient> BindMockLoginScreenClient();

}  // namespace ash

#endif  // ASH_LOGIN_MOCK_LOGIN_SCREEN_CLIENT_H_
