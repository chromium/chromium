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
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enter_old_password_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/local_password_setup_handler.h"
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

constexpr UIPath kEnterOldPasswordInputStep = {"enter-old-password",
                                               "passwordStep"};
constexpr UIPath kEnterOldPasswordInput = {"enter-old-password",
                                           "oldPasswordInput"};
constexpr UIPath kEnterOldPasswordProceedButton = {"enter-old-password",
                                                   "next"};
constexpr UIPath kEnterOldPasswordForgotButton = {"enter-old-password",
                                                  "forgotPasswordButton"};

constexpr UIPath kDataLossWarningElement = {"local-data-loss-warning"};
// TODO: why don't we have it?
constexpr UIPath kDataLossWarningCancel = {"local-data-loss-warning", "cancel"};

constexpr UIPath kDataLossWarningBack = {"local-data-loss-warning",
                                         "backButton"};
constexpr UIPath kDataLossWarningRemove = {"local-data-loss-warning",
                                           "proceedRemove"};
constexpr UIPath kDataLossWarningReset = {"local-data-loss-warning",
                                          "powerwash"};

constexpr UIPath kRecoveryReauthNotificationStep = {"cryptohome-recovery",
                                                    "reauthNotificationDialog"};
constexpr UIPath kRecoveryReauthButton = {"cryptohome-recovery",
                                          "reauthButton"};

constexpr UIPath kFactorSetupSuccessElement = {"factor-setup-success"};
constexpr UIPath kFactorSetupSuccessDoneButton = {"factor-setup-success",
                                                  "doneButton"};
constexpr UIPath kFactorSetupSuccessNextButton = {"factor-setup-success",
                                                  "nextButton"};

constexpr UIPath kLocalPasswordSetupElement = {"local-password-setup"};
constexpr UIPath kLocalPasswordSetupFirstInput = {
    "local-password-setup", "passwordInput", "firstInput"};
constexpr UIPath kLocalPasswordSetupConfirmInput = {
    "local-password-setup", "passwordInput", "confirmInput"};
constexpr UIPath kLocalPasswordSetupBackButton = {"local-password-setup",
                                                  "backButton"};
constexpr UIPath kLocalPasswordSetupNextButton = {"local-password-setup",
                                                  "nextButton"};

const UIPath kFirstOnboardingScreen = {"consolidated-consent"};

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

  void SubmitPassword(const AccountId& account_id,
                      const std::string& password,
                      bool check_if_submittable);

  void SubmitPassword(const AccountId& account_id,
                      const std::string& password) override {
    LoginScreenTestApi::SubmitPassword(account_id, password, true);
  }

  std::unique_ptr<LocalAuthenticationDialogActor>
  WaitForLocalAuthenticationDialog() override {
    LocalAuthenticationDialogWaiter()->Wait();
    return std::make_unique<LocalAuthenticationDialogActor>();
  }

  std::unique_ptr<AuthErrorBubbleActor> WaitForAuthErrorBubble() override {
    AuthErrorBubbleWaiter()->Wait();
    return std::make_unique<AuthErrorBubbleActor>();
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

LocalPasswordSetupPageActor::LocalPasswordSetupPageActor()
    : OobePageActor(LocalPasswordSetupView::kScreenId,
                    kLocalPasswordSetupElement) {}
LocalPasswordSetupPageActor::~LocalPasswordSetupPageActor() = default;

void LocalPasswordSetupPageActor::TypeFirstPassword(
    const std::string& password) {
  LocalPasswordSetupExpectFirstInput();
  LocalPasswordSetupTypeFirstPassword(password);
}

void LocalPasswordSetupPageActor::TypeConfirmPassword(
    const std::string& password) {
  LocalPasswordSetupExpectConfirmInput();
  LocalPasswordSetupTypeConfirmPassword(password);
}

void LocalPasswordSetupPageActor::GoBack() {
  LocalPasswordSetupExpectBackButton();
  LocalPasswordSetupBackAction();
}

void LocalPasswordSetupPageActor::Submit() {
  LocalPasswordSetupExpectNextButton();
  LocalPasswordSetupNextAction();
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

void LocalAuthenticationDialogActor::WaitUntilDismissed() {
  LocalAuthenticationDialogDismissWaiter()->Wait();
}

// ----------------------------------------------------------

AuthErrorBubbleActor::AuthErrorBubbleActor() = default;
AuthErrorBubbleActor::~AuthErrorBubbleActor() = default;

bool AuthErrorBubbleActor::IsVisible() {
  return LoginScreenTestApi::IsAuthErrorBubbleShown();
}

void AuthErrorBubbleActor::Hide() {
  LoginScreenTestApi::HideAuthError();
}

void AuthErrorBubbleActor::PressRecoveryButton() {
  LoginScreenTestApi::PressAuthErrorRecoveryButton();
}

void AuthErrorBubbleActor::PressLearnMoreButton() {
  LoginScreenTestApi::PressAuthErrorLearnMoreButton();
}

// ----------------------------------------------------------

std::unique_ptr<test::TestConditionWaiter> CreateOldPasswordEnterPageWaiter() {
  return std::make_unique<CompositeWaiter>(
      std::make_unique<OobeWindowVisibilityWaiter>(true),
      std::make_unique<OobeScreenWaiter>(
          ash::EnterOldPasswordScreenView::kScreenId),
      OobeJS().CreateVisibilityWaiter(true, kEnterOldPasswordInputStep));
}

void PasswordChangedTypeOldPassword(const std::string& text) {
  test::OobeJS().TypeIntoPath(text, kEnterOldPasswordInput);
}

void PasswordChangedSubmitOldPassword() {
  test::OobeJS().ClickOnPath(kEnterOldPasswordProceedButton);
}

std::unique_ptr<test::TestConditionWaiter>
PasswordChangedInvalidPasswordFeedback() {
  return test::OobeJS().CreateWaiter(
      test::GetOobeElementPath(kEnterOldPasswordInput) + ".invalid");
}

void PasswordChangedForgotPasswordAction() {
  test::OobeJS().ClickOnPath(kEnterOldPasswordForgotButton);
}

std::unique_ptr<test::TestConditionWaiter> LocalDataLossWarningPageWaiter() {
  return std::make_unique<CompositeWaiter>(
      std::make_unique<OobeWindowVisibilityWaiter>(true),
      std::make_unique<OobeScreenWaiter>(
          LocalDataLossWarningScreenView::kScreenId),
      OobeJS().CreateVisibilityWaiter(true, kDataLossWarningElement));
}

void LocalDataLossWarningPageCancelAction() {
  test::OobeJS().ClickOnPath(kDataLossWarningCancel);
}

void LocalDataLossWarningPageGoBackAction() {
  test::OobeJS().ClickOnPath(kDataLossWarningBack);
}

void LocalDataLossWarningPageRemoveAction() {
  test::OobeJS().ClickOnPath(kDataLossWarningRemove);
}

void LocalDataLossWarningPageResetAction() {
  test::OobeJS().ClickOnPath(kDataLossWarningReset);
}

void LocalDataLossWarningPageExpectGoBack() {
  test::OobeJS().ExpectVisiblePath(kDataLossWarningBack);
}

void LocalDataLossWarningPageExpectRemove() {
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

void LocalPasswordSetupExpectNextButton() {
  test::OobeJS().ExpectVisiblePath(kLocalPasswordSetupNextButton);
}

void LocalPasswordSetupNextAction() {
  test::OobeJS().ClickOnPath(kLocalPasswordSetupNextButton);
}

void LocalPasswordSetupExpectBackButton() {
  test::OobeJS().ExpectVisiblePath(kLocalPasswordSetupBackButton);
}

void LocalPasswordSetupBackAction() {
  test::OobeJS().ClickOnPath(kLocalPasswordSetupBackButton);
}

void LocalPasswordSetupExpectFirstInput() {
  test::OobeJS().ExpectVisiblePath(kLocalPasswordSetupFirstInput);
}

void LocalPasswordSetupTypeFirstPassword(const std::string& pw) {
  test::OobeJS().TypeIntoPath(pw, kLocalPasswordSetupFirstInput);
}

void LocalPasswordSetupExpectConfirmInput() {
  test::OobeJS().ExpectVisiblePath(kLocalPasswordSetupConfirmInput);
}

void LocalPasswordSetupTypeConfirmPassword(const std::string& pw) {
  test::OobeJS().TypeIntoPath(pw, kLocalPasswordSetupConfirmInput);
}

std::unique_ptr<test::TestConditionWaiter> RecoveryPasswordUpdatedPageWaiter() {
  return CreatePasswordUpdateNoticePageWaiter();
}

std::unique_ptr<LocalPasswordSetupPageActor> AwaitLocalPasswordSetupUI() {
  std::unique_ptr<LocalPasswordSetupPageActor> result =
      std::make_unique<LocalPasswordSetupPageActor>();
  result->UntilShown()->Wait();
  return result;
}

void RecoveryPasswordUpdatedProceedAction() {
  // TODO(b/315829727): inline this and other now-one-line-methods.
  PasswordUpdateNoticeDoneAction();
}

std::unique_ptr<test::TestConditionWaiter> RecoveryErrorPageWaiter() {
  return std::make_unique<CompositeWaiter>(
      std::make_unique<OobeWindowVisibilityWaiter>(true),
      std::make_unique<OobeScreenWaiter>(
          ash::OSAuthErrorScreenView::kScreenId));
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

std::unique_ptr<test::TestConditionWaiter>
LocalAuthenticationDialogDismissWaiter() {
  return std::make_unique<test::TestPredicateWaiter>(base::BindRepeating([]() {
    return !LoginScreenTestApi::IsLocalAuthenticationDialogVisible();
  }));
}

std::unique_ptr<test::TestConditionWaiter> AuthErrorBubbleWaiter() {
  return std::make_unique<test::TestPredicateWaiter>(base::BindRepeating(
      []() { return LoginScreenTestApi::IsAuthErrorBubbleShown(); }));
}

std::unique_ptr<test::TestConditionWaiter> AuthErrorBubbleDismissWaiter() {
  return std::make_unique<test::TestPredicateWaiter>(base::BindRepeating(
      []() { return !LoginScreenTestApi::IsAuthErrorBubbleShown(); }));
}

}  // namespace ash::test
