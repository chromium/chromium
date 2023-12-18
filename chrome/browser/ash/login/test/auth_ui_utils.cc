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
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enter_old_password_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_password_changed_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/factor_setup_success_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/local_data_loss_warning_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/osauth_error_screen_handler.h"

namespace ash::test {

namespace {
const UIPath kPasswordStep = {"gaia-password-changed", "passwordStep"};
const UIPath kOldPasswordInput = {"gaia-password-changed", "oldPasswordInput"};
const UIPath kSendPasswordButton = {"gaia-password-changed", "next"};
const UIPath kForgotPasswordButton = {"gaia-password-changed",
                                      "forgotPasswordButton"};

const UIPath kEnterOldPasswordInputStep = {"enter-old-password",
                                           "passwordStep"};
const UIPath kEnterOldPasswordInput = {"enter-old-password",
                                       "oldPasswordInput"};
const UIPath kEnterOldPasswordProceedButton = {"enter-old-password", "next"};
const UIPath kEnterOldPasswordForgotButton = {"enter-old-password",
                                              "forgotPasswordButton"};

const UIPath kForgotPasswordStep = {"gaia-password-changed", "forgotPassword"};
const UIPath kForgotCancel = {"gaia-password-changed", "cancelForgot"};

const UIPath kTryAgainRecovery = {"gaia-password-changed", "backButton"};
const UIPath kProceedAnyway = {"gaia-password-changed", "proceedAnyway"};

const UIPath kDataLossWarningElement = {"local-data-loss-warning"};
// TODO: why don't we have it?
const UIPath kDataLossWarningCancel = {"local-data-loss-warning", "cancel"};

const UIPath kDataLossWarningBack = {"local-data-loss-warning", "backButton"};
const UIPath kDataLossWarningRemove = {"local-data-loss-warning",
                                       "proceedRemove"};
const UIPath kDataLossWarningReset = {"local-data-loss-warning", "powerwash"};

const test::UIPath kRecoverySuccessStep = {"cryptohome-recovery",
                                           "successDialog"};
const test::UIPath kRecoveryDoneButton = {"cryptohome-recovery", "doneButton"};
const test::UIPath kRecoveryErrorStep = {"cryptohome-recovery", "errorDialog"};
const test::UIPath kRecoveryManualRecoveryButton = {"cryptohome-recovery",
                                                    "manualRecoveryButton"};

const UIPath kFactorSetupSuccessElement = {"factor-setup-success"};
const UIPath kFactorSetupSuccessDoneButton = {"factor-setup-success",
                                              "doneButton"};
const UIPath kFactorSetupSuccessNextButton = {"factor-setup-success",
                                              "nextButton"};

bool IsOldFlow() {
  return base::FeatureList::IsEnabled(
      ash::features::kCryptohomeRecoveryBeforeFlowSplit);
}

}  // namespace

// Password change scenario

std::unique_ptr<test::TestConditionWaiter> CreateOldPasswordEnterPageWaiter() {
  if (IsOldFlow()) {
    return std::make_unique<CompositeWaiter>(
        std::make_unique<OobeWindowVisibilityWaiter>(true),
        std::make_unique<OobeScreenWaiter>(GaiaPasswordChangedView::kScreenId),
        OobeJS().CreateVisibilityWaiter(true, kPasswordStep));
  }
  return std::make_unique<CompositeWaiter>(
      std::make_unique<OobeWindowVisibilityWaiter>(true),
      std::make_unique<OobeScreenWaiter>(
          ash::EnterOldPasswordScreenView::kScreenId),
      OobeJS().CreateVisibilityWaiter(true, kEnterOldPasswordInputStep));
}

void PasswordChangedTypeOldPassword(const std::string& text) {
  if (IsOldFlow()) {
    test::OobeJS().TypeIntoPath(text, kOldPasswordInput);
    return;
  }
  test::OobeJS().TypeIntoPath(text, kEnterOldPasswordInput);
}

void PasswordChangedSubmitOldPassword() {
  if (IsOldFlow()) {
    test::OobeJS().ClickOnPath(kSendPasswordButton);
    return;
  }
  test::OobeJS().ClickOnPath(kEnterOldPasswordProceedButton);
}

std::unique_ptr<test::TestConditionWaiter>
PasswordChangedInvalidPasswordFeedback() {
  if (IsOldFlow()) {
    return test::OobeJS().CreateWaiter(
        test::GetOobeElementPath(kOldPasswordInput) + ".invalid");
  }
  return test::OobeJS().CreateWaiter(
      test::GetOobeElementPath(kEnterOldPasswordInput) + ".invalid");
}

void PasswordChangedForgotPasswordAction() {
  if (IsOldFlow()) {
    test::OobeJS().ClickOnPath(kForgotPasswordButton);
    return;
  }
  test::OobeJS().ClickOnPath(kEnterOldPasswordForgotButton);
}

std::unique_ptr<test::TestConditionWaiter> LocalDataLossWarningPageWaiter() {
  if (IsOldFlow()) {
    return std::make_unique<CompositeWaiter>(
        std::make_unique<OobeWindowVisibilityWaiter>(true),
        std::make_unique<OobeScreenWaiter>(GaiaPasswordChangedView::kScreenId),
        OobeJS().CreateVisibilityWaiter(true, kForgotPasswordStep));
  }
  return std::make_unique<CompositeWaiter>(
      std::make_unique<OobeWindowVisibilityWaiter>(true),
      std::make_unique<OobeScreenWaiter>(
          LocalDataLossWarningScreenView::kScreenId),
      OobeJS().CreateVisibilityWaiter(true, kDataLossWarningElement));
}

void LocalDataLossWarningPageCancelAction() {
  if (IsOldFlow()) {
    test::OobeJS().ClickOnPath(kForgotCancel);
    return;
  }
  test::OobeJS().ClickOnPath(kDataLossWarningCancel);
}

void LocalDataLossWarningPageGoBackAction() {
  if (IsOldFlow()) {
    test::OobeJS().ClickOnPath(kTryAgainRecovery);
    return;
  }
  test::OobeJS().ClickOnPath(kDataLossWarningBack);
}

void LocalDataLossWarningPageRemoveAction() {
  if (IsOldFlow()) {
    test::OobeJS().ClickOnPath(kProceedAnyway);
    return;
  }
  test::OobeJS().ClickOnPath(kDataLossWarningRemove);
}

void LocalDataLossWarningPageResetAction() {
  test::OobeJS().ClickOnPath(kDataLossWarningReset);
}

void LocalDataLossWarningPageExpectGoBack() {
  if (IsOldFlow()) {
    test::OobeJS().ExpectVisiblePath(kTryAgainRecovery);
    return;
  }
  test::OobeJS().ExpectVisiblePath(kDataLossWarningBack);
}

void LocalDataLossWarningPageExpectRemove() {
  if (IsOldFlow()) {
    test::OobeJS().ExpectVisiblePath(kProceedAnyway);
    return;
  }
  test::OobeJS().ExpectVisiblePath(kDataLossWarningRemove);
}

void LocalDataLossWarningPageExpectReset() {
  test::OobeJS().ExpectVisiblePath(kDataLossWarningReset);
}

std::unique_ptr<test::TestConditionWaiter>
CreatePasswordUpdateNoticePageWaiter() {
  return std::make_unique<CompositeWaiter>(
      std::make_unique<OobeWindowVisibilityWaiter>(true),
      std::make_unique<OobeScreenWaiter>(
          ash::FactorSetupSuccessScreenView::kScreenId),
      OobeJS().CreateVisibilityWaiter(true, kFactorSetupSuccessElement));
}

void PasswordUpdateNoticeExpectNext() {
  test::OobeJS().ExpectVisiblePath(kFactorSetupSuccessNextButton);
}

void PasswordUpdateNoticeNextAction() {
  test::OobeJS().ClickOnPath(kFactorSetupSuccessNextButton);
}

void PasswordUpdateNoticeExpectDone() {
  test::OobeJS().ExpectVisiblePath(kFactorSetupSuccessDoneButton);
}

void PasswordUpdateNoticeDoneAction() {
  test::OobeJS().ClickOnPath(kFactorSetupSuccessDoneButton);
}

std::unique_ptr<test::TestConditionWaiter> RecoveryPasswordUpdatedPageWaiter() {
  if (IsOldFlow()) {
    return std::make_unique<CompositeWaiter>(
        std::make_unique<OobeWindowVisibilityWaiter>(true),
        std::make_unique<OobeScreenWaiter>(
            CryptohomeRecoveryScreenView::kScreenId),
        OobeJS().CreateVisibilityWaiter(true, kRecoverySuccessStep));
  }
  return CreatePasswordUpdateNoticePageWaiter();
}

void RecoveryPasswordUpdatedProceedAction() {
  if (IsOldFlow()) {
    test::OobeJS().ClickOnPath(kRecoveryDoneButton);
    return;
  }
  PasswordUpdateNoticeDoneAction();
}

std::unique_ptr<test::TestConditionWaiter> RecoveryErrorPageWaiter() {
  if (IsOldFlow()) {
    return std::make_unique<CompositeWaiter>(
        std::make_unique<OobeWindowVisibilityWaiter>(true),
        std::make_unique<OobeScreenWaiter>(
            CryptohomeRecoveryScreenView::kScreenId),
        OobeJS().CreateVisibilityWaiter(true, kRecoveryErrorStep));
  }
  return std::make_unique<CompositeWaiter>(
      std::make_unique<OobeWindowVisibilityWaiter>(true),
      std::make_unique<OobeScreenWaiter>(
          ash::OSAuthErrorScreenView::kScreenId));
}

void RecoveryErrorExpectFallback() {
  CHECK(IsOldFlow());
  test::OobeJS().ExpectVisiblePath(kRecoveryManualRecoveryButton);
  return;
}

void RecoveryErrorFallbackAction() {
  CHECK(IsOldFlow());
  test::OobeJS().ClickOnPath(kRecoveryManualRecoveryButton);
  return;
}

}  // namespace ash::test
