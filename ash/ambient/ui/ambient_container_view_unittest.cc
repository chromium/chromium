// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

using AmbientContainerViewTest = AmbientAshTestBase;

// Tests that AmbientContainerView window should be fullscreen.
TEST_F(AmbientContainerViewTest, WindowFullscreenSize) {
  SetAmbientShownAndWaitForWidgets();
  for (const auto* container : GetContainerViews()) {
    const views::Widget* widget = container->GetWidget();

    gfx::Rect root_window_bounds =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(
                widget->GetNativeWindow()->GetRootWindow())
            .bounds();
    gfx::Rect container_window_bounds =
        widget->GetNativeWindow()->GetBoundsInScreen();
    EXPECT_EQ(root_window_bounds, container_window_bounds);
  }

  // Clean up.
  CloseAmbientScreen();
}

}  // namespace ash
