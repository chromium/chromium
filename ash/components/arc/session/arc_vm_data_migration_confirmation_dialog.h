// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_VM_DATA_MIGRATION_CONFIRMATION_DIALOG_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_VM_DATA_MIGRATION_CONFIRMATION_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/window/dialog_delegate.h"

namespace arc {

using ArcVmDataMigrationConfirmationCallback = base::OnceCallback<void(bool)>;

// A dialog to ask the user to confirm ARCVM /data migration. Executes the
// passed callback with |accepted| = true/false when the OK/Cancel button is
// clicked.
class ArcVmDataMigrationConfirmationDialog : public views::DialogDelegate {
 public:
  explicit ArcVmDataMigrationConfirmationDialog(
      ArcVmDataMigrationConfirmationCallback callback);

  ArcVmDataMigrationConfirmationDialog(
      const ArcVmDataMigrationConfirmationDialog&) = delete;
  ArcVmDataMigrationConfirmationDialog& operator=(
      const ArcVmDataMigrationConfirmationDialog&) = delete;

  ~ArcVmDataMigrationConfirmationDialog() override;

 private:
  void InitializeView();
  void OnButtonClicked(bool accepted);

  ArcVmDataMigrationConfirmationCallback callback_;

  base::WeakPtrFactory<ArcVmDataMigrationConfirmationDialog> weak_ptr_factory_{
      this};
};

void ShowArcVmDataMigrationConfirmationDialog(
    ArcVmDataMigrationConfirmationCallback callback);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_VM_DATA_MIGRATION_CONFIRMATION_DIALOG_H_
