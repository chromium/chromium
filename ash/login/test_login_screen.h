// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_TEST_LOGIN_SCREEN_H_
#define ASH_LOGIN_TEST_LOGIN_SCREEN_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/login/test_login_screen_model.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/management_disclosure_client.h"

namespace ash {
class ScopedGuestButtonBlocker;
enum class SupervisedAction;
}  // namespace ash

// Test implementation of ash's mojo LoginScreen interface.
class ASH_EXPORT TestLoginScreen : public ash::LoginScreen {
 public:
  TestLoginScreen();

  TestLoginScreen(const TestLoginScreen&) = delete;
  TestLoginScreen& operator=(const TestLoginScreen&) = delete;

  ~TestLoginScreen() override;

  // ash::LoginScreen:
  void SetClient(ash::LoginScreenClient* client) override;
  ash::LoginScreenModel* GetModel() override;
  void ShowLockScreen() override;
  void ShowLoginScreen() override;
  void ShowKioskAppError(const std::string& message) override;
  void FocusLoginShelf(bool reverse) override;
  bool IsReadyForPassword() override;
  void EnableAddUserButton(bool enable) override;
  void EnableShutdownButton(bool enable) override;
  void EnableShelfButtons(bool enable) override;
  void SetIsFirstSigninStep(bool is_first) override;
  void ShowParentAccessButton(bool show) override;
  void SetAllowLoginAsGuest(bool allow_guest) override;
  std::unique_ptr<ash::ScopedGuestButtonBlocker> GetScopedGuestButtonBlocker()
      override;
  void RequestSecurityTokenPin(ash::SecurityTokenPinRequest request) override;
  void ClearSecurityTokenPinRequest() override;
  views::Widget* GetLoginWindowWidget() override;
  void SetManagementDisclosureClient(
      ash::ManagementDisclosureClient* client) override;

 private:
  TestLoginScreenModel test_screen_model_;
};

#endif  // ASH_LOGIN_TEST_LOGIN_SCREEN_H_
