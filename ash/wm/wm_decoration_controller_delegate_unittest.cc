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
#include "ui/decoration/shadow.h"
#include "ui/views/test/test_widget_builder.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/decoration_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using chromeos::AppType;

using WmDecorationControllerDelegateTest = AshTestBase;

TEST_F(WmDecorationControllerDelegateTest,
       UpdateShadowRoundedCornersEnterExitOverview) {
  auto window = CreateWindowWithAppType();

  auto* window_rounded_corner =
      window->GetProperty(aura::client::kWindowRoundedCornersKey);

  auto* decoration_controller = Shell::Get()->decoration_controller();
  decoration_controller->UpdateDecorationForWindow(window.get());

  // Before entering Overview, the shadow should have a same rounded corner
  // radius with its window.
  auto* decoration =
      decoration_controller->GetDecorationForWindow(window.get());
  EXPECT_TRUE(window_rounded_corner);
  EXPECT_EQ(decoration->rounded_corners(), *window_rounded_corner);

  // Enter Overview, the shadow's rounded corner radius becomes 0.
  ToggleOverview();
  EXPECT_EQ(decoration->rounded_corners(), gfx::RoundedCornersF());

  // Exit Overview, the shadow's rounded corner radius is reset to window
  // rounded corner radius.
  ToggleOverview();
  EXPECT_EQ(decoration->rounded_corners(), *window_rounded_corner);
}

TEST_F(WmDecorationControllerDelegateTest,
       AlwaysOnTopWindowsDontHaveShadowsInOverview) {
  auto window = CreateWindowWithAppType(AppType::SYSTEM_APP, {200, 300});
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);

  // The window is on the always on-top-container and has shadow.
  EXPECT_TRUE(Shell::GetPrimaryRootWindow()
                  ->GetChildById(kShellWindowId_AlwaysOnTopContainer)
                  ->Contains(window.get()));
  auto* decoration_controller = Shell::Get()->decoration_controller();
  decoration_controller->UpdateDecorationForWindow(window.get());
  auto* decoration =
      decoration_controller->GetDecorationForWindow(window.get());
  ASSERT_TRUE(decoration);
  EXPECT_TRUE(decoration->layer()->visible());

  EnterOverview();
  EXPECT_FALSE(decoration->layer()->visible());

  ExitOverview();
  EXPECT_TRUE(decoration->layer()->visible());
}

TEST_F(WmDecorationControllerDelegateTest, HideShadowForOccludedWindow) {
  auto* decoration_controller = Shell::Get()->decoration_controller();

  constexpr gfx::Rect kBoundsA(200, 300);
  constexpr gfx::Rect kBoundsB(100, 100, 400, 300);

  auto window1 = CreateWindowWithAppType(AppType::SYSTEM_APP, kBoundsA);
  window1->SetName("w1");
  auto window2 = CreateWindowWithAppType(AppType::SYSTEM_APP, kBoundsA);
  window2->SetName("w2");
  auto window3 = CreateWindowWithAppType(AppType::SYSTEM_APP, kBoundsB);
  window3->SetName("w3");

  auto* shadow_decoration1 =
      decoration_controller->GetDecorationForWindow(window1.get());
  auto* shadow_decoration2 =
      decoration_controller->GetDecorationForWindow(window2.get());
  auto* shadow_decoration3 =
      decoration_controller->GetDecorationForWindow(window3.get());

  // window2 occludes window1
  EXPECT_FALSE(shadow_decoration1->layer()->visible());
  EXPECT_TRUE(shadow_decoration2->layer()->visible());
  EXPECT_TRUE(shadow_decoration3->layer()->visible());

  // Bring the window 1 to the front.
  wm::ActivateWindow(window1.get());

  EXPECT_TRUE(shadow_decoration1->layer()->visible());
  EXPECT_FALSE(shadow_decoration2->layer()->visible());
  EXPECT_TRUE(shadow_decoration3->layer()->visible());

  // Move window1 on top of window3.
  window1->SetBounds(kBoundsB);
  EXPECT_TRUE(shadow_decoration1->layer()->visible());
  EXPECT_TRUE(shadow_decoration2->layer()->visible());
  EXPECT_FALSE(shadow_decoration3->layer()->visible());

  // Hide window1.
  window1->Hide();
  EXPECT_TRUE(shadow_decoration2->layer()->visible());
  EXPECT_TRUE(shadow_decoration3->layer()->visible());
}

TEST_F(WmDecorationControllerDelegateTest, ContainerShouldHaveNoShadow) {
  auto* decoration_controller = Shell::Get()->decoration_controller();

  auto* primary_root = Shell::GetPrimaryRootWindow();

  for (int id : desks_util::GetDesksContainersIds()) {
    auto* container = Shell::GetContainer(primary_root, id);
    EXPECT_FALSE(decoration_controller->IsShadowVisibleForWindow(container));
    EXPECT_FALSE(decoration_controller->IsObservingWindowForTest(container));
  }
}

TEST_F(WmDecorationControllerDelegateTest, ControlShouldHaveNoShadow) {
  auto* decoration_controller = Shell::Get()->decoration_controller();
  auto window = CreateWindowWithAppType(AppType::SYSTEM_APP, {300, 200});

  auto child = views::test::TestWidgetBuilder()
                   .SetParent(window.get())
                   .SetWidgetType(views::Widget::InitParams::TYPE_CONTROL)
                   .BuildClientOwnsWidget();
  ASSERT_EQ(aura::client::WINDOW_TYPE_CONTROL,
            child->GetNativeWindow()->GetType());
  EXPECT_FALSE(decoration_controller->IsShadowVisibleForWindow(
      child->GetNativeWindow()));
  EXPECT_FALSE(decoration_controller->IsObservingWindowForTest(
      child->GetNativeWindow()));
}

}  // namespace ash
