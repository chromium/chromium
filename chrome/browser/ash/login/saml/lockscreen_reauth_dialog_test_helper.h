// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_LOCKSCREEN_REAUTH_DIALOG_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_LOCKSCREEN_REAUTH_DIALOG_TEST_HELPER_H_

#include "chrome/browser/ash/login/test/js_checker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
class LockScreenStartReauthDialog;
class LockScreenStartReauthUI;
class LockScreenReauthHandler;
class LockScreenNetworkDialog;
class LockScreenNetworkUI;
class NetworkConfigMessageHandler;
}  // namespace chromeos

namespace content {
class WebContents;
}

namespace ash {

class InSessionPasswordSyncManager;

// Supports triggering the online re-authentication dialog on the Chrome OS lock
// screen from browser tests and interacting with it.
class LockScreenReauthDialogTestHelper {
 public:
  // Triggers the online re-authentication dialog.
  // Precondition: A user is logged in and the lock screen is shown.
  // Returns an empty `absl::optional` if the operation fails.
  static absl::optional<LockScreenReauthDialogTestHelper> ShowDialogAndWait();

  ~LockScreenReauthDialogTestHelper();

  // Non-copyable, movable.
  LockScreenReauthDialogTestHelper(
      const LockScreenReauthDialogTestHelper& other) = delete;
  LockScreenReauthDialogTestHelper& operator=(
      const LockScreenReauthDialogTestHelper& other) = delete;

  LockScreenReauthDialogTestHelper(LockScreenReauthDialogTestHelper&& other);
  LockScreenReauthDialogTestHelper& operator=(
      LockScreenReauthDialogTestHelper&& other);

  // Forces SAML redirect regardless of email.
  void ForceSamlRedirect();

  // Waits for the 'Verify Account' screen (the first screen the dialog shows)
  // to be visible.
  void WaitForVerifyAccountScreen();

  // Clicks the 'Verify' button on the 'Verify Account' screen and wait for the
  // authenticator page to be loaded.
  // For SAML flows this proceeds to the SAML flow.
  void ClickVerifyButton();

  // Waits for a screen with the `saml-container` element to be shown.
  void WaitForSamlScreen();

  void ExpectVerifyAccountScreenVisible();
  void ExpectVerifyAccountScreenHidden();
  void ExpectSamlScreenVisible();

  void ShowNetworkScreenAndWait();
  void WaitForNetworkDialogAndSetHandlers();
  void CloseNetworkScreen();

  void ExpectNetworkDialogVisible();
  void ExpectNetworkDialogHidden();
  void ClickCloseNetworkButton();

  // Wait for the SAML IdP page to load.
  // Precondition: The SAML container is visible.
  void WaitForIdpPageLoad();

  // Returns the WebContents of the dialog's WebUI.
  content::WebContents* DialogWebContents();
  // Returns a JSChecker for the WebContents of the dialog's WebUI.
  test::JSChecker DialogJS();

  // Returns a JSChecker for the WebContents of the network dialog's WebUI.
  test::JSChecker NetworkJS();

  // Returns a JSChecker for the WebContents of the signin frame webview.
  // Precondition: The SAML container is visible.
  test::JSChecker SigninFrameJS();

 private:
  // Instantiate using the static function `ShowDialogAndWait`.
  LockScreenReauthDialogTestHelper();

  bool ShowDialogAndWaitImpl();

  void WaitForAuthenticatorToLoad();
  void WaitForReauthDialogToLoad();

  // Waits for the network dialog to load.
  // Precondition: Main dialog must exist, since it owns the network dialog.
  void WaitForNetworkDialogToLoad();

  // Main Dialog
  InSessionPasswordSyncManager* password_sync_manager_ = nullptr;
  chromeos::LockScreenStartReauthDialog* reauth_dialog_ = nullptr;
  chromeos::LockScreenStartReauthUI* reauth_webui_controller_ = nullptr;
  chromeos::LockScreenReauthHandler* main_handler_ = nullptr;

  // Network dialog which is owned by the main dialog.
  chromeos::LockScreenNetworkDialog* network_dialog_ = nullptr;
  chromeos::LockScreenNetworkUI* network_webui_controller_ = nullptr;
  chromeos::NetworkConfigMessageHandler* network_handler_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_LOCKSCREEN_REAUTH_DIALOG_TEST_HELPER_H_
