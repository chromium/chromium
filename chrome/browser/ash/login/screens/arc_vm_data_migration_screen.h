// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ARC_VM_DATA_MIGRATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ARC_VM_DATA_MIGRATION_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class ArcVmDataMigrationScreen : public BaseScreen {
 public:
  explicit ArcVmDataMigrationScreen(
      base::WeakPtr<ArcVmDataMigrationScreenView> view);
  ~ArcVmDataMigrationScreen() override;
  ArcVmDataMigrationScreen(const ArcVmDataMigrationScreen&) = delete;
  ArcVmDataMigrationScreen& operator=(const ArcVmDataMigrationScreen&) = delete;

 private:
  // BaseScreen overrides:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void SetUpInitialView();

  void OnGetFreeDiskSpace(absl::optional<int64_t> reply);

  void UpdateUIState(ArcVmDataMigrationScreenView::UIState state);

  void HandleSkip();
  void HandleUpdate();

  void HandleFatalError();

  Profile* profile_;

  base::WeakPtr<ArcVmDataMigrationScreenView> view_;

  base::WeakPtrFactory<ArcVmDataMigrationScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ARC_VM_DATA_MIGRATION_SCREEN_H_
