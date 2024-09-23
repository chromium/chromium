// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_RESET_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_RESET_SCREEN_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "ash/public/cpp/login_accelerators.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/tpm/tpm_firmware_update.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"

class PrefRegistrySimple;

namespace ash {

class ResetView;
class ScopedGuestButtonBlocker;

// Representation independent class that controls screen showing reset to users.
// It run exit callback only if the user cancels the reset. Other user actions
// will end up in the device restart.
class ResetScreen : public BaseScreen, public UpdateEngineClient::Observer {
 public:
  enum class State {
    kRestartRequired = 0,
    kRevertPromise,
    kPowerwashProposal,
    kError,
  };

  ResetScreen(base::WeakPtr<ResetView> view,
              const base::RepeatingClosure& exit_callback);

  ResetScreen(const ResetScreen&) = delete;
  ResetScreen& operator=(const ResetScreen&) = delete;

  ~ResetScreen() override;

  // Registers Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  using TpmFirmwareUpdateAvailabilityCallback = base::OnceCallback<void(
      const std::set<tpm_firmware_update::Mode>& modes)>;
  using TpmFirmwareUpdateAvailabilityChecker = base::RepeatingCallback<void(
      TpmFirmwareUpdateAvailabilityCallback callback,
      base::TimeDelta delay)>;
  // Overrides the method used to determine TPM firmware update availability.
  // It should be called before the ResetScreen is created, otherwise it will
  // have no effect.
  static void SetTpmFirmwareUpdateCheckerForTesting(
      TpmFirmwareUpdateAvailabilityChecker* checker);

  // Checks if powerwash is allowed and passes the result to `callback`. In case
  // TPM firmware update has to be installed, the mode of update will be passed
  // as second parameter to `callback`.
  static void CheckIfPowerwashAllowed(
      base::OnceCallback<void(bool, std::optional<tpm_firmware_update::Mode>)>
          callback);

 private:
  // BaseScreen implementation:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool HandleAccelerator(LoginAcceleratorAction action) final;

  // UpdateEngineClient::Observer implementation:
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

  void SetIsRollbackAvailable(bool value);
  // Only serve the request if the confirmation dialog isn't being shown.
  void SetIsRollbackRequested(bool value);
  void SetIsTpmFirmwareUpdateChecked(bool value);
  void SetTpmFirmwareUpdateMode(tpm_firmware_update::Mode value);
  void SetShouldShowConfirmationDialog(bool value);
  void SetConfirmationDialogClosed();
  void SetScreenState(State value);

  void OnRollbackCheck(bool can_rollback);
  void OnTPMFirmwareUpdateAvailableCheck(
      const std::set<tpm_firmware_update::Mode>& modes);

  void OnCancel();
  void OnPowerwash();
  void OnRestart();
  void OnToggleRollback();

  void ShowHelpArticle(HelpAppLauncher::HelpTopic topic);

  base::WeakPtr<ResetView> view_;
  base::RepeatingClosure exit_callback_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Callback used to check whether a TPM firnware update is available.
  TpmFirmwareUpdateAvailabilityChecker tpm_firmware_update_checker_;

  std::unique_ptr<ScopedGuestButtonBlocker> scoped_guest_button_blocker_;

  State state_ = State::kRestartRequired;
  tpm_firmware_update::Mode mode_ = tpm_firmware_update::Mode::kPowerwash;
  bool is_rollback_available_ = false;
  bool is_rollback_requested_ = false;
  bool is_tpm_firmware_update_checked_ = false;
  bool is_showing_confirmation_dialog_ = false;

  base::WeakPtrFactory<ResetScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_RESET_SCREEN_H_
