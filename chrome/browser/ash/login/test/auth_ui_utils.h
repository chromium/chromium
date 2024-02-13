// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_AUTH_UI_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_AUTH_UI_UTILS_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"

class AccountId;

namespace ash::test {

class LocalAuthenticationDialogActor;
class AuthErrorBubbleActor;

class FullScreenAuthSurface {
 public:
  FullScreenAuthSurface();
  virtual ~FullScreenAuthSurface();

  virtual void SelectUserPod(const AccountId& account_id) = 0;
  virtual void AddNewUser() = 0;
  virtual void SubmitPassword(const AccountId& account_id,
                              const std::string& password) = 0;

  virtual std::unique_ptr<LocalAuthenticationDialogActor>
  WaitForLocalAuthenticationDialog() = 0;

  virtual std::unique_ptr<AuthErrorBubbleActor> WaitForAuthErrorBubble() = 0;
};

class OobePageActor {
 public:
  OobePageActor(std::optional<OobeScreenId> screen,
                std::optional<ash::test::UIPath> path);
  virtual ~OobePageActor();

  std::unique_ptr<test::TestConditionWaiter> UntilShown();

 protected:
  std::optional<OobeScreenId> screen_;
  std::optional<ash::test::UIPath> path_;
};

class UserSelectionPageActor : public OobePageActor {
 public:
  UserSelectionPageActor();
  ~UserSelectionPageActor() override;

  void ChooseConsumerUser();
  void AwaitNextButton();
  void Next();
};

class GaiaPageActor : public OobePageActor {
 public:
  GaiaPageActor();
  ~GaiaPageActor() override;

  virtual void ReauthConfirmEmail(const AccountId& account_id) = 0;
  virtual void SubmitFullAuthEmail(const AccountId& account_id) = 0;
  virtual void TypePassword(const std::string& password) = 0;
  virtual void ContinueLogin() = 0;
};

class RecoveryReauthPageActor : public OobePageActor {
 public:
  RecoveryReauthPageActor();
  ~RecoveryReauthPageActor() override;

  void ConfirmReauth();
};

class PasswordChangedPageActor : public OobePageActor {
 public:
  PasswordChangedPageActor();
  ~PasswordChangedPageActor() override;

  void TypePreviousPassword(const std::string& password);
  void SubmitPreviousPassword();
  [[nodiscard]] std::unique_ptr<test::TestConditionWaiter>
  InvalidPasswordFeedback();
  void ForgotPreviousPassword();
};

class PasswordUpdatedPageActor : public OobePageActor {
 public:
  PasswordUpdatedPageActor();
  ~PasswordUpdatedPageActor() override;

  void ExpectPasswordUpdateState();
  void ConfirmPasswordUpdate();
};

class LocalPasswordSetupPageActor : public OobePageActor {
 public:
  LocalPasswordSetupPageActor();
  ~LocalPasswordSetupPageActor() override;

  void TypeFirstPassword(const std::string& password);
  void TypeConfirmPassword(const std::string& password);
  void GoBack();
  void Submit();
};

class LocalAuthenticationDialogActor {
 public:
  LocalAuthenticationDialogActor();
  ~LocalAuthenticationDialogActor();

  bool IsVisible();
  void CancelDialog();
  void SubmitPassword(const std::string& password);
  void WaitUntilDismissed();
};

class AuthErrorBubbleActor {
 public:
  AuthErrorBubbleActor();
  ~AuthErrorBubbleActor();

  bool IsVisible();
  void Hide();
  void PressRecoveryButton();
  void PressLearnMoreButton();
};

std::unique_ptr<FullScreenAuthSurface> OnLoginScreen();

[[nodiscard]] std::unique_ptr<GaiaPageActor> AwaitGaiaSigninUI();
[[nodiscard]] std::unique_ptr<RecoveryReauthPageActor> AwaitRecoveryReauthUI();
[[nodiscard]] std::unique_ptr<PasswordChangedPageActor>
AwaitPasswordChangedUI();
[[nodiscard]] std::unique_ptr<PasswordUpdatedPageActor>
AwaitPasswordUpdatedUI();
[[nodiscard]] std::unique_ptr<UserSelectionPageActor> AwaitNewUserSelectionUI();
[[nodiscard]] std::unique_ptr<LocalPasswordSetupPageActor>
AwaitLocalPasswordSetupUI();

// Password change scenario
// page for entering old password
std::unique_ptr<test::TestConditionWaiter> CreateOldPasswordEnterPageWaiter();
void PasswordChangedTypeOldPassword(const std::string& text);
void PasswordChangedSubmitOldPassword();
std::unique_ptr<test::TestConditionWaiter>
PasswordChangedInvalidPasswordFeedback();
void PasswordChangedForgotPasswordAction();

// page for cryptohome re-creation suggestion
std::unique_ptr<test::TestConditionWaiter> LocalDataLossWarningPageWaiter();
void LocalDataLossWarningPageCancelAction();
void LocalDataLossWarningPageGoBackAction();
void LocalDataLossWarningPageRemoveAction();
void LocalDataLossWarningPageResetAction();

void LocalDataLossWarningPageExpectGoBack();
void LocalDataLossWarningPageExpectRemove();
void LocalDataLossWarningPageExpectReset();

std::unique_ptr<test::TestConditionWaiter>
CreatePasswordUpdateNoticePageWaiter();
void PasswordUpdateNoticeExpectNext();
void PasswordUpdateNoticeNextAction();
void PasswordUpdateNoticeExpectDone();
void PasswordUpdateNoticeDoneAction();

void LocalPasswordSetupExpectNextButton();
void LocalPasswordSetupNextAction();
void LocalPasswordSetupExpectBackButton();
void LocalPasswordSetupBackAction();
void LocalPasswordSetupExpectFirstInput();
void LocalPasswordSetupTypeFirstPassword(const std::string& pw);
void LocalPasswordSetupExpectConfirmInput();
void LocalPasswordSetupTypeConfirmPassword(const std::string& pw);

std::unique_ptr<test::TestConditionWaiter> RecoveryPasswordUpdatedPageWaiter();
void RecoveryPasswordUpdatedProceedAction();

std::unique_ptr<test::TestConditionWaiter> RecoveryErrorPageWaiter();
void RecoveryErrorExpectFallback();
void RecoveryErrorFallbackAction();

std::unique_ptr<test::TestConditionWaiter> UserOnboardingWaiter();

std::unique_ptr<test::TestConditionWaiter> LocalAuthenticationDialogWaiter();
std::unique_ptr<test::TestConditionWaiter>
LocalAuthenticationDialogDismissWaiter();

std::unique_ptr<test::TestConditionWaiter> AuthErrorBubbleWaiter();
std::unique_ptr<test::TestConditionWaiter> AuthErrorBubbleDismissWaiter();

}  // namespace ash::test

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_AUTH_UI_UTILS_H_
