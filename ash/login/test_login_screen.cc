// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/test_login_screen.h"

#include <memory>

#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "ash/public/cpp/scoped_guest_button_blocker.h"

namespace {
class ScopedGuestButtonBlockerTestImpl : public ash::ScopedGuestButtonBlocker {
 public:
  ScopedGuestButtonBlockerTestImpl() = default;
  ~ScopedGuestButtonBlockerTestImpl() override = default;
};
}  // namespace

TestLoginScreen::TestLoginScreen() = default;

TestLoginScreen::~TestLoginScreen() = default;

void TestLoginScreen::SetClient(ash::LoginScreenClient* client) {}

ash::LoginScreenModel* TestLoginScreen::GetModel() {
  return &test_screen_model_;
}

void TestLoginScreen::ShowLockScreen() {}

void TestLoginScreen::ShowLoginScreen() {}

void TestLoginScreen::ShowKioskAppError(const std::string& message) {}

void TestLoginScreen::FocusLoginShelf(bool reverse) {}

bool TestLoginScreen::IsReadyForPassword() {
  return true;
}

void TestLoginScreen::EnableAddUserButton(bool enable) {}

void TestLoginScreen::EnableShutdownButton(bool enable) {}

void TestLoginScreen::EnableShelfButtons(bool enable) {}

void TestLoginScreen::SetIsFirstSigninStep(bool is_first) {}

void TestLoginScreen::ShowParentAccessButton(bool show) {}

void TestLoginScreen::SetAllowLoginAsGuest(bool allow_guest) {}

void TestLoginScreen::SetManagementDisclosureClient(
    ash::ManagementDisclosureClient* client) {}

std::unique_ptr<ash::ScopedGuestButtonBlocker>
TestLoginScreen::GetScopedGuestButtonBlocker() {
  return std::make_unique<ScopedGuestButtonBlockerTestImpl>();
}

void TestLoginScreen::RequestSecurityTokenPin(
    ash::SecurityTokenPinRequest request) {}

void TestLoginScreen::ClearSecurityTokenPinRequest() {}

views::Widget* TestLoginScreen::GetLoginWindowWidget() {
  return nullptr;
}
