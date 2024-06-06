// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overlay_layout_manager.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using OverlayLayoutManagerTest = AshTestBase;

// Verifies that a fullscreen widget in the overlay container has its bounds
// updated when the display rotates. https://crbug.com/869130
TEST_F(OverlayLayoutManagerTest, FullscreenWidgetWithDisplayRotation) {
  UpdateDisplay("800x600");

  // Create a fullscreen widget in the overlay container.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       nullptr, kShellWindowId_OverlayContainer);
  widget->SetFullscreen(true);
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), widget->GetWindowBoundsInScreen());

  // Widget bounds update after display rotation.
  UpdateDisplay("800x600/r");
  EXPECT_EQ(gfx::Rect(0, 0, 600, 800), widget->GetWindowBoundsInScreen());
}

}  // namespace
}  // namespace ash
