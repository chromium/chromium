// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_SCREEN_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chromeos/dbus/update_engine_client.h"

class PrefRegistrySimple;

namespace ash {
class ScopedGuestButtonBlocker;
}

namespace chromeos {

class ErrorScreen;
class ResetView;
class ScopedGuestButtonBlocker;

// Representation independent class that controls screen showing reset to users.
// It run exit callback only if the user cancels the reset. Other user actions
// will end up in the device restart.
class ResetScreen : public BaseScreen, public UpdateEngineClient::Observer {
 public:
  ResetScreen(ResetView* view,
              ErrorScreen* error_screen,
              const base::RepeatingClosure& exit_callback);
  ~ResetScreen() override;

  // Called when view is destroyed so there's no dead reference to it.
  void OnViewDestroyed(ResetView* view);

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

  // Checks if powerwash is allowed and passes the result to |callback|. In case
  // TPM firmware update has to be installed, the mode of update will be passed
  // as second parameter to |callback|.
  static void CheckIfPowerwashAllowed(
      base::OnceCallback<void(bool, base::Optional<tpm_firmware_update::Mode>)>
          callback);

 private:
  // BaseScreen implementation:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  // UpdateEngineClient::Observer implementation:
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

  void OnRollbackCheck(bool can_rollback);
  void OnTPMFirmwareUpdateAvailableCheck(
      const std::set<tpm_firmware_update::Mode>& modes);

  void OnCancel();
  void OnPowerwash();
  void OnRestart();
  void OnToggleRollback();
  void OnShowConfirm();
  void OnConfirmationDismissed();

  void ShowHelpArticle(HelpAppLauncher::HelpTopic topic);

  ResetView* view_;
  ErrorScreen* error_screen_;
  base::RepeatingClosure exit_callback_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Callback used to check whether a TPM firnware update is available.
  TpmFirmwareUpdateAvailabilityChecker tpm_firmware_update_checker_;

  std::unique_ptr<ash::ScopedGuestButtonBlocker> scoped_guest_button_blocker_;

  base::WeakPtrFactory<ResetScreen> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ResetScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_SCREEN_H_
