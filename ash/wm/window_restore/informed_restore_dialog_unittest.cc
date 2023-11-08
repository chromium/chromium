// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_dialog.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class InformedRestoreDialogTest : public AshTestBase {
 public:
  InformedRestoreDialogTest() = default;
  InformedRestoreDialogTest(const InformedRestoreDialogTest&) = delete;
  InformedRestoreDialogTest& operator=(const InformedRestoreDialogTest&) =
      delete;
  ~InformedRestoreDialogTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kPostLoginGlanceables};
};

TEST_F(InformedRestoreDialogTest, Show) {
  Shell::Get()->window_restore_controller()->MaybeStartInformedRestore();
}

}  // namespace ash
