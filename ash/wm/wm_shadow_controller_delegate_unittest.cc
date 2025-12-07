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
#include "ui/views/test/test_widget_builder.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using WmShadowControllerDelegateTest = AshTestBase;

TEST_F(WmShadowControllerDelegateTest,
       UpdateShadowRoundedCornersEnterExitOverview) {
  auto window = CreateTestWindow();

  auto* window_rounded_corner =
      window->GetProperty(aura::client::kWindowRoundedCornersKey);

  auto* shadow_controller = Shell::Get()->shadow_controller();
  shadow_controller->UpdateShadowForWindow(window.get());

  // Before entering Overview, the shadow should have a same rounded corner
  // radius with its window.
  auto* shadow = shadow_controller->GetShadowForWindow(window.get());
  EXPECT_TRUE(window_rounded_corner);
  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(),
            window_rounded_corner->upper_left());

  // Enter Overview, the shadow's rounded corner radius becomes 0.
  ToggleOverview();
  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(), 0);

  // Exit Overview, the shadow's rounded corner radius is reset to window
  // rounded corner radius.
  ToggleOverview();
  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(),
            window_rounded_corner->upper_left());
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

TEST_F(WmShadowControllerDelegateTest, HideShadowForOccludedWindow) {
  auto* shadow_controller = Shell::Get()->shadow_controller();

  constexpr gfx::Rect kBoundsA(200, 300);
  constexpr gfx::Rect kBoundsB(100, 100, 400, 300);

  auto window1 = CreateAppWindow(kBoundsA);
  window1->SetName("w1");
  auto window2 = CreateAppWindow(kBoundsA);
  window2->SetName("w2");
  auto window3 = CreateAppWindow(kBoundsB);
  window3->SetName("w3");

  auto* shadow1 = shadow_controller->GetShadowForWindow(window1.get());
  auto* shadow2 = shadow_controller->GetShadowForWindow(window2.get());
  auto* shadow3 = shadow_controller->GetShadowForWindow(window3.get());

  // window2 occludes window1
  EXPECT_FALSE(shadow1->layer()->visible());
  EXPECT_TRUE(shadow2->layer()->visible());
  EXPECT_TRUE(shadow3->layer()->visible());

  // Bring the window 1 to the front.
  wm::ActivateWindow(window1.get());

  EXPECT_TRUE(shadow1->layer()->visible());
  EXPECT_FALSE(shadow2->layer()->visible());
  EXPECT_TRUE(shadow3->layer()->visible());

  // Move window1 on top of window3.
  window1->SetBounds(kBoundsB);
  EXPECT_TRUE(shadow1->layer()->visible());
  EXPECT_TRUE(shadow2->layer()->visible());
  EXPECT_FALSE(shadow3->layer()->visible());

  // Hide window1.
  window1->Hide();
  EXPECT_TRUE(shadow2->layer()->visible());
  EXPECT_TRUE(shadow3->layer()->visible());
}

TEST_F(WmShadowControllerDelegateTest, ContainerShouldHaveNoShadow) {
  auto* shadow_controller = Shell::Get()->shadow_controller();

  auto* primary_root = Shell::GetPrimaryRootWindow();

  for (int id : desks_util::GetDesksContainersIds()) {
    auto* container = Shell::GetContainer(primary_root, id);
    EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(container));
    EXPECT_FALSE(shadow_controller->IsObservingWindowForTest(container));
  }
}

TEST_F(WmShadowControllerDelegateTest, ControlShouldHaveNoShadow) {
  auto* shadow_controller = Shell::Get()->shadow_controller();
  auto window = CreateAppWindow({300, 200});

  auto child = views::test::TestWidgetBuilder()
                   .SetParent(window.get())
                   .SetWidgetType(views::Widget::InitParams::TYPE_CONTROL)
                   .BuildClientOwnsWidget();
  ASSERT_EQ(aura::client::WINDOW_TYPE_CONTROL,
            child->GetNativeWindow()->GetType());
  EXPECT_FALSE(
      shadow_controller->IsShadowVisibleForWindow(child->GetNativeWindow()));
  EXPECT_FALSE(
      shadow_controller->IsObservingWindowForTest(child->GetNativeWindow()));
}

}  // namespace ash
