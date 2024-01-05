// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/auth_ui_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/check.h"
#include "chrome/browser/ash/login/test/composite_waiter.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enter_old_password_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_password_changed_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/factor_setup_success_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/local_data_loss_warning_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/osauth_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::test {

namespace {

constexpr UIPath kUserCreationConsumerOption = {"user-creation", "selfButton"};
constexpr UIPath kUserCreationNextButton = {"user-creation", "nextButton"};

constexpr UIPath kGaiaSigninPrimaryButton = {
    "gaia-signin", "signin-frame-dialog", "primary-action-button"};

constexpr UIPath kPasswordStep = {"gaia-password-changed", "passwordStep"};
constexpr UIPath kOldPasswordInput = {"gaia-password-changed",
                                      "oldPasswordInput"};
constexpr UIPath kSendPasswordButton = {"gaia-password-changed", "next"};
constexpr UIPath kForgotPasswordButton = {"gaia-password-changed",
                                          "forgotPasswordButton"};

constexpr UIPath kEnterOldPasswordInputStep = {"enter-old-password",
                                               "passwordStep"};
constexpr UIPath kEnterOldPasswordInput = {"enter-old-password",
                                           "oldPasswordInput"};
constexpr UIPath kEnterOldPasswordProceedButton = {"enter-old-password",
                                                   "next"};
constexpr UIPath kEnterOldPasswordForgotButton = {"enter-old-password",
                                                  "forgotPasswordButton"};

constexpr UIPath kForgotPasswordStep = {"gaia-password-changed",
                                        "forgotPassword"};
constexpr UIPath kForgotCancel = {"gaia-password-changed", "cancelForgot"};

constexpr UIPath kTryAgainRecovery = {"gaia-password-changed", "backButton"};
constexpr UIPath kProceedAnyway = {"gaia-password-changed", "proceedAnyway"};

constexpr UIPath kDataLossWarningElement = {"local-data-loss-warning"};
// TODO: why don't we have it?
constexpr UIPath kDataLossWarningCancel = {"local-data-loss-warning", "cancel"};

constexpr UIPath kDataLossWarningBack = {"local-data-loss-warning",
                                         "backButton"};
constexpr UIPath kDataLossWarningRemove = {"local-data-loss-warning",
                                           "proceedRemove"};
constexpr UIPath kDataLossWarningReset = {"local-data-loss-warning",
                                          "powerwash"};

constexpr UIPath kRecoverySuccessStep = {"cryptohome-recovery",
                                         "successDialog"};
constexpr UIPath kRecoveryDoneButton = {"cryptohome-recovery", "doneButton"};
constexpr UIPath kRecoveryErrorStep = {"cryptohome-recovery", "errorDialog"};
constexpr UIPath kRecoveryManualRecoveryButton = {"cryptohome-recovery",
                                                  "manualRecoveryButton"};

constexpr UIPath kRecoveryReauthNotificationStep = {"cryptohome-recovery",
                                                    "reauthNotificationDialog"};
constexpr UIPath kRecoveryReauthButton = {"cryptohome-recovery",
                                          "reauthButton"};

constexpr UIPath kFactorSetupSuccessElement = {"factor-setup-success"};
constexpr UIPath kFactorSetupSuccessDoneButton = {"factor-setup-success",
                                                  "doneButton"};
constexpr UIPath kFactorSetupSuccessNextButton = {"factor-setup-success",
                                                  "nextButton"};

const UIPath kFirstOnboardingScreen = {"consolidated-consent"};

bool IsOldFlow() {
  return base::FeatureList::IsEnabled(
      ash::features::kCryptohomeRecoveryBeforeFlowSplit);
}

}  // namespace

class LoginScreenAuthSurface : public FullScreenAuthSurface {
 public:
  LoginScreenAuthSurface() = default;
  ~LoginScreenAuthSurface() override = default;

  void SelectUserPod(const AccountId& account_id) override {
    EXPECT_TRUE(LoginScreenTestApi::FocusUser(account_id));
  }

  void AddNewUser() override {
    ASSERT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  }

  std::unique_ptr<LocalAuthenticationDialogActor>
  WaitForLocalAuthenticationDialog() override {
    LocalAuthenticationDialogWaiter()->Wait();
    return std::make_unique<LocalAuthenticationDialogActor>();
  }
};

class GaiaPageActorImpl : public GaiaPageActor {
 public:
  GaiaPageActorImpl() = default;
  ~GaiaPageActorImpl() override = default;

  void ReauthConfirmEmail(const AccountId& account_id) override {
    gaia_js_.ExpectElementValue(account_id.GetUserEmail(),
                                FakeGaiaMixin::kEmailPath);
    OobeJS().ClickOnPath(kGaiaSigninPrimaryButton);
  }

  void SubmitFullAuthEmail(const AccountId& account_id) override {
    gaia_js_.ExpectElementValue("", FakeGaiaMixin::kEmailPath);
    gaia_js_.TypeIntoPath(account_id.GetUserEmail(), FakeGaiaMixin::kEmailPath);
    OobeJS().ClickOnPath(kGaiaSigninPrimaryButton);
  }

  void TypePassword(const std::string& password) override {
    gaia_js_.TypeIntoPath(password, FakeGaiaMixin::kPasswordPath);
  }

  void ContinueLogin() override {
    OobeJS().ClickOnPath(kGaiaSigninPrimaryButton);
  }

  JSChecker gaia_js_;
};


FullScreenAuthSurface::FullScreenAuthSurface() = default;
FullScreenAuthSurface::~FullScreenAuthSurface() = default;

std::unique_ptr<FullScreenAuthSurface> OnLoginScreen() {
  return std::make_unique<LoginScreenAuthSurface>();
}

// ----------------------------------------------------------

OobePageActor::OobePageActor(std::optional<OobeScreenId> screen,
                             std::optional<ash::test::UIPath> path)
    : screen_(screen), path_(path) {}

OobePageActor::~OobePageActor() {}

std::unique_ptr<test::TestConditionWaiter> OobePageActor::UntilShown() {
  std::unique_ptr<test::TestConditionWaiter> result =
      std::make_unique<OobeWindowVisibilityWaiter>(true);
  if (screen_) {
    result = std::make_unique<CompositeWaiter>(
        std::move(result), std::make_unique<OobeScreenWaiter>(*screen_));
  }
  if (path_) {
    result = std::make_unique<CompositeWaiter>(
        std::move(result), OobeJS().CreateVisibilityWaiter(true, *path_));
  }
  return result;
}

// ----------------------------------------------------------

UserSelectionPageActor::UserSelectionPageActor()
    : OobePageActor(UserCreationView::kScreenId, std::nullopt) {}

UserSelectionPageActor::~UserSelectionPageActor() = default;

void UserSelectionPageActor::ChooseConsumerUser() {
  OobeJS().ClickOnPath(kUserCreationConsumerOption);
}

void UserSelectionPageActor::AwaitNextButton() {
  OobeJS().CreateEnabledWaiter(true, kUserCreationNextButton)->Wait();
}

void UserSelectionPageActor::Next() {
  OobeJS().ClickOnPath(kUserCreationNextButton);
}

std::unique_ptr<UserSelectionPageActor> AwaitNewUserSelectionUI() {
  std::unique_ptr<UserSelectionPageActor> result =
      std::make_unique<UserSelectionPageActor>();
  result->UntilShown()->Wait();

  return result;
}

// ----------------------------------------------------------

GaiaPageActor::GaiaPageActor()
    : OobePageActor(GaiaView::kScreenId, std::nullopt) {}
GaiaPageActor::~GaiaPageActor() = default;

std::unique_ptr<GaiaPageActor> AwaitGaiaSigninUI() {
  std::unique_ptr<GaiaPageActorImpl> result =
      std::make_unique<GaiaPageActorImpl>();
  result->UntilShown()->Wait();

  // Rely on primary button state to detect a moment when
  // embedded GAIA is fully loaded.
  OobeJS().CreateEnabledWaiter(true, kGaiaSigninPrimaryButton)->Wait();

  content::RenderFrameHost* frame = signin::GetAuthFrame(
      LoginDisplayHost::default_host()->GetOobeWebContents(), "signin-frame");
  CHECK(frame);
  result->gaia_js_ = test::JSChecker(frame);
  return result;
}

// ----------------------------------------------------------

RecoveryReauthPageActor::RecoveryReauthPageActor()
    : OobePageActor(CryptohomeRecoveryScreenView::kScreenId,
                    kRecoveryReauthNotificationStep) {}
RecoveryReauthPageActor::~RecoveryReauthPageActor() = default;

void RecoveryReauthPageActor::ConfirmReauth() {
  OobeJS().ClickOnPath(kRecoveryReauthButton);
}

std::unique_ptr<RecoveryReauthPageActor> AwaitRecoveryReauthUI() {
  std::unique_ptr<RecoveryReauthPageActor> result =
      std::make_unique<RecoveryReauthPageActor>();
  result->UntilShown()->Wait();
  return result;
}

// ----------------------------------------------------------

PasswordChangedPageActor::PasswordChangedPageActor()
    : OobePageActor(EnterOldPasswordScreenView::kScreenId,
                    kEnterOldPasswordInputStep) {}
PasswordChangedPageActor::~PasswordChangedPageActor() = default;

void PasswordChangedPageActor::TypePreviousPassword(
    const std::string& password) {
  PasswordChangedTypeOldPassword(password);
}

void PasswordChangedPageActor::SubmitPreviousPassword() {
  PasswordChangedSubmitOldPassword();
}

std::unique_ptr<test::TestConditionWaiter>
PasswordChangedPageActor::InvalidPasswordFeedback() {
  return PasswordChangedInvalidPasswordFeedback();
}

void PasswordChangedPageActor::ForgotPreviousPassword() {
  PasswordChangedForgotPasswordAction();
}

std::unique_ptr<PasswordChangedPageActor> AwaitPasswordChangedUI() {
  std::unique_ptr<PasswordChangedPageActor> result =
      std::make_unique<PasswordChangedPageActor>();
  result->UntilShown()->Wait();
  return result;
}

// ----------------------------------------------------------

PasswordUpdatedPageActor::PasswordUpdatedPageActor()
    : OobePageActor(FactorSetupSuccessScreenView::kScreenId,
                    kFactorSetupSuccessElement) {}
PasswordUpdatedPageActor::~PasswordUpdatedPageActor() = default;

void PasswordUpdatedPageActor::ExpectPasswordUpdateState() {
  PasswordUpdateNoticeExpectDone();
}

void PasswordUpdatedPageActor::ConfirmPasswordUpdate() {
  PasswordUpdateNoticeDoneAction();
}

std::unique_ptr<PasswordUpdatedPageActor> AwaitPasswordUpdatedUI() {
  std::unique_ptr<PasswordUpdatedPageActor> result =
      std::make_unique<PasswordUpdatedPageActor>();
  result->UntilShown()->Wait();
  return result;
}

// ----------------------------------------------------------

LocalAuthenticationDialogActor::LocalAuthenticationDialogActor() = default;
LocalAuthenticationDialogActor::~LocalAuthenticationDialogActor() = default;

bool LocalAuthenticationDialogActor::IsVisible() {
  return LoginScreenTestApi::IsLocalAuthenticationDialogVisible();
}

void LocalAuthenticationDialogActor::CancelDialog() {
  EXPECT_TRUE(IsVisible());
  LoginScreenTestApi::CancelLocalAuthenticationDialog();
}

void LocalAuthenticationDialogActor::SubmitPassword(
    const std::string& password) {
  EXPECT_TRUE(IsVisible());
  LoginScreenTestApi::SubmitPasswordLocalAuthenticationDialog(password);
}

// ----------------------------------------------------------

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

std::unique_ptr<test::TestConditionWaiter> UserOnboardingWaiter() {
  return std::make_unique<CompositeWaiter>(
      std::make_unique<OobeWindowVisibilityWaiter>(true),
      OobeJS().CreateVisibilityWaiter(true, kFirstOnboardingScreen));
}

std::unique_ptr<test::TestConditionWaiter> LocalAuthenticationDialogWaiter() {
  return std::make_unique<test::TestPredicateWaiter>(base::BindRepeating([]() {
    return LoginScreenTestApi::IsLocalAuthenticationDialogVisible();
  }));
}

}  // namespace ash::test
