// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using WindowParentingControllerTest = AshTestBase;

// Verifies a window with a transient parent is in the same container as its
// transient parent.
TEST_F(WindowParentingControllerTest, TransientParent) {
  // Normal window.
  auto window = CreateAppWindow();

  // Move the window to a container that isn't a default container.
  aura::Window* shelf = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                            kShellWindowId_ShelfContainer);
  shelf->AddChild(window.get());

  // Transient.
  std::unique_ptr<views::Widget> transient_widget =
      TestWidgetBuilder()
          .SetShow(true)
          .SetParent(window.get())
          .BuildOwnsNativeWidget();
  ASSERT_TRUE(wm::HasTransientAncestor(transient_widget->GetNativeWindow(),
                                       window.get()));
  EXPECT_EQ(shelf, transient_widget->GetNativeWindow()->parent());
}

}  // namespace ash
