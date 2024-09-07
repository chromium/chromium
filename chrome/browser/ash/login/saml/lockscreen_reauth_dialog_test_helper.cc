// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/lockscreen_reauth_dialog_test_helper.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_captive_portal_dialog.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_dialog.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_ui.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_start_reauth_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Main dialog
const test::UIPath kWebviewContainer = {"main-element", "body"};
const test::UIPath kErrorCancelButton = {"main-element",
                                         "cancelButtonErrorScreen"};
const test::UIPath kSamlCancelButton = {"main-element", "saml-close-button"};
const test::UIPath kSamlNoticeMessage = {"main-element", "samlNoticeMessage"};
const test::UIPath kChangeIdPButton = {"main-element", "change-account"};
const test::UIPath kChangeIdPButtonContainer = {"main-element",
                                                "saml-footer-container"};
const test::UIPath kGaiaButtons = {"main-element", "buttons-container"};
const test::UIPath kGaiaPrimaryButton = {"main-element", "gaia-buttons",
                                         "primary-button"};
const test::UIPath kErrorScreen = {"main-element", "errorScreen"};
const test::UIPath kSamlConfirmPasswordScreen = {"main-element",
                                                 "samlConfirmPasswordScreen"};
const test::UIPath kPasswordInput = {"main-element", "passwordInput"};
const test::UIPath kPasswordConfirmInput = {"main-element",
                                            "confirmPasswordInput"};
const test::UIPath kPasswordSubmit = {"main-element",
                                      "nextButtonSamlConfirmPassword"};
const char kSigninFrame[] = "signin-frame";

// Network dialog
const test::UIPath kNetworkDialog = {"network-ui", "dialog"};
const test::UIPath kNetworkCancelButton = {"network-ui", "cancelButton"};

}  // namespace

LockScreenReauthDialogTestHelper::LockScreenReauthDialogTestHelper() = default;
LockScreenReauthDialogTestHelper::~LockScreenReauthDialogTestHelper() = default;

LockScreenReauthDialogTestHelper::LockScreenReauthDialogTestHelper(
    LockScreenReauthDialogTestHelper&& other) = default;

LockScreenReauthDialogTestHelper& LockScreenReauthDialogTestHelper::operator=(
    LockScreenReauthDialogTestHelper&& other) = default;

// static
std::optional<LockScreenReauthDialogTestHelper>
LockScreenReauthDialogTestHelper::ShowDialogAndWait() {
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    ADD_FAILURE() << "Screen must be locked";
    return std::nullopt;
  }

  LockScreenStartReauthDialog::Show();

  return InitForShownDialog();
}

// static
std::optional<LockScreenReauthDialogTestHelper>
LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad() {
  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      ShowDialogAndWait();
  if (!reauth_dialog_helper.has_value()) {
    return std::nullopt;
  }

  reauth_dialog_helper->WaitForSigninWebview();

  // With reauth endpoint we start on a Gaia page where user needs to click
  // "Next" before being redirected to SAML IdP page.
  reauth_dialog_helper->WaitForPrimaryGaiaButtonToBeEnabled();
  reauth_dialog_helper->ClickPrimaryGaiaButton();

  reauth_dialog_helper->WaitForSamlIdpPageLoad();
  reauth_dialog_helper->ExpectGaiaButtonsHidden();

  return reauth_dialog_helper;
}

// static
std::optional<LockScreenReauthDialogTestHelper>
LockScreenReauthDialogTestHelper::InitForShownDialog() {
  LockScreenReauthDialogTestHelper dialog_test_helper;
  // Fetch the dialog, WebUi controller and main message handler.
  dialog_test_helper.reauth_dialog_ =
      LockScreenStartReauthDialog::GetInstance();
  CHECK(dialog_test_helper.reauth_dialog_);
  dialog_test_helper.WaitForReauthDialogToLoad();
  if (!dialog_test_helper.reauth_dialog_->GetWebUIForTest()) {
    ADD_FAILURE()
        << "Could not retrieve WebUI from LockScreenStartReauthDialog";
    return std::nullopt;
  }
  LockScreenStartReauthUI* reauth_webui_controller =
      static_cast<LockScreenStartReauthUI*>(
          dialog_test_helper.reauth_dialog_->GetWebUIForTest()
              ->GetController());
  if (!reauth_webui_controller) {
    ADD_FAILURE() << "Could not retrieve LockScreenStartReauthUI";
    return std::nullopt;
  }
  dialog_test_helper.main_handler_ = reauth_webui_controller->GetMainHandler();
  if (!dialog_test_helper.main_handler_) {
    ADD_FAILURE() << "Could not retrieve LockScreenReauthHandler";
    return std::nullopt;
  }
  return dialog_test_helper;
}

void LockScreenReauthDialogTestHelper::ClickCancelButtonOnErrorScreen() {
  ExpectErrorScreenVisible();
  DialogJS().TapOnPath(kErrorCancelButton);
}

void LockScreenReauthDialogTestHelper::ClickCancelButtonOnSamlScreen() {
  ExpectSigninWebviewVisible();
  DialogJS().TapOnPath(kSamlCancelButton);
}

void LockScreenReauthDialogTestHelper::ClickChangeIdPButtonOnSamlScreen() {
  ExpectSigninWebviewVisible();
  DialogJS().TapOnPath(kChangeIdPButton);
}

void LockScreenReauthDialogTestHelper::ClickPrimaryGaiaButton() {
  DialogJS().TapOnPath(kGaiaPrimaryButton);
}

void LockScreenReauthDialogTestHelper::WaitForPrimaryGaiaButtonToBeEnabled() {
  DialogJS().CreateEnabledWaiter(/*enabled=*/true, kGaiaPrimaryButton)->Wait();
}

void LockScreenReauthDialogTestHelper::ExpectGaiaButtonsVisible() {
  ExpectSigninWebviewVisible();
  DialogJS().ExpectVisiblePath(kGaiaButtons);
}

void LockScreenReauthDialogTestHelper::ExpectGaiaButtonsHidden() {
  DialogJS().ExpectHiddenPath(kGaiaButtons);
}

void LockScreenReauthDialogTestHelper::ExpectChangeIdPButtonVisible() {
  ExpectSigninWebviewVisible();
  DialogJS().ExpectVisiblePath(kChangeIdPButtonContainer);
}

void LockScreenReauthDialogTestHelper::ExpectChangeIdPButtonHidden() {
  DialogJS().ExpectHiddenPath(kChangeIdPButtonContainer);
}

void LockScreenReauthDialogTestHelper::WaitForSigninWebview() {
  WaitForAuthenticatorToLoad();
  DialogJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kWebviewContainer)
      ->Wait();
}

void LockScreenReauthDialogTestHelper::ExpectErrorScreenVisible() {
  DialogJS().CreateVisibilityWaiter(/*visibility=*/true, kErrorScreen)->Wait();
}

void LockScreenReauthDialogTestHelper::ExpectSigninWebviewVisible() {
  DialogJS().ExpectVisiblePath(kWebviewContainer);
}

void LockScreenReauthDialogTestHelper::ExpectSigninWebviewHidden() {
  DialogJS().ExpectHiddenPath(kWebviewContainer);
}

void LockScreenReauthDialogTestHelper::ExpectGaiaScreenVisible() {
  DialogJS().ExpectAttributeEQ("isDefaultSsoProvider", {"main-element"}, false);
}

void LockScreenReauthDialogTestHelper::ExpectSamlConfirmPasswordVisible() {
  DialogJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kSamlConfirmPasswordScreen)
      ->Wait();
}

void LockScreenReauthDialogTestHelper::ExpectPasswordConfirmInputHidden() {
  DialogJS()
      .CreateVisibilityWaiter(/*visibility=*/false, kPasswordConfirmInput)
      ->Wait();
}

void LockScreenReauthDialogTestHelper::ExpectPasswordConfirmInputVisible() {
  DialogJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kPasswordConfirmInput)
      ->Wait();
}

void LockScreenReauthDialogTestHelper::SendConfirmPassword(
    const std::string& password_to_confirm) {
  DialogJS().TypeIntoPath(password_to_confirm, kPasswordInput);
  DialogJS().TapOnPath(kPasswordSubmit);
}

void LockScreenReauthDialogTestHelper::SetManualPasswords(
    const std::string& password,
    const std::string& confirm_password) {
  DialogJS().TypeIntoPath(password, kPasswordInput);
  DialogJS().TypeIntoPath(confirm_password, kPasswordConfirmInput);
  DialogJS().TapOnPath(kPasswordSubmit);
}

test::UIPath LockScreenReauthDialogTestHelper::SamlNoticeMessage() const {
  return kSamlNoticeMessage;
}

void LockScreenReauthDialogTestHelper::WaitForSamlNoticeMessage() {
  DialogJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kSamlNoticeMessage)
      ->Wait();
}

void LockScreenReauthDialogTestHelper::ExpectSamlNoticeMessageVisible() {
  DialogJS().ExpectVisiblePath(kSamlNoticeMessage);
}

void LockScreenReauthDialogTestHelper::ExpectSamlNoticeMessageHidden() {
  DialogJS().ExpectHiddenPath(kSamlNoticeMessage);
}

void LockScreenReauthDialogTestHelper::WaitForSamlIdpPageLoad() {
  // Rely on the invariant that SAML notice message is shown if and only
  // if the dialog is currently displaying a 3P IdP page.
  WaitForSamlNoticeMessage();
}

content::WebContents* LockScreenReauthDialogTestHelper::DialogWebContents() {
  CHECK(reauth_dialog_);
  return reauth_dialog_->GetWebUIForTest()->GetWebContents();
}

test::JSChecker LockScreenReauthDialogTestHelper::DialogJS() {
  return test::JSChecker(DialogWebContents());
}

test::JSChecker LockScreenReauthDialogTestHelper::NetworkJS() {
  CHECK(network_dialog_);
  return test::JSChecker(network_dialog_->GetWebUIForTest()->GetWebContents());
}

test::JSChecker LockScreenReauthDialogTestHelper::SigninFrameJS() {
  content::RenderFrameHost* frame =
      signin::GetAuthFrame(DialogWebContents(), kSigninFrame);
  CHECK(frame);
  CHECK(frame->IsDOMContentLoaded());
  return test::JSChecker(frame);
}

void LockScreenReauthDialogTestHelper::WaitForAuthenticatorToLoad() {
  base::test::TestFuture<void> future;
  if (!main_handler_->IsAuthenticatorLoaded(future.GetCallback())) {
    EXPECT_TRUE(future.Wait());
  }
}

void LockScreenReauthDialogTestHelper::WaitForReauthDialogToClose() {
  base::test::TestFuture<void> future;
  if (!reauth_dialog_->IsClosedForTesting(future.GetCallback())) {
    EXPECT_TRUE(future.Wait());
  }
}

void LockScreenReauthDialogTestHelper::WaitForReauthDialogToLoad() {
  base::test::TestFuture<void> future;
  if (!reauth_dialog_->IsLoadedForTesting(future.GetCallback())) {
    EXPECT_TRUE(future.Wait());
  }
}

void LockScreenReauthDialogTestHelper::WaitForNetworkDialogToLoad() {
  CHECK(reauth_dialog_);
  base::test::TestFuture<void> future;
  if (!reauth_dialog_->IsNetworkDialogLoadedForTesting(future.GetCallback())) {
    EXPECT_TRUE(future.Wait());
  }
}

void LockScreenReauthDialogTestHelper::WaitForCaptivePortalDialogToLoad() {
  base::test::TestFuture<void> future;
  if (!reauth_dialog_->IsCaptivePortalDialogLoadedForTesting(
          future.GetCallback())) {
    EXPECT_TRUE(future.Wait());
  }

  captive_portal_dialog_ =
      reauth_dialog_->get_captive_portal_dialog_for_testing();
}

void LockScreenReauthDialogTestHelper::WaitForCaptivePortalDialogToShow() {
  base::test::TestFuture<void> future;
  if (!captive_portal_dialog_->IsDialogShownForTesting(future.GetCallback())) {
    EXPECT_TRUE(future.Wait());
  }
}

void LockScreenReauthDialogTestHelper::WaitForCaptivePortalDialogToClose() {
  base::test::TestFuture<void> future;
  if (!captive_portal_dialog_->IsDialogClosedForTesting(future.GetCallback())) {
    EXPECT_TRUE(future.Wait());
  }
}

void LockScreenReauthDialogTestHelper::WaitForNetworkDialogAndSetHandlers() {
  WaitForNetworkDialogToLoad();

  network_dialog_ = reauth_dialog_->get_network_dialog_for_testing();
  if (!network_dialog_ || !network_dialog_->GetWebUIForTest()) {
    ADD_FAILURE() << "Could not retrieve LockScreenNetworkDialog";
  }
  network_webui_controller_ = static_cast<LockScreenNetworkUI*>(
      network_dialog_->GetWebUIForTest()->GetController());
  if (!network_webui_controller_) {
    ADD_FAILURE() << "Could not retrieve LockScreenNetworkUI";
  }
  network_handler_ = network_webui_controller_->GetMainHandlerForTests();
  if (!network_handler_) {
    ADD_FAILURE() << "Could not retrieve LockScreenNetworkHandler";
  }
}

// Makes the main dialog show its inner 'network' dialog and fetches
// pointers to the Dialog, WebUI Controller and Message Handler.
void LockScreenReauthDialogTestHelper::ShowNetworkScreenAndWait() {
  reauth_dialog_->ShowLockScreenNetworkDialog();
  WaitForNetworkDialogAndSetHandlers();
}

void LockScreenReauthDialogTestHelper::CloseNetworkScreen() {
  reauth_dialog_->DismissLockScreenNetworkDialog();
}

void LockScreenReauthDialogTestHelper::ExpectNetworkDialogVisible() {
  NetworkJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kNetworkDialog)
      ->Wait();
}

void LockScreenReauthDialogTestHelper::ExpectNetworkDialogHidden() {
  EXPECT_FALSE(reauth_dialog_->is_network_dialog_visible_for_testing());
}

void LockScreenReauthDialogTestHelper::ClickCloseNetworkButton() {
  NetworkJS().TapOnPath(kNetworkCancelButton);
}

void LockScreenReauthDialogTestHelper::ExpectCaptivePortalDialogVisible() {
  EXPECT_TRUE(captive_portal_dialog_->IsRunning());
}

void LockScreenReauthDialogTestHelper::ExpectCaptivePortalDialogHidden() {
  EXPECT_FALSE(captive_portal_dialog_->IsRunning());
}

void LockScreenReauthDialogTestHelper::CloseCaptivePortalDialogAndWait() {
  captive_portal_dialog_->Close();
  WaitForCaptivePortalDialogToClose();
}

}  // namespace ash
