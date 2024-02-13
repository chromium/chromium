// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/oobe_screens_utils.h"

#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/consumer_update_screen.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/browser/ash/login/screens/update_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/guest_tos_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/pin_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/events/devices/touchpad_device.h"

namespace ash {
namespace test {

namespace {

const ui::TouchpadDevice kSampleTouchpadInternal(1,
                                                 ui::INPUT_DEVICE_INTERNAL,
                                                 "kSampleTouchpadInternal",
                                                 "",
                                                 base::FilePath(),
                                                 0x1111,
                                                 0x4444,
                                                 0);

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
  // Wait until OOBE is fully initialized and the 'Get Started' button enabled.
  OobeJS()
      .CreateEnabledWaiter(true /* enabled */,
                           {"connect", "welcomeScreen", "getStarted"})
      ->Wait();
}

void TapWelcomeNext() {
  OobeJS().TapOnPath({"connect", "welcomeScreen", "getStarted"});
}

void WaitForNetworkSelectionScreen() {
  WaitFor(NetworkScreenView::kScreenId);
}

void TapNetworkSelectionNext() {
  OobeJS()
      .CreateEnabledWaiter(true /* enabled */,
                           {"network-selection", "nextButton"})
      ->Wait();
  OobeJS().TapOnPath({"network-selection", "nextButton"});
}

void WaitForUpdateScreen() {
  WaitFor(UpdateView::kScreenId);
  OobeJS().CreateVisibilityWaiter(true, {"oobe-update"})->Wait();
}

void ExitUpdateScreenNoUpdate() {
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::ERROR);

  UpdateScreen* screen =
      WizardController::default_controller()->GetScreen<UpdateScreen>();
  screen->GetVersionUpdaterForTesting()->UpdateStatusChangedForTesting(status);
}

void WaitForConsumerUpdateScreen() {
  if (!LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build) {
    return;
  }
  WaitFor(ConsumerUpdateScreenView::kScreenId);
  OobeJS().CreateVisibilityWaiter(true, {"consumer-update"})->Wait();
}

void ExitConsumerUpdateScreenNoUpdate() {
  if (!LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build) {
    return;
  }
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::ERROR);

  ConsumerUpdateScreen* screen =
      WizardController::default_controller()->GetScreen<ConsumerUpdateScreen>();
  screen->get_version_updater_for_testing()->UpdateStatusChangedForTesting(
      status);
}

void WaitForFingerprintScreen() {
  LOG(INFO) << "Waiting for 'fingerprint-setup' screen.";
  OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();
  LOG(INFO) << "Waiting for fingerprint setup screen "
               "to show.";
  OobeJS().CreateVisibilityWaiter(true, {"fingerprint-setup"})->Wait();
  LOG(INFO) << "Waiting for fingerprint setup screen "
               "to show setupFingerprint.";
  OobeJS()
      .CreateVisibilityWaiter(true, {"fingerprint-setup", "setupFingerprint"})
      ->Wait();
}

void ExitFingerprintPinSetupScreen() {
  OobeJS().ExpectVisiblePath({"fingerprint-setup", "setupFingerprint"});
  // This might be the last step in flow. Synchronous execute gets stuck as
  // WebContents may be destroyed in the process. So it may never return.
  // So we use ExecuteAsync() here.
  OobeJS().ExecuteAsync("$('fingerprint-setup').$.skipStart.click()");
  LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
               "to close.";
  WaitForExit(FingerprintSetupScreenView::kScreenId);
}

void WaitForPinSetupScreen() {
  WaitFor(PinSetupScreenView::kScreenId);
}

void ExitPinSetupScreen() {
  // This might be the last step in flow. Synchronous execute gets stuck as
  // WebContents may be destroyed in the process. So it may never return.
  // So we use ExecuteAsync() here.
  OobeJS().ExecuteAsync("$('pin-setup').$.setupSkipButton.click()");
  WaitForExit(PinSetupScreenView::kScreenId);
}

void SkipToEnrollmentOnRecovery() {
  WaitForWelcomeScreen();
  TapWelcomeNext();

  WaitForNetworkSelectionScreen();
  TapNetworkSelectionNext();

  WaitForUpdateScreen();
  ExitUpdateScreenNoUpdate();

  WaitFor(EnrollmentScreenView::kScreenId);
}

void WaitForEnrollmentScreen() {
  WaitFor(EnrollmentScreenView::kScreenId);
}

void WaitForUserCreationScreen() {
  WaitFor(UserCreationView::kScreenId);
}

void TapForPersonalUseCrRadioButton() {
  OobeJS().TapOnPath({"user-creation", "selfButton"});
}

void TapUserCreationNext() {
  OobeJS().TapOnPath({"user-creation", "nextButton"});
}

void WaitForGaiaInfoScreen() {
  WaitFor(GaiaInfoScreenView::kScreenId);
}

void WaitForOobeJSReady() {
  if (!LoginDisplayHost::default_host()->GetOobeUI()) {
    base::RunLoop run_loop;
    LoginDisplayHost::default_host()->AddWizardCreatedObserverForTests(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  base::RunLoop run_loop;
  if (!LoginDisplayHost::default_host()->GetOobeUI()->IsJSReady(
          run_loop.QuitClosure())) {
    run_loop.Run();
  }
}

void WaitForSyncConsentScreen() {
  if (!LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build)
    return;
  WaitFor(SyncConsentScreenView::kScreenId);
}

void ExitScreenSyncConsent() {
  if (!LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build)
    return;
  SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
      WizardController::default_controller()->GetScreen(
          SyncConsentScreenView::kScreenId));

  screen->SetProfileSyncDisabledByPolicyForTesting(true);
  screen->OnStateChanged(nullptr);
  WaitForExit(SyncConsentScreenView::kScreenId);
}

void WaitForConsolidatedConsentScreen() {
  if (!LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build)
    return;
  WaitFor(ConsolidatedConsentScreenView::kScreenId);
}

void TapConsolidatedConsentAccept() {
  if (!LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build)
    return;
  OobeJS().TapOnPath({"consolidated-consent", "acceptButton"});
}

void WaitForGuestTosScreen() {
  WaitFor(GuestTosScreenView::kScreenId);
}

void TapGuestTosAccept() {
  OobeJS().TapOnPath({"guest-tos", "acceptButton"});
}

void ClickSignInFatalScreenActionButton() {
  OobeJS().ClickOnPath({"signin-fatal-error", "actionButton"});
}

bool IsScanningRequestedOnNetworkScreen() {
  return OobeJS().GetAttributeBool(
      "enableWifiScans",
      {"network-selection", "networkSelectLogin", "networkSelect"});
}

bool IsScanningRequestedOnErrorScreen() {
  return OobeJS().GetAttributeBool(
      "enableWifiScans",
      {"error-message", "offline-network-control", "networkSelect"});
}

// Set Fake Touchpad device.
void SetFakeTouchpadDevice() {
  Shell::Get()->input_device_settings_controller()->OnTouchpadListUpdated(
      {kSampleTouchpadInternal}, {});
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

OobeUiDestroyedWaiter::OobeUiDestroyedWaiter(OobeUI* oobe_ui) {
  oobe_ui_observation_.Observe(oobe_ui);
}

OobeUiDestroyedWaiter::~OobeUiDestroyedWaiter() {}

void OobeUiDestroyedWaiter::Wait() {
  if (was_destroyed_)
    return;

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();

  ASSERT_TRUE(was_destroyed_)
      << "Timed out while waiting for OobeUI to be destroyed!";
}

void OobeUiDestroyedWaiter::OnDestroyingOobeUI() {
  oobe_ui_observation_.Reset();
  was_destroyed_ = true;
  run_loop_->Quit();
}

// Start observing, tap/click and wait.
void TapOnPathAndWaitForOobeToBeDestroyed(
    std::initializer_list<std::string_view> element_ids) {
  // Get the OOBE WebUI Controller (OobeUI) and start observing.
  content::WebContents* web_contents =
      LoginDisplayHost::default_host()->GetOobeWebContents();
  CHECK(web_contents);
  OobeUI* oobe_ui =
      static_cast<OobeUI*>(web_contents->GetWebUI()->GetController());
  OobeUiDestroyedWaiter observer{oobe_ui};

  test::OobeJS().TapOnPathAsync(element_ids);

  observer.Wait();
}

}  // namespace test
}  // namespace ash
