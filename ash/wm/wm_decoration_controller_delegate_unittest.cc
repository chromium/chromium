// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/decoration/highlight_border.h"
#include "ui/decoration/shadow.h"
#include "ui/views/test/test_widget_builder.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/decoration_controller.h"
#include "ui/wm/core/shadow_types.h"
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

  auto* decoration1 =
      decoration_controller->GetDecorationForWindow(window1.get());
  auto* decoration2 =
      decoration_controller->GetDecorationForWindow(window2.get());
  auto* decoration3 =
      decoration_controller->GetDecorationForWindow(window3.get());

  // window2 occludes window1
  EXPECT_FALSE(decoration1->layer()->visible());
  EXPECT_TRUE(decoration2->layer()->visible());
  EXPECT_TRUE(decoration3->layer()->visible());

  // Bring the window 1 to the front.
  wm::ActivateWindow(window1.get());

  EXPECT_TRUE(decoration1->layer()->visible());
  EXPECT_FALSE(decoration2->layer()->visible());
  EXPECT_TRUE(decoration3->layer()->visible());

  // Move window1 on top of window3.
  window1->SetBounds(kBoundsB);
  EXPECT_TRUE(decoration1->layer()->visible());
  EXPECT_TRUE(decoration2->layer()->visible());
  EXPECT_FALSE(decoration3->layer()->visible());

  // Hide window1.
  window1->Hide();
  EXPECT_TRUE(decoration2->layer()->visible());
  EXPECT_TRUE(decoration3->layer()->visible());
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

TEST_F(WmDecorationControllerDelegateTest,
       HighlightBorderVisibilityAcrossModesAndStates) {
  auto* decoration_controller = Shell::Get()->decoration_controller();
  auto window = CreateWindowWithAppType(AppType::SYSTEM_APP, {300, 200});
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  auto* window_state = WindowState::Get(window.get());

  // Highlight border is off unless opted in.
  EXPECT_TRUE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_FALSE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  window->SetProperty(wm::kWindowHasHighlightBorderKey, true);
  EXPECT_TRUE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  // Clamshell maximized: both shadow and highlight border are hidden.
  widget->Maximize();
  EXPECT_FALSE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_FALSE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  widget->Restore();
  EXPECT_TRUE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  // Clamshell split view snapped: shadow is hidden, highlight border remains
  // visible with sharp corners.
  auto other_window =
      CreateWindowWithAppType(AppType::SYSTEM_APP, {400, 100, 300, 200});
  EnterOverview();
  auto* split_view_controller = SplitViewController::Get(window.get());
  split_view_controller->SnapWindow(window.get(), SnapPosition::kPrimary);
  ASSERT_TRUE(split_view_controller->IsWindowInSplitView(window.get()));
  EXPECT_FALSE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));
  EXPECT_EQ(wm::DecorationController::GetDecorationForWindow(window.get())
                ->rounded_corners(),
            gfx::RoundedCornersF());

  ExitOverview();
  other_window.reset();
  const WMEvent restore_event(WM_EVENT_NORMAL);
  window_state->OnWMEvent(&restore_event);
  ASSERT_TRUE(window_state->IsNormalStateType());

  // Clamshell fullscreen: both shadow and highlight border are hidden.
  widget->SetFullscreen(true);
  EXPECT_FALSE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_FALSE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));
  widget->SetFullscreen(false);
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  // Clamshell pinned: both shadow and highlight border are hidden.
  const WMEvent pin_event(WM_EVENT_PIN);
  window_state->OnWMEvent(&pin_event);
  ASSERT_TRUE(window_state->IsPinned());
  EXPECT_FALSE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_FALSE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));
  window_state->OnWMEvent(&restore_event);
  ASSERT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  // Overview mode: both shadow and highlight border are hidden.
  EnterOverview();
  EXPECT_FALSE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_FALSE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));
  ExitOverview();
  EXPECT_TRUE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  // Entering tablet mode maximizes the normal window, hiding both shadow and
  // highlight border.
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(window_state->IsMaximized());
  EXPECT_FALSE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_FALSE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  // Floating in tablet mode shows both shadow and highlight border.
  const WMEvent float_event(WM_EVENT_FLOAT);
  window_state->OnWMEvent(&float_event);
  ASSERT_TRUE(window_state->IsFloated());
  EXPECT_TRUE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  // Unfloating back to maximized in tablet mode hides both again.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  EXPECT_FALSE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  // Leaving tablet mode restores the normal window state and shows both shadow
  // and highlight border.
  TabletModeControllerTestApi().LeaveTabletMode();
  ASSERT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  // PiP windows keep their highlight border in tablet mode.
  const WMEvent pip_event(WM_EVENT_PIP);
  window_state->OnWMEvent(&pip_event);
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(window_state->IsPip());
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));
}

TEST_F(WmDecorationControllerDelegateTest, HighlightBorderHiddenWhenOccluded) {
  auto* decoration_controller = Shell::Get()->decoration_controller();
  constexpr gfx::Rect kBounds(100, 100, 300, 200);

  auto window1 = CreateWindowWithAppType(AppType::SYSTEM_APP, kBounds);
  window1->SetProperty(wm::kWindowHasHighlightBorderKey, true);
  auto window2 = CreateWindowWithAppType(AppType::SYSTEM_APP, kBounds);
  window2->SetProperty(wm::kWindowHasHighlightBorderKey, true);

  // window2 occludes window1.
  EXPECT_FALSE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window1.get()));
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window2.get()));

  wm::ActivateWindow(window1.get());
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window1.get()));
  EXPECT_FALSE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window2.get()));
}

TEST_F(WmDecorationControllerDelegateTest, HighlightBorderThemeColors) {
  auto window = CreateWindowWithAppType(AppType::SYSTEM_APP, {300, 200});
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  window->SetProperty(wm::kWindowHasHighlightBorderKey, true);

  auto get_color = [&](ui::ColorId id) {
    return widget->GetColorProvider()->GetColor(id);
  };

  DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
  auto* border =
      wm::DecorationController::GetHighlightBorderForWindow(window.get());
  ASSERT_TRUE(border);
  EXPECT_EQ(get_color(ui::kColorCrosSystemHighlight),
            border->highlight_color());
  EXPECT_EQ(get_color(ui::kColorCrosSystemHighlightBorder1),
            border->border_color());

  // Switching to dark mode updates the highlight border colors.
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);
  border = wm::DecorationController::GetHighlightBorderForWindow(window.get());
  ASSERT_TRUE(border);
  EXPECT_EQ(get_color(ui::kColorCrosSystemHighlight),
            border->highlight_color());
  EXPECT_EQ(get_color(ui::kColorCrosSystemHighlightBorder1),
            border->border_color());

  // Disabling the shadow sets the shadow elevation to 0 while preserving the
  // highlight border colors.
  wm::SetShadowElevation(window.get(), wm::kShadowElevationNone);
  EXPECT_EQ(
      0,
      wm::DecorationController::GetShadowForWindow(window.get())->elevation());
  border = wm::DecorationController::GetHighlightBorderForWindow(window.get());
  ASSERT_TRUE(border);
  EXPECT_EQ(get_color(ui::kColorCrosSystemHighlight),
            border->highlight_color());
  EXPECT_EQ(get_color(ui::kColorCrosSystemHighlightBorder1),
            border->border_color());

  // Re-enabling the shadow restores the shadow with themed colors.
  wm::SetShadowElevation(window.get(), wm::kShadowElevationDefault);
  auto* shadow = wm::DecorationController::GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow);
  EXPECT_GT(shadow->elevation(), 0);
  EXPECT_EQ(wm::DecorationController::GenerateShadowColorsMap(
                widget->GetColorProvider()),
            shadow->color_map());
  border = wm::DecorationController::GetHighlightBorderForWindow(window.get());
  ASSERT_TRUE(border);
  EXPECT_EQ(get_color(ui::kColorCrosSystemHighlight),
            border->highlight_color());
  EXPECT_EQ(get_color(ui::kColorCrosSystemHighlightBorder1),
            border->border_color());
}

}  // namespace ash
