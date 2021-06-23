// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_MIGRATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_MIGRATION_SCREEN_H_

#include "chrome/browser/ash/login/screens/base_screen.h"
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

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  LacrosDataMigrationScreenView* view_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::LacrosDataMigrationScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_MIGRATION_SCREEN_H_
