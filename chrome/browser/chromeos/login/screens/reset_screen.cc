// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/reset_screen.h"

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/scoped_guest_button_blocker.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/reset/metrics.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace {

constexpr const char kUserActionCancelReset[] = "cancel-reset";
constexpr const char kUserActionResetRestartPressed[] = "restart-pressed";
constexpr const char kUserActionResetPowerwashPressed[] = "powerwash-pressed";
constexpr const char kUserActionResetLearnMorePressed[] = "learn-more-link";
constexpr const char kUserActionResetRollbackToggled[] = "rollback-toggled";
constexpr const char kUserActionResetShowConfirmationPressed[] =
    "show-confirmation";
constexpr const char kUserActionResetResetConfirmationDismissed[] =
    "reset-confirm-dismissed";
constexpr const char kUserActionTPMFirmwareUpdateLearnMore[] =
    "tpm-firmware-update-learn-more-link";

// If set, callback that will be run to determine TPM firmware update
// availability. Used for tests.
ResetScreen::TpmFirmwareUpdateAvailabilityChecker*
    g_tpm_firmware_update_checker = nullptr;

void StartTPMFirmwareUpdate(
    tpm_firmware_update::Mode requested_mode,
    const std::set<tpm_firmware_update::Mode>& available_modes) {
  if (available_modes.count(requested_mode) == 0) {
    // This should not happen, except for edge cases such as hijacked
    // UI, device policy changing while the dialog was up, etc.
    LOG(ERROR) << "Firmware update no longer available?";
    return;
  }

  std::string mode_string;
  switch (requested_mode) {
    case tpm_firmware_update::Mode::kNone:
      // Error handled below.
      break;
    case tpm_firmware_update::Mode::kPowerwash:
      mode_string = "first_boot";
      break;
    case tpm_firmware_update::Mode::kPreserveDeviceState:
      mode_string = "preserve_stateful";
      break;
    case tpm_firmware_update::Mode::kCleanup:
      mode_string = "cleanup";
      break;
  }

  if (mode_string.empty()) {
    LOG(ERROR) << "Invalid mode " << static_cast<int>(requested_mode);
    return;
  }

  SessionManagerClient::Get()->StartTPMFirmwareUpdate(mode_string);
}

// Checks if powerwash is allowed based on update modes and passes the result
// to |callback|.
void OnUpdateModesAvailable(
    base::OnceCallback<void(bool, base::Optional<tpm_firmware_update::Mode>)>
        callback,
    const std::set<tpm_firmware_update::Mode>& modes) {
  using tpm_firmware_update::Mode;
  for (Mode mode : {Mode::kPowerwash, Mode::kCleanup}) {
    if (modes.count(mode) == 0)
      continue;

    std::move(callback).Run(true, mode);
    return;
  }
  std::move(callback).Run(false, base::nullopt);
}

}  // namespace

// static
void ResetScreen::SetTpmFirmwareUpdateCheckerForTesting(
    TpmFirmwareUpdateAvailabilityChecker* checker) {
  g_tpm_firmware_update_checker = checker;
}

// static
void ResetScreen::CheckIfPowerwashAllowed(
    base::OnceCallback<void(bool, base::Optional<tpm_firmware_update::Mode>)>
        callback) {
  if (g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsEnterpriseManaged()) {
    // Powerwash is allowed by default, if the policy is loaded. Admin can
    // explicitly forbid powerwash. If the policy is not loaded yet, we
    // consider by default that the device is not allowed to powerwash.
    bool is_powerwash_allowed = false;
    CrosSettings::Get()->GetBoolean(kDevicePowerwashAllowed,
                                    &is_powerwash_allowed);
    if (is_powerwash_allowed) {
      std::move(callback).Run(true, base::nullopt);
      return;
    }

    // Check if powerwash is only allowed by the admin specifically for the
    // purpose of installing a TPM firmware update.
    tpm_firmware_update::GetAvailableUpdateModes(
        base::BindOnce(&OnUpdateModesAvailable, base::Passed(&callback)),
        base::TimeDelta());
    return;
  }

  // Devices that are still in OOBE may be subject to forced re-enrollment (FRE)
  // and thus pending for enterprise management. These should not be allowed to
  // powerwash either. Note that taking consumer device ownership has the side
  // effect of dropping the FRE requirement if it was previously in effect.
  std::move(callback).Run(
      AutoEnrollmentController::GetFRERequirement() !=
          AutoEnrollmentController::FRERequirement::kExplicitlyRequired,
      base::nullopt);
}

ResetScreen::ResetScreen(ResetView* view,
                         ErrorScreen* error_screen,
                         const base::RepeatingClosure& exit_callback)
    : BaseScreen(ResetView::kScreenId, OobeScreenPriority::SCREEN_RESET),
      view_(view),
      error_screen_(error_screen),
      exit_callback_(exit_callback),
      tpm_firmware_update_checker_(
          g_tpm_firmware_update_checker
              ? *g_tpm_firmware_update_checker
              : base::BindRepeating(
                    &tpm_firmware_update::GetAvailableUpdateModes)) {
  DCHECK(view_);
  if (view_) {
    view_->Bind(this);
    view_->SetScreenState(ResetView::State::kRestartRequired);
    view_->SetIsRollbackAvailable(false);
    view_->SetIsRollbackRequested(false);
    view_->SetIsTpmFirmwareUpdateAvailable(false);
    view_->SetIsTpmFirmwareUpdateChecked(false);
    view_->SetIsTpmFirmwareUpdateEditable(true);
    view_->SetTpmFirmwareUpdateMode(tpm_firmware_update::Mode::kPowerwash);
    view_->SetShouldShowConfirmationDialog(false);
  }
}

ResetScreen::~ResetScreen() {
  if (view_)
    view_->Unbind();
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
}

// static
void ResetScreen::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFactoryResetRequested, false);
  registry->RegisterIntegerPref(
      prefs::kFactoryResetTPMFirmwareUpdateMode,
      static_cast<int>(tpm_firmware_update::Mode::kNone));
}

void ResetScreen::ShowImpl() {
  if (view_)
    view_->Show();

  // Guest sign-in button should be disabled as sign-in is not possible while
  // reset screen is shown.
  if (!scoped_guest_button_blocker_) {
    scoped_guest_button_blocker_ =
        ash::LoginScreen::Get()->GetScopedGuestButtonBlocker();
  }

  reset::DialogViewType dialog_type =
      reset::DIALOG_VIEW_TYPE_SIZE;  // used by UMA metrics.

  bool restart_required = user_manager::UserManager::Get()->IsUserLoggedIn() ||
                          !base::CommandLine::ForCurrentProcess()->HasSwitch(
                              switches::kFirstExecAfterBoot);
  if (restart_required) {
    if (view_)
      view_->SetScreenState(ResetView::State::kRestartRequired);
    dialog_type = reset::DIALOG_SHORTCUT_RESTART_REQUIRED;
  } else {
    if (view_)
      view_->SetScreenState(ResetView::State::kPowerwashProposal);
  }

  // Set availability of Rollback feature.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRollbackOption)) {
    if (view_)
      view_->SetIsRollbackAvailable(false);
    dialog_type = reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_UNAVAILABLE;
  } else {
    chromeos::DBusThreadManager::Get()
        ->GetUpdateEngineClient()
        ->CanRollbackCheck(base::BindOnce(&ResetScreen::OnRollbackCheck,
                                          weak_ptr_factory_.GetWeakPtr()));
  }

  if (dialog_type < reset::DIALOG_VIEW_TYPE_SIZE) {
    UMA_HISTOGRAM_ENUMERATION("Reset.ChromeOS.PowerwashDialogShown",
                              dialog_type, reset::DIALOG_VIEW_TYPE_SIZE);
  }

  // Set availability of TPM firmware update.
  PrefService* prefs = g_browser_process->local_state();
  bool tpm_firmware_update_requested =
      prefs->HasPrefPath(prefs::kFactoryResetTPMFirmwareUpdateMode);
  if (tpm_firmware_update_requested) {
    // If an update has been requested previously, rely on the earlier update
    // availability test to initialize the dialog. This avoids a race condition
    // where the powerwash dialog gets shown immediately after reboot before the
    // init job to determine update availability has completed.
    if (view_) {
      view_->SetIsTpmFirmwareUpdateAvailable(true);
      view_->SetTpmFirmwareUpdateMode(static_cast<tpm_firmware_update::Mode>(
          prefs->GetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode)));
    }
  } else {
    // If a TPM firmware update hasn't previously been requested, check the
    // system to see whether to offer the checkbox to update TPM firmware. Note
    // that due to the asynchronous availability check, the decision might not
    // be available immediately, so set a timeout of a couple seconds.
    tpm_firmware_update_checker_.Run(
        base::BindOnce(&ResetScreen::OnTPMFirmwareUpdateAvailableCheck,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(10));
  }

  if (view_) {
    view_->SetIsTpmFirmwareUpdateChecked(tpm_firmware_update_requested);
    view_->SetIsTpmFirmwareUpdateEditable(!tpm_firmware_update_requested);
  }

  // Clear prefs so the reset screen isn't triggered again the next time the
  // device is about to show the login screen.
  prefs->ClearPref(prefs::kFactoryResetRequested);
  prefs->ClearPref(prefs::kFactoryResetTPMFirmwareUpdateMode);
  prefs->CommitPendingWrite();
}

void ResetScreen::HideImpl() {
  if (view_)
    view_->Hide();

  scoped_guest_button_blocker_.reset();
}

void ResetScreen::OnViewDestroyed(ResetView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void ResetScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionCancelReset)
    OnCancel();
  else if (action_id == kUserActionResetRestartPressed)
    OnRestart();
  else if (action_id == kUserActionResetPowerwashPressed)
    OnPowerwash();
  else if (action_id == kUserActionResetLearnMorePressed)
    ShowHelpArticle(HelpAppLauncher::HELP_POWERWASH);
  else if (action_id == kUserActionResetRollbackToggled)
    OnToggleRollback();
  else if (action_id == kUserActionResetShowConfirmationPressed)
    OnShowConfirm();
  else if (action_id == kUserActionResetResetConfirmationDismissed)
    OnConfirmationDismissed();
  else if (action_id == kUserActionTPMFirmwareUpdateLearnMore)
    ShowHelpArticle(HelpAppLauncher::HELP_TPM_FIRMWARE_UPDATE);
  else
    BaseScreen::OnUserAction(action_id);
}

void ResetScreen::OnCancel() {
  if (view_ && view_->GetScreenState() == ResetView::State::kRevertPromise) {
    return;
  }
  // Hide Rollback view for the next show.
  if (view_ && view_->GetIsRollbackAvailable() &&
      view_->GetIsRollbackRequested()) {
    OnToggleRollback();
  }
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
  exit_callback_.Run();
}

void ResetScreen::OnPowerwash() {
  if (view_ &&
      view_->GetScreenState() != ResetView::State::kPowerwashProposal) {
    return;
  }

  if (view_)
    view_->SetShouldShowConfirmationDialog(false);

  if (view_ && view_->GetIsRollbackRequested() &&
      !view_->GetIsRollbackAvailable()) {
    NOTREACHED()
        << "Rollback was checked but not available. Starting powerwash.";
  }

  if (view_ && view_->GetIsRollbackAvailable() &&
      view_->GetIsRollbackRequested()) {
    view_->SetScreenState(ResetView::State::kRevertPromise);
    DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(this);
    VLOG(1) << "Starting Rollback";
    DBusThreadManager::Get()->GetUpdateEngineClient()->Rollback();
  } else if (view_ && view_->GetIsTpmFirmwareUpdateChecked()) {
    VLOG(1) << "Starting TPM firmware update";
    // Re-check availability with a couple seconds timeout. This addresses the
    // case where the powerwash dialog gets shown immediately after reboot and
    // the decision on whether the update is available is not known immediately.
    tpm_firmware_update_checker_.Run(
        base::BindOnce(&StartTPMFirmwareUpdate,
                       view_->GetTpmFirmwareUpdateMode()),
        base::TimeDelta::FromSeconds(10));
  } else {
    VLOG(1) << "Starting Powerwash";
    SessionManagerClient::Get()->StartDeviceWipe();
  }
}

void ResetScreen::OnRestart() {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  if (view_ && view_->GetIsTpmFirmwareUpdateChecked()) {
    prefs->SetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode,
                      static_cast<int>(tpm_firmware_update::Mode::kPowerwash));
  } else {
    prefs->ClearPref(prefs::kFactoryResetTPMFirmwareUpdateMode);
  }
  prefs->CommitPendingWrite();

  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_FOR_USER, "login reset screen restart");
}

void ResetScreen::OnToggleRollback() {
  // Hide Rollback if visible.
  if (view_ && view_->GetIsRollbackAvailable() &&
      view_->GetIsRollbackRequested()) {
    VLOG(1) << "Hiding rollback view on reset screen";
    view_->SetIsRollbackRequested(false);
    return;
  }

  // Show Rollback if available.
  VLOG(1) << "Requested rollback availability"
          << view_->GetIsRollbackAvailable();
  if (view_->GetIsRollbackAvailable() && !view_->GetIsRollbackRequested()) {
    UMA_HISTOGRAM_ENUMERATION(
        "Reset.ChromeOS.PowerwashDialogShown",
        reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_AVAILABLE,
        reset::DIALOG_VIEW_TYPE_SIZE);
    view_->SetIsRollbackRequested(true);
  }
}

void ResetScreen::OnShowConfirm() {
  reset::DialogViewType dialog_type =
      view_->GetIsRollbackRequested()
          ? reset::DIALOG_SHORTCUT_CONFIRMING_POWERWASH_AND_ROLLBACK
          : reset::DIALOG_SHORTCUT_CONFIRMING_POWERWASH_ONLY;
  UMA_HISTOGRAM_ENUMERATION("Reset.ChromeOS.PowerwashDialogShown", dialog_type,
                            reset::DIALOG_VIEW_TYPE_SIZE);

  view_->SetShouldShowConfirmationDialog(true);
}

void ResetScreen::OnConfirmationDismissed() {
  view_->SetConfirmationDialogClosed();
}

void ResetScreen::ShowHelpArticle(HelpAppLauncher::HelpTopic topic) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  VLOG(1) << "Trying to view help article " << topic;
  if (!help_app_.get()) {
    help_app_ = new HelpAppLauncher(
        LoginDisplayHost::default_host()->GetNativeWindow());
  }
  help_app_->ShowHelpTopic(topic);
#endif
}

void ResetScreen::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  VLOG(1) << "Update status operation change to " << status.current_operation();
  if (status.current_operation() == update_engine::Operation::ERROR ||
      status.current_operation() ==
          update_engine::Operation::REPORTING_ERROR_EVENT) {
    view_->SetScreenState(ResetView::State::kError);
    // Show error screen.
    error_screen_->SetUIState(NetworkError::UI_STATE_ROLLBACK_ERROR);
    error_screen_->Show(nullptr);
  } else if (status.current_operation() ==
             update_engine::Operation::UPDATED_NEED_REBOOT) {
    PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_FOR_UPDATE, "login reset screen update");
  }
}

// Invoked from call to CanRollbackCheck upon completion of the DBus call.
void ResetScreen::OnRollbackCheck(bool can_rollback) {
  VLOG(1) << "Callback from CanRollbackCheck, result " << can_rollback;
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  const bool rollback_available =
      !connector->IsEnterpriseManaged() && can_rollback;
  reset::DialogViewType dialog_type =
      rollback_available ? reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_AVAILABLE
                         : reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_UNAVAILABLE;
  UMA_HISTOGRAM_ENUMERATION("Reset.ChromeOS.PowerwashDialogShown", dialog_type,
                            reset::DIALOG_VIEW_TYPE_SIZE);

  view_->SetIsRollbackAvailable(rollback_available);
}

void ResetScreen::OnTPMFirmwareUpdateAvailableCheck(
    const std::set<tpm_firmware_update::Mode>& modes) {
  bool available = modes.count(tpm_firmware_update::Mode::kPowerwash) > 0;
  view_->SetIsTpmFirmwareUpdateAvailable(available);
  if (available)
    view_->SetTpmFirmwareUpdateMode(tpm_firmware_update::Mode::kPowerwash);
}

}  // namespace chromeos
