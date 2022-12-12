// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_vm_data_migration_confirmation_dialog.h"

#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/window/dialog_delegate.h"

namespace arc {

namespace {

class ArcVmDataMigrationConfirmationDialogTest : public ash::AshTestBase {
 public:
  ArcVmDataMigrationConfirmationDialogTest() = default;
  ArcVmDataMigrationConfirmationDialogTest(
      const ArcVmDataMigrationConfirmationDialogTest&) = delete;
  ArcVmDataMigrationConfirmationDialogTest& operator=(
      const ArcVmDataMigrationConfirmationDialogTest&) = delete;
  ~ArcVmDataMigrationConfirmationDialogTest() override = default;
};

TEST_F(ArcVmDataMigrationConfirmationDialogTest, Show) {
  ShowArcVmDataMigrationConfirmationDialog(base::DoNothing());
}

TEST_F(ArcVmDataMigrationConfirmationDialogTest, Accept) {
  int accept_count = 0;
  int cancel_count = 0;
  std::unique_ptr<ArcVmDataMigrationConfirmationDialog> dialog =
      std::make_unique<ArcVmDataMigrationConfirmationDialog>(
          base::BindLambdaForTesting([&](bool accepted) {
            if (accepted)
              accept_count++;
            else
              cancel_count++;
          }));
  dialog->Accept();
  EXPECT_EQ(1, accept_count);
  EXPECT_EQ(0, cancel_count);
}

TEST_F(ArcVmDataMigrationConfirmationDialogTest, Cancel) {
  int accept_count = 0;
  int cancel_count = 0;
  std::unique_ptr<ArcVmDataMigrationConfirmationDialog> dialog =
      std::make_unique<ArcVmDataMigrationConfirmationDialog>(
          base::BindLambdaForTesting([&](bool accepted) {
            if (accepted)
              accept_count++;
            else
              cancel_count++;
          }));
  dialog->Cancel();
  EXPECT_EQ(0, accept_count);
  EXPECT_EQ(1, cancel_count);
}

}  // namespace

}  // namespace arc
