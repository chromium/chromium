// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_BACKWARD_MIGRATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_BACKWARD_MIGRATION_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class LacrosDataBackwardMigrationScreenView;

// A screen that shows loading spinner during user data is copied to lacros
// directory. The screen is shown during login.
class LacrosDataBackwardMigrationScreen : public BaseScreen {
 public:
  explicit LacrosDataBackwardMigrationScreen(
      base::WeakPtr<LacrosDataBackwardMigrationScreenView> view);
  ~LacrosDataBackwardMigrationScreen() override;
  LacrosDataBackwardMigrationScreen(const LacrosDataBackwardMigrationScreen&) =
      delete;
  LacrosDataBackwardMigrationScreen& operator=(
      const LacrosDataBackwardMigrationScreen&) = delete;

  // Set `migrator_for_testing_`.
  static void SetMigratorForTesting(BrowserDataBackMigratorBase* migrator);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // Updates progress during migration.
  void OnProgress(int percent);

  // Called when migration is completed.
  void OnMigrated(BrowserDataBackMigratorBase::Result result);

  // Called when migration is canceled by the user.
  void OnCanceled();

  base::WeakPtr<LacrosDataBackwardMigrationScreenView> view_;
  std::unique_ptr<BrowserDataBackMigratorBase> migrator_;

  static BrowserDataBackMigratorBase* migrator_for_testing_;

  base::WeakPtrFactory<LacrosDataBackwardMigrationScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_LACROS_DATA_BACKWARD_MIGRATION_SCREEN_H_
