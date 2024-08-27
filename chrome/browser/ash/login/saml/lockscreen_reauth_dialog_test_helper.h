// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_LOCKSCREEN_REAUTH_DIALOG_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_LOCKSCREEN_REAUTH_DIALOG_TEST_HELPER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/test/js_checker.h"

namespace content {
class WebContents;
}

namespace ash {

class LockScreenStartReauthDialog;
class LockScreenReauthHandler;
class LockScreenNetworkDialog;
class LockScreenNetworkUI;
class LockScreenCaptivePortalDialog;
class NetworkConfigMessageHandler;

// Supports triggering the online re-authentication dialog on the Chrome OS lock
// screen from browser tests and interacting with it.
class LockScreenReauthDialogTestHelper {
 public:
  // Triggers the online re-authentication dialog.
  // Precondition: A user is logged in and the lock screen is shown.
  // Returns an empty `std::optional` if the operation fails.
  static std::optional<LockScreenReauthDialogTestHelper> ShowDialogAndWait();

  // Triggers the online re-authentication dialog and navigates through initial
  // state until SAML IdP page is loaded. Returns an empty `std::optional` if
  // the operation fails.
  static std::optional<LockScreenReauthDialogTestHelper>
  StartSamlAndWaitForIdpPageLoad();

  // Initialize and return (if successful) an instance of
  // `LockScreenReauthDialogTestHelper` for an already shown online
  // re-authentication dialog.
  static std::optional<LockScreenReauthDialogTestHelper> InitForShownDialog();

  ~LockScreenReauthDialogTestHelper();

  // Non-copyable, movable.
  LockScreenReauthDialogTestHelper(
      const LockScreenReauthDialogTestHelper& other) = delete;
  LockScreenReauthDialogTestHelper& operator=(
      const LockScreenReauthDialogTestHelper& other) = delete;

  LockScreenReauthDialogTestHelper(LockScreenReauthDialogTestHelper&& other);
  LockScreenReauthDialogTestHelper& operator=(
      LockScreenReauthDialogTestHelper&& other);

  void ClickCancelButtonOnErrorScreen();

  void ClickCancelButtonOnSamlScreen();

  // Clicks the 'Enter Google Account Info' button on the SAML screen.
  void ClickChangeIdPButtonOnSamlScreen();

  // Primary Gaia button is the "Next" button on Gaia pages.
  void ClickPrimaryGaiaButton();
  void WaitForPrimaryGaiaButtonToBeEnabled();

  // Check visibility of native Gaia button on online re-authentication dialog.
  void ExpectGaiaButtonsVisible();
  void ExpectGaiaButtonsHidden();

  // Check visibility of the button which allows to restart online flow from the
  // Gaia page.
  void ExpectChangeIdPButtonVisible();
  void ExpectChangeIdPButtonHidden();

  void WaitForSigninWebview();

  void ExpectErrorScreenVisible();
  void ExpectSigninWebviewVisible();
  void ExpectSigninWebviewHidden();

  void ExpectGaiaScreenVisible();

  // Next members allow to check visibility of some elements on 'confirm
  // password screen' and also help to fill forms. Precondition: 'confirm
  // password screen' is visible.
  void ExpectSamlConfirmPasswordVisible();
  void ExpectPasswordConfirmInputHidden();
  void ExpectPasswordConfirmInputVisible();
  void SendConfirmPassword(const std::string& password_to_confirm);
  void SetManualPasswords(const std::string& password,
                          const std::string& confirm_password);

  void ShowNetworkScreenAndWait();
  void WaitForNetworkDialogAndSetHandlers();
  void CloseNetworkScreen();

  void ExpectNetworkDialogVisible();
  void ExpectNetworkDialogHidden();
  void ClickCloseNetworkButton();

  // Wait until the main dialog closes.
  void WaitForReauthDialogToClose();

  // SAML notice message is displayed when we show a 3P IdP page.
  test::UIPath SamlNoticeMessage() const;
  void WaitForSamlNoticeMessage();
  void ExpectSamlNoticeMessageVisible();
  void ExpectSamlNoticeMessageHidden();

  void WaitForSamlIdpPageLoad();

  // Next members allow to wait for the captive portal dialog to load (i.e. be
  // initialized in `LockScreenStartReauthDialog`), be shown or be closed.
  // Precondition: Main dialog must exist, since it owns the portal dialog.
  void WaitForCaptivePortalDialogToLoad();
  void WaitForCaptivePortalDialogToShow();
  void WaitForCaptivePortalDialogToClose();

  void ExpectCaptivePortalDialogVisible();
  void ExpectCaptivePortalDialogHidden();
  void CloseCaptivePortalDialogAndWait();

  content::WebContents* DialogWebContents();
  // Returns a JSChecker for the WebContents of the dialog's WebUI.
  test::JSChecker DialogJS();

  // Returns a JSChecker for the WebContents of the network dialog's WebUI.
  test::JSChecker NetworkJS();

  // Returns a JSChecker for the WebContents of the signin frame webview.
  // Precondition: The SAML container is visible.
  test::JSChecker SigninFrameJS();

 private:
  // Instantiate using public static factory methods.
  LockScreenReauthDialogTestHelper();

  void WaitForAuthenticatorToLoad();
  void WaitForReauthDialogToLoad();

  // Waits for the network dialog to load.
  // Precondition: Main dialog must exist, since it owns the network dialog.
  void WaitForNetworkDialogToLoad();

  // Main Dialog
  raw_ptr<LockScreenStartReauthDialog, AcrossTasksDanglingUntriaged>
      reauth_dialog_ = nullptr;
  raw_ptr<LockScreenReauthHandler, AcrossTasksDanglingUntriaged> main_handler_ =
      nullptr;

  // Network dialog which is owned by the main dialog.
  raw_ptr<LockScreenNetworkDialog, AcrossTasksDanglingUntriaged>
      network_dialog_ = nullptr;
  raw_ptr<LockScreenNetworkUI, AcrossTasksDanglingUntriaged>
      network_webui_controller_ = nullptr;
  raw_ptr<NetworkConfigMessageHandler, AcrossTasksDanglingUntriaged>
      network_handler_ = nullptr;

  // Captive portal dialog which is owned by the main dialog.
  raw_ptr<LockScreenCaptivePortalDialog, AcrossTasksDanglingUntriaged>
      captive_portal_dialog_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_LOCKSCREEN_REAUTH_DIALOG_TEST_HELPER_H_
