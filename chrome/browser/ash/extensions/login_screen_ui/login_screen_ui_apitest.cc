// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/ash/extensions/login_screen_ui/ui_handler.h"
#include "chrome/browser/ash/policy/login/signin_profile_extensions_policy_test_base.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.h"
#include "chrome/browser/ui/ash/login/login_screen_extension_ui/dialog_delegate.h"
#include "chrome/browser/ui/ash/login/login_screen_extension_ui/window.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

constexpr char kCanOpenWindow[] = "LoginScreenUiCanOpenWindow";
constexpr char kCannotOpenMultipleWindows[] =
    "LoginScreenUiCannotOpenMultipleWindows";
constexpr char kCanOpenAndCloseWindow[] = "LoginScreenUiCanOpenAndCloseWindow";
constexpr char kCannotCloseNoWindow[] = "LoginScreenUiCannotCloseNoWindow";
constexpr char kUserCanCloseWindow[] = "LoginScreenUiUserCanCloseWindow";
constexpr char kUserCannotCloseWindow[] = "LoginScreenUiUserCannotCloseWindow";

}  // namespace

namespace chromeos {

namespace login_screen_extension_ui {

class LoginScreenUiApitest : public LoginScreenApitestBase {
 public:
  LoginScreenUiApitest() : LoginScreenApitestBase(version_info::Channel::DEV) {}

  LoginScreenUiApitest(const LoginScreenUiApitest&) = delete;
  LoginScreenUiApitest& operator=(const LoginScreenUiApitest&) = delete;

  ~LoginScreenUiApitest() override = default;

  bool HasOpenWindow() const {
    return UiHandler::Get(false)->HasOpenWindow(extension_id_);
  }

  bool OnDialogCloseRequested() const {
    return UiHandler::Get(false)
        ->GetWindowForTesting(extension_id_)
        ->GetDialogDelegateForTesting()
        ->OnDialogCloseRequested();
  }

  bool ShouldShowCloseButton() const {
    return UiHandler::Get(false)
        ->GetWindowForTesting(extension_id_)
        ->GetDialogWidgetForTesting()
        ->widget_delegate()
        ->ShouldShowCloseButton();
  }

  bool IsMovementDisabled() const {
    return UiHandler::Get(false)
        ->GetWindowForTesting(extension_id_)
        ->GetDialogWidgetForTesting()
        ->movement_disabled();
  }
};

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest, ExtensionCanOpenWindow) {
  SetUpLoginScreenExtensionAndRunTest(kCanOpenWindow);
  ASSERT_TRUE(HasOpenWindow());
  // userCanClose defaults to false
  EXPECT_TRUE(IsMovementDisabled());
  EXPECT_FALSE(OnDialogCloseRequested());
  EXPECT_FALSE(ShouldShowCloseButton());
}

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest,
                       ExtensionCannotOpenMultipleWindows) {
  SetUpLoginScreenExtensionAndRunTest(kCannotOpenMultipleWindows);
  EXPECT_TRUE(HasOpenWindow());
}

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest, ExtensionCanOpenAndCloseWindow) {
  SetUpLoginScreenExtensionAndRunTest(kCanOpenAndCloseWindow);
  EXPECT_FALSE(HasOpenWindow());
}

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest, ExtensionCannotCloseNoWindow) {
  SetUpLoginScreenExtensionAndRunTest(kCannotCloseNoWindow);
  EXPECT_FALSE(HasOpenWindow());
}

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest, UserCanCloseWindow) {
  SetUpLoginScreenExtensionAndRunTest(kUserCanCloseWindow);
  ASSERT_TRUE(HasOpenWindow());
  EXPECT_TRUE(IsMovementDisabled());
  EXPECT_TRUE(OnDialogCloseRequested());
  EXPECT_TRUE(ShouldShowCloseButton());
}

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest, UserCannotCloseWindow) {
  SetUpLoginScreenExtensionAndRunTest(kUserCannotCloseWindow);
  ASSERT_TRUE(HasOpenWindow());
  EXPECT_TRUE(IsMovementDisabled());
  EXPECT_FALSE(OnDialogCloseRequested());
  EXPECT_FALSE(ShouldShowCloseButton());
}

}  // namespace login_screen_extension_ui

}  // namespace chromeos
