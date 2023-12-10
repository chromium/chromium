// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/auth_ui_utils.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/test/composite_waiter.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ui/webui/ash/login/gaia_password_changed_screen_handler.h"

namespace ash::test {

namespace {
const UIPath kPasswordStep = {"gaia-password-changed", "passwordStep"};
const UIPath kOldPasswordInput = {"gaia-password-changed", "oldPasswordInput"};
const UIPath kSendPasswordButton = {"gaia-password-changed", "next"};
const UIPath kForgotPasswordButton = {"gaia-password-changed",
                                      "forgotPasswordButton"};

const UIPath kForgotPasswordStep = {"gaia-password-changed", "forgotPassword"};
const UIPath kForgotCancel = {"gaia-password-changed", "cancelForgot"};

const UIPath kTryAgainRecovery = {"gaia-password-changed", "backButton"};
const UIPath kProceedAnyway = {"gaia-password-changed", "proceedAnyway"};
}  // namespace

// Password change scenario

std::unique_ptr<test::TestConditionWaiter> CreateOldPasswordEnterPageWaiter() {
  return std::make_unique<CompositeWaiter>(
      std::make_unique<OobeWindowVisibilityWaiter>(true),
      std::make_unique<OobeScreenWaiter>(GaiaPasswordChangedView::kScreenId),
      OobeJS().CreateVisibilityWaiter(true, kPasswordStep));
}

void PasswordChangedTypeOldPassword(const std::string& text) {
  test::OobeJS().TypeIntoPath(text, kOldPasswordInput);
}

void PasswordChangedSubmitOldPassword() {
  test::OobeJS().ClickOnPath(kSendPasswordButton);
}

std::unique_ptr<test::TestConditionWaiter>
PasswordChangedInvalidPasswordFeedback() {
  return test::OobeJS().CreateWaiter(
      test::GetOobeElementPath(kOldPasswordInput) + ".invalid");
}

void PasswordChangedForgotPasswordAction() {
  test::OobeJS().ClickOnPath(kForgotPasswordButton);
}

std::unique_ptr<test::TestConditionWaiter> LocalDataLossWarningPageWaiter() {
  return std::make_unique<CompositeWaiter>(
      std::make_unique<OobeWindowVisibilityWaiter>(true),
      std::make_unique<OobeScreenWaiter>(GaiaPasswordChangedView::kScreenId),
      OobeJS().CreateVisibilityWaiter(true, kForgotPasswordStep));
}

void LocalDataLossWarningPageCancelAction() {
  test::OobeJS().ClickOnPath(kForgotCancel);
}

void LocalDataLossWarningPageGoBackAction() {
  test::OobeJS().ClickOnPath(kTryAgainRecovery);
}

void LocalDataLossWarningPageProceedAction() {
  test::OobeJS().ClickOnPath(kProceedAnyway);
}

void LocalDataLossWarningPageExpectGoBack() {
  test::OobeJS().ExpectVisiblePath(kTryAgainRecovery);
}

void LocalDataLossWarningPageExpectProceed() {
  test::OobeJS().ExpectVisiblePath(kProceedAnyway);
}

}  // namespace ash::test
