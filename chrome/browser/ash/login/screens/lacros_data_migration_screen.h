// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_MIGRATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_MIGRATION_SCREEN_H_

#include "base/callback_forward.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/lacros_data_migration_screen_handler.h"

namespace ash {

// A screen that shows loading spinner during user data is copied to lacros
// directory. The screen is shown during login.
class LacrosDataMigrationScreen : public BaseScreen {
 public:
  explicit LacrosDataMigrationScreen(LacrosDataMigrationScreenView* view);
  ~LacrosDataMigrationScreen() override;
  LacrosDataMigrationScreen(const LacrosDataMigrationScreen&) = delete;
  LacrosDataMigrationScreen& operator=(const LacrosDataMigrationScreen&) =
      delete;

  // Called when `view` has been destroyed. If this instance is destroyed before
  // the `view` it should call view->Unbind().
  void OnViewDestroyed(LacrosDataMigrationScreenView* view);

  // Called from `LacrosDataMigratorScreenHandler::OnCancelClicked()`. It runs
  // `cancel_callback_` to cancel migration.
  void OnCancelClicked();

  // Passed to `BrowserDataMigrator` as a callback to transmit the progress
  // value. `progress` is then passed to `LacrosDataMigrationView`.
  void OnProgressUpdate(int progress);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  device::mojom::WakeLock* GetWakeLock();

  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  LacrosDataMigrationScreenView* view_;
  // Callback to cancel migration. Stores the return value from
  // `BrowserDataMigrator::Migrate()`.
  base::OnceClosure cancel_callback_;
  base::WeakPtrFactory<LacrosDataMigrationScreen> weak_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::LacrosDataMigrationScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_MIGRATION_SCREEN_H_
