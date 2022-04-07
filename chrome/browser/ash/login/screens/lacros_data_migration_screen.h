// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_MIGRATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_MIGRATION_SCREEN_H_

#include "base/callback_forward.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/lacros_data_migration_screen_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

// A screen that shows loading spinner during user data is copied to lacros
// directory. The screen is shown during login.
class LacrosDataMigrationScreen : public BaseScreen,
                                  public PowerManagerClient::Observer {
 public:
  explicit LacrosDataMigrationScreen(LacrosDataMigrationScreenView* view);
  ~LacrosDataMigrationScreen() override;
  LacrosDataMigrationScreen(const LacrosDataMigrationScreen&) = delete;
  LacrosDataMigrationScreen& operator=(const LacrosDataMigrationScreen&) =
      delete;

  // Called when `view` gets visible.
  void OnViewVisible();

  // Called when `view` has been destroyed. If this instance is destroyed before
  // the `view` it should call view->Unbind().
  void OnViewDestroyed(LacrosDataMigrationScreenView* view);

  // Passed to `BrowserDataMigrator` as a callback to transmit the progress
  // value. `progress` is then passed to `LacrosDataMigrationView`.
  void OnProgressUpdate(int progress);

  // Posted as a delayed task from `ShowImpl()`. It calls the method of the same
  // name on `LacrosDataMigrationScreenView`.
  void ShowSkipButton();

  // Set `migrator_` for testing.
  void SetMigratorForTesting(std::unique_ptr<BrowserDataMigrator> migrator);

  // Sets `skip_post_show_button_for_testing_` for testing. Setting this to true
  // prevents `ShowSkipButton()` from being posted.
  void SetSkipPostShowButtonForTesting(bool value);

  // PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  // Sets |attempt_restart_| for testing. This helps testing as it can block
  // to restart.
  void SetAttemptRestartForTesting(
      const base::RepeatingClosure& attempt_restart);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserActionDeprecated(const std::string& action_id) override;

  // Updates the low battery message.
  void UpdateLowBatteryStatus();

  // Called when migration is completed.
  void OnMigrated(BrowserDataMigrator::Result result);

  // Called when pending local_state commit is flushed.
  void OnLocalStateCommited();

  device::mojom::WakeLock* GetWakeLock();

  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  LacrosDataMigrationScreenView* view_;
  std::unique_ptr<BrowserDataMigrator> migrator_;
  bool skip_post_show_button_for_testing_ = false;
  base::RepeatingClosure attempt_restart_;

  // PowerManagerClient::Observer is used only when screen is shown.
  base::ScopedObservation<PowerManagerClient, PowerManagerClient::Observer>
      power_manager_subscription_{this};

  base::WeakPtrFactory<LacrosDataMigrationScreen> weak_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::LacrosDataMigrationScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_MIGRATION_SCREEN_H_
