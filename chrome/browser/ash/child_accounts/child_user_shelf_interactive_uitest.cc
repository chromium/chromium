// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "base/test/gtest_tags.h"
#include "chrome/browser/ash/child_accounts/child_user_interactive_base_test.h"
#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "ui/aura/env.h"

namespace ash {

// Interactive test that uses a supervised user mixin to test shelf behavior for
// child users.
using ChildUserShelfInteractiveTest = ChildUserInteractiveBaseTest;

IN_PROC_BROWSER_TEST_F(ChildUserShelfInteractiveTest, CheckIncognitoDisabled) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-5e1a3142-c3a8-4367-ba99-292837fe8185");

  // Get the primary display's shelf view.
  Shelf* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  ASSERT_TRUE(shelf);
  ShelfView* shelf_view = shelf->GetShelfViewForTesting();
  ASSERT_TRUE(shelf_view);
  ShelfViewTestAPI test_api(shelf_view);

  ShelfAppButton* chrome_button = test_api.GetButton(0);
  ASSERT_TRUE(chrome_button);

  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(Log("Check that Incognito Window item is not available"),
                  MoveMouseTo(chrome_button->GetBoundsInScreen().CenterPoint()),
                  ClickMouse(ui_controls::RIGHT),
                  EnsureNotPresent(kShelfContextMenuIncognitoWindowMenuItem),
                  EnsurePresent(kShelfContextMenuNewWindowMenuItem),
                  Log("Test completed"));
}

}  // namespace ash
