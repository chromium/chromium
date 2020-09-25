// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/oobe_screens_utils.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/discover_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace chromeos {
namespace test {

namespace {

void WaitFor(OobeScreenId screen_id) {
  OobeScreenWaiter(screen_id).Wait();
  LOG(INFO) << "Switched to '" << screen_id.name << "' screen.";
}

void WaitForExit(OobeScreenId screen_id) {
  OobeScreenExitWaiter(screen_id).Wait();
  LOG(INFO) << "Screen '" << screen_id.name << "' is done.";
}

}  // namespace

void WaitForWelcomeScreen() {
  WaitFor(WelcomeView::kScreenId);
}

void TapWelcomeNext() {
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "welcomeNextButton"});
}

void WaitForNetworkSelectionScreen() {
  WaitFor(NetworkScreenView::kScreenId);
}

void TapNetworkSelectionNext() {
  test::OobeJS()
      .CreateEnabledWaiter(true /* enabled */,
                           {"network-selection", "nextButton"})
      ->Wait();
  test::OobeJS().TapOnPath({"network-selection", "nextButton"});
}

void WaitForUpdateScreen() {
  WaitFor(UpdateView::kScreenId);
  test::OobeJS().CreateVisibilityWaiter(true, {"oobe-update"})->Wait();
}

void ExitUpdateScreenNoUpdate() {
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::ERROR);

  UpdateScreen* screen = UpdateScreen::Get(
      WizardController::default_controller()->screen_manager());
  screen->GetVersionUpdaterForTesting()->UpdateStatusChangedForTesting(status);
}

void WaitForFingerprintScreen() {
  LOG(INFO) << "Waiting for 'fingerprint-setup' screen.";
  OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();
  LOG(INFO) << "Waiting for fingerprint setup screen "
               "to show.";
  test::OobeJS().CreateVisibilityWaiter(true, {"fingerprint-setup"})->Wait();
  LOG(INFO) << "Waiting for fingerprint setup screen "
               "to show setupFingerprint.";
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"fingerprint-setup", "setupFingerprint"})
      ->Wait();
}

void ExitFingerprintPinSetupScreen() {
  test::OobeJS().ExpectVisiblePath({"fingerprint-setup", "placeFinger"});
  // This might be the last step in flow. Synchronous execute gets stuck as
  // WebContents may be destroyed in the process. So it may never return.
  // So we use ExecuteAsync() here.
  test::OobeJS().ExecuteAsync("$('fingerprint-setup').$.setupLater.click()");
  LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
               "to close.";
  WaitForExit(FingerprintSetupScreenView::kScreenId);
}

void WaitForDiscoverScreen() {
  WaitFor(DiscoverScreenView::kScreenId);
}

void ExitDiscoverPinSetupScreen() {
  // This might be the last step in flow. Synchronous execute gets stuck as
  // WebContents may be destroyed in the process. So it may never return.
  // So we use ExecuteAsync() here.
  test::OobeJS().ExecuteAsync(
      "$('discover-impl').root.querySelector('discover-pin-setup-module')."
      "$.setupSkipButton.click()");
  WaitForExit(DiscoverScreenView::kScreenId);
}

void SkipToEnrollmentOnRecovery() {
  test::WaitForWelcomeScreen();
  test::TapWelcomeNext();

  test::WaitForNetworkSelectionScreen();
  test::TapNetworkSelectionNext();

  WaitForEulaScreen();
  TapEulaAccept();

  test::WaitForUpdateScreen();
  test::ExitUpdateScreenNoUpdate();

  WaitFor(EnrollmentScreenView::kScreenId);
}

void WaitForEnrollmentScreen() {
  WaitFor(EnrollmentScreenView::kScreenId);
}

void WaitForUserCreationScreen() {
  WaitFor(UserCreationView::kScreenId);
}

void TapUserCreationNext() {
  test::OobeJS().TapOnPath({"user-creation", "nextButton"});
}

void WaitForEulaScreen() {
  if (!WizardController::IsBrandedBuildForTesting())
    return;
  WaitFor(EulaView::kScreenId);
}

void TapEulaAccept() {
  if (!WizardController::IsBrandedBuildForTesting())
    return;
  test::OobeJS().TapOnPath({"oobe-eula-md", "acceptButton"});
}

void WaitForSyncConsentScreen() {
  if (!SyncConsentScreen::IsBrandedBuildForTesting())
    return;
  WaitFor(SyncConsentScreenView::kScreenId);
}

void ExitScreenSyncConsent() {
  if (!SyncConsentScreen::IsBrandedBuildForTesting())
    return;
  SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
      WizardController::default_controller()->GetScreen(
          SyncConsentScreenView::kScreenId));

  screen->SetProfileSyncDisabledByPolicyForTesting(true);
  screen->OnStateChanged(nullptr);
  WaitForExit(SyncConsentScreenView::kScreenId);
}

LanguageReloadObserver::LanguageReloadObserver(WelcomeScreen* welcome_screen)
    : welcome_screen_(welcome_screen) {
  welcome_screen_->AddObserver(this);
}

void LanguageReloadObserver::OnLanguageListReloaded() {
  run_loop_.Quit();
}

void LanguageReloadObserver::Wait() {
  run_loop_.Run();
}

LanguageReloadObserver::~LanguageReloadObserver() {
  welcome_screen_->RemoveObserver(this);
}

}  // namespace test
}  // namespace chromeos
