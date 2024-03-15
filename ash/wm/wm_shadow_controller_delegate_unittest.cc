// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_controller.h"

namespace ash {

using WmShadowControllerDelegateTest = AshTestBase;

TEST_F(WmShadowControllerDelegateTest,
       UpdateShadowRoundedCornersBasedOnWindowState) {
  auto window = CreateTestWindow();

  const int window_corner_radius =
      window->GetProperty(aura::client::kWindowCornerRadiusKey);

  auto* shadow_controller = Shell::Get()->shadow_controller();
  shadow_controller->UpdateShadowForWindow(window.get());

  // For normal window state, top-level windows have rounded window.
  WindowState* window_state = WindowState::Get(window.get());

  auto* shadow = shadow_controller->GetShadowForWindow(window.get());
  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(), window_corner_radius);

  // Window in snapped state does not have rounded corners, therefore the window
  // shadow should adjust accordingly.
  const WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_event);

  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(), 0);

  window_state->Restore();

  ASSERT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(), window_corner_radius);
}

TEST_F(WmShadowControllerDelegateTest,
       AlwaysOnTopWindowsDontHaveShadowsInOverview) {
  auto window = CreateAppWindow(gfx::Rect(200, 300));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);

  // The window is on the always on-top-container and has shadow.
  EXPECT_TRUE(Shell::GetPrimaryRootWindow()
                  ->GetChildById(kShellWindowId_AlwaysOnTopContainer)
                  ->Contains(window.get()));
  auto* shadow_controller = Shell::Get()->shadow_controller();
  shadow_controller->UpdateShadowForWindow(window.get());
  auto* shadow = shadow_controller->GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow);
  EXPECT_TRUE(shadow->layer()->visible());

  EnterOverview();
  EXPECT_FALSE(shadow->layer()->visible());

  ExitOverview();
  EXPECT_TRUE(shadow->layer()->visible());
}

}  // namespace ash
