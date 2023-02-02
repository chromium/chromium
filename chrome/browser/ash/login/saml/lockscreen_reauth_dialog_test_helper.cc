// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/lockscreen_reauth_dialog_test_helper.h"

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
const test::UIPath kSamlContainer = {"main-element", "body"};
const test::UIPath kMainVerifyButton = {"main-element",
                                        "nextButtonVerifyScreen"};
const test::UIPath kMainCancelButton = {"main-element",
                                        "cancelButtonVerifyScreen"};
const test::UIPath kErrorCancelButton = {"main-element",
                                         "cancelButtonErrorScreen"};
const test::UIPath kSamlCancelButton = {"main-element", "saml-close-button"};
const test::UIPath kChangeIdPButton = {"main-element", "change-account"};
const test::UIPath kMainScreen = {"main-element", "verifyAccountScreen"};
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
absl::optional<LockScreenReauthDialogTestHelper>
LockScreenReauthDialogTestHelper::ShowDialogAndWait() {
  LockScreenReauthDialogTestHelper dialog_test_helper;
  if (!dialog_test_helper.ShowDialogAndWaitImpl())
    return absl::nullopt;
  return dialog_test_helper;
}

// static
absl::optional<LockScreenReauthDialogTestHelper>
LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad() {
  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  if (!reauth_dialog_helper.has_value()) {
    return absl::nullopt;
  }

  reauth_dialog_helper->ForceSamlRedirect();

  // Expect the 'Verify Account' screen (the first screen the dialog shows) to
  // be visible and proceed to the SAML page.
  reauth_dialog_helper->WaitForVerifyAccountScreen();
  reauth_dialog_helper->ClickVerifyButton();

  reauth_dialog_helper->WaitForSamlScreen();
  reauth_dialog_helper->ExpectVerifyAccountScreenHidden();

  reauth_dialog_helper->WaitForIdpPageLoad();
  return reauth_dialog_helper;
}

bool LockScreenReauthDialogTestHelper::ShowDialogAndWaitImpl() {
  // Check precondition: Screen is locked.
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    ADD_FAILURE() << "Screen must be locked";
    return false;
  }

  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, true);

  LockScreenStartReauthDialog::Show();

  // Fetch the dialog, WebUi controller and main message handler.
  reauth_dialog_ = LockScreenStartReauthDialog::GetInstance();
  WaitForReauthDialogToLoad();
  if (!reauth_dialog_ || !reauth_dialog_->GetWebUIForTest()) {
    ADD_FAILURE() << "Could not retrieve LockScreenStartReauthDialog";
    return false;
  }
  reauth_webui_controller_ = static_cast<LockScreenStartReauthUI*>(
      reauth_dialog_->GetWebUIForTest()->GetController());
  if (!reauth_webui_controller_) {
    ADD_FAILURE() << "Could not retrieve LockScreenStartReauthUI";
    return false;
  }
  main_handler_ = reauth_webui_controller_->GetMainHandler();
  if (!main_handler_) {
    ADD_FAILURE() << "Could not retrieve LockScreenReauthHandler";
    return false;
  }
  return true;
}

void LockScreenReauthDialogTestHelper::ForceSamlRedirect() {
  main_handler_->force_saml_redirect_for_testing();
}

void LockScreenReauthDialogTestHelper::WaitForVerifyAccountScreen() {
  test::JSChecker js_checker = DialogJS();
  js_checker.CreateVisibilityWaiter(true, kMainScreen)->Wait();
  js_checker.ExpectVisiblePath(kMainScreen);
}

void LockScreenReauthDialogTestHelper::ClickVerifyButton() {
  ExpectVerifyAccountScreenVisible();

  // Expect the main screen to be visible and proceed to the SAML page.
  DialogJS().TapOnPath(kMainVerifyButton);
}

void LockScreenReauthDialogTestHelper::ClickCancelButtonOnVerifyScreen() {
  ExpectVerifyAccountScreenVisible();
  DialogJS().TapOnPath(kMainCancelButton);
}

void LockScreenReauthDialogTestHelper::ClickCancelButtonOnErrorScreen() {
  ExpectErrorScreenVisible();
  DialogJS().TapOnPath(kErrorCancelButton);
}

void LockScreenReauthDialogTestHelper::ClickCancelButtonOnSamlScreen() {
  ExpectSamlScreenVisible();
  DialogJS().TapOnPath(kSamlCancelButton);
}

void LockScreenReauthDialogTestHelper::ClickChangeIdPButtonOnSamlScreen() {
  ExpectSamlScreenVisible();
  DialogJS().TapOnPath(kChangeIdPButton);
}

void LockScreenReauthDialogTestHelper::WaitForSamlScreen() {
  WaitForAuthenticatorToLoad();
  DialogJS().CreateVisibilityWaiter(true, kSamlContainer)->Wait();
  DialogJS().ExpectVisiblePath(kSamlContainer);
}

void LockScreenReauthDialogTestHelper::ExpectVerifyAccountScreenVisible() {
  DialogJS().ExpectVisiblePath(kMainScreen);
}

void LockScreenReauthDialogTestHelper::ExpectVerifyAccountScreenHidden() {
  DialogJS().ExpectHiddenPath(kMainScreen);
}

void LockScreenReauthDialogTestHelper::ExpectErrorScreenVisible() {
  DialogJS().CreateVisibilityWaiter(true, kErrorScreen)->Wait();
  DialogJS().ExpectVisiblePath(kErrorScreen);
}

void LockScreenReauthDialogTestHelper::ExpectSamlScreenVisible() {
  DialogJS().ExpectVisiblePath(kSamlContainer);
}

void LockScreenReauthDialogTestHelper::ExpectSamlScreenHidden() {
  DialogJS().ExpectHiddenPath(kSamlContainer);
}

void LockScreenReauthDialogTestHelper::ExpectGaiaScreenVisible() {
  DialogJS().ExpectAttributeEQ("isDefaultSsoProvider", {"main-element"}, false);
}

void LockScreenReauthDialogTestHelper::ExpectSamlConfirmPasswordVisible() {
  DialogJS().CreateVisibilityWaiter(true, kSamlConfirmPasswordScreen)->Wait();
  DialogJS().ExpectVisiblePath(kSamlConfirmPasswordScreen);
}

void LockScreenReauthDialogTestHelper::ExpectPasswordConfirmInputHidden() {
  DialogJS().CreateVisibilityWaiter(false, kPasswordConfirmInput)->Wait();
  DialogJS().ExpectHiddenPath(kPasswordConfirmInput);
}

void LockScreenReauthDialogTestHelper::ExpectPasswordConfirmInputVisible() {
  DialogJS().CreateVisibilityWaiter(true, kPasswordConfirmInput)->Wait();
  DialogJS().ExpectVisiblePath(kPasswordConfirmInput);
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

void LockScreenReauthDialogTestHelper::WaitForIdpPageLoad() {
  content::DOMMessageQueue message_queue(DialogWebContents());
  content::ExecuteScriptAsync(
      DialogWebContents(),
      R"($('main-element').authenticator_.addEventListener('authFlowChange',
            function f() {
              $('main-element').authenticator_.removeEventListener(
                  'authFlowChange', f);
              window.domAutomationController.send('Loaded');
            });)");
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"Loaded\"");
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
  NetworkJS().CreateVisibilityWaiter(true, kNetworkDialog)->Wait();
  NetworkJS().ExpectVisiblePath(kNetworkDialog);
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
