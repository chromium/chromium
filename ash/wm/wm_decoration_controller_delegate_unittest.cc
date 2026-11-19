// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/decoration/decoration.h"
#include "ui/decoration/highlight_border.h"
#include "ui/decoration/shadow.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
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

namespace {

// Records whether the decoration is visible each time the target transform of
// the window is about to change, i.e. when a transform animation starts.
class DecorationVisibilityRecorder : public aura::WindowObserver {
 public:
  DecorationVisibilityRecorder(aura::Window* window, ui::Decoration* decoration)
      : decoration_(decoration) {
    observation_.Observe(window);
  }

  // The number of times the target transform has changed.
  int transform_change_count() const { return transform_change_count_; }

  // True if the decoration was visible in any of the changes.
  bool was_decoration_visible() const { return was_decoration_visible_; }

  // aura::WindowObserver:
  void OnWindowTargetTransformChanging(
      aura::Window* window,
      const gfx::Transform& new_transform) override {
    ++transform_change_count_;
    was_decoration_visible_ |= decoration_->layer()->GetTargetVisibility();
  }
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    was_decoration_visible_ |= decoration_->layer()->GetTargetVisibility();
  }

 private:
  const raw_ptr<ui::Decoration> decoration_;
  int transform_change_count_ = 0;
  bool was_decoration_visible_ = false;
  base::ScopedObservation<aura::Window, aura::WindowObserver> observation_{
      this};
};

}  // namespace

// Tests that the decoration is hidden for the entire overview session: before
// the enter animation starts, while in overview, and until the exit animation
// ends.
TEST_F(WmDecorationControllerDelegateTest, HideDecorationDuringOverview) {
  auto* decoration_controller = Shell::Get()->decoration_controller();
  auto window = CreateWindowWithAppType(AppType::SYSTEM_APP, {200, 300});
  window->SetProperty(wm::kWindowHasHighlightBorderKey, true);
  auto* decoration =
      decoration_controller->GetDecorationForWindow(window.get());
  ASSERT_TRUE(decoration);
  EXPECT_TRUE(decoration->layer()->GetTargetVisibility());

  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  DecorationVisibilityRecorder recorder(window.get(), decoration);

  // The decoration is hidden before the enter animation starts.
  EnterOverview();
  ASSERT_TRUE(
      animator->IsAnimatingProperty(ui::LayerAnimationElement::TRANSFORM));
  const int enter_transform_change_count = recorder.transform_change_count();
  EXPECT_GT(enter_transform_change_count, 0);
  EXPECT_FALSE(recorder.was_decoration_visible());
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());
  WaitForOverviewEnterAnimation();
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());

  // The decoration stays hidden while in overview even if it is updated.
  decoration_controller->UpdateDecorationForWindow(window.get());
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());
  wm::SetShadowElevation(window.get(), wm::kShadowElevationInactiveWindow);
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());
  wm::SetShadowElevation(window.get(), wm::kShadowElevationDefault);
  window->SetProperty(wm::kWindowHasHighlightBorderKey, false);
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());
  window->SetProperty(wm::kWindowHasHighlightBorderKey, true);
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());
  EXPECT_FALSE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_FALSE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));

  // The decoration stays hidden until the exit animation ends.
  ExitOverview();
  ASSERT_TRUE(
      animator->IsAnimatingProperty(ui::LayerAnimationElement::TRANSFORM));
  EXPECT_GT(recorder.transform_change_count(), enter_transform_change_count);
  EXPECT_FALSE(recorder.was_decoration_visible());
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());
  WaitForOverviewExitAnimation();
  EXPECT_TRUE(decoration->layer()->GetTargetVisibility());
  EXPECT_TRUE(decoration_controller->IsShadowVisibleForWindow(window.get()));
  EXPECT_TRUE(
      decoration_controller->IsHighlightBorderVisibleForWindow(window.get()));
}

// Tests that the decoration of a window added during overview is hidden until
// the overview ends.
TEST_F(WmDecorationControllerDelegateTest,
       HideDecorationForWindowAddedDuringOverview) {
  auto* decoration_controller = Shell::Get()->decoration_controller();
  auto window1 = CreateWindowWithAppType(AppType::SYSTEM_APP, {200, 300});
  EnterOverview();

  // Show the new window without activating it so that overview does not end.
  auto window2 =
      CreateWindowWithAppType(AppType::SYSTEM_APP, {200, 300},
                              kShellWindowId_Invalid, /*delegate=*/nullptr,
                              /*show=*/false);
  views::Widget::GetWidgetForNativeWindow(window2.get())->ShowInactive();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_TRUE(decoration_controller->IsShadowVisibleForWindow(window2.get()));
  OverviewController::Get()->overview_session()->AddItemInMruOrder(
      window2.get(), /*reposition=*/true, /*animate=*/false,
      /*restack=*/false, /*use_spawn_animation=*/false);
  ASSERT_TRUE(GetOverviewItemForWindow(window2.get()));
  auto* decoration2 =
      decoration_controller->GetDecorationForWindow(window2.get());
  ASSERT_TRUE(decoration2);
  EXPECT_FALSE(decoration2->layer()->GetTargetVisibility());
  decoration_controller->UpdateDecorationForWindow(window2.get());
  EXPECT_FALSE(decoration2->layer()->GetTargetVisibility());
  EXPECT_FALSE(decoration_controller->IsShadowVisibleForWindow(window2.get()));

  // A transient child added during overview is hidden as well.
  auto transient =
      CreateWindowWithAppType(AppType::SYSTEM_APP, {50, 50, 100, 100},
                              kShellWindowId_Invalid, /*delegate=*/nullptr,
                              /*show=*/false);
  wm::AddTransientChild(window2.get(), transient.get());
  views::Widget::GetWidgetForNativeWindow(transient.get())->ShowInactive();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_FALSE(
      decoration_controller->IsShadowVisibleForWindow(transient.get()));
  decoration_controller->UpdateDecorationForWindow(transient.get());
  EXPECT_FALSE(
      decoration_controller->IsShadowVisibleForWindow(transient.get()));

  ExitOverview();
  EXPECT_TRUE(decoration2->layer()->GetTargetVisibility());
  EXPECT_TRUE(decoration_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_TRUE(decoration_controller->IsShadowVisibleForWindow(transient.get()));
}

// Tests that destroying a window during overview or during the exit animation
// is safe, and that the decoration of the remaining window is restored.
TEST_F(WmDecorationControllerDelegateTest, WindowDestroyedDuringOverview) {
  auto* decoration_controller = Shell::Get()->decoration_controller();
  auto window1 = CreateWindowWithAppType(AppType::SYSTEM_APP, {200, 300});
  auto window2 = CreateWindowWithAppType(AppType::SYSTEM_APP, {200, 300});
  auto window3 = CreateWindowWithAppType(AppType::SYSTEM_APP, {200, 300});
  auto* decoration3 =
      decoration_controller->GetDecorationForWindow(window3.get());
  ASSERT_TRUE(decoration3);

  EnterOverview();
  EXPECT_FALSE(decoration3->layer()->GetTargetVisibility());

  // Destroy a window while in overview.
  window1.reset();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_FALSE(decoration3->layer()->GetTargetVisibility());

  // Destroy a window while the exit animation is running.
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ExitOverview();
  ASSERT_TRUE(window2->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));
  EXPECT_FALSE(decoration3->layer()->GetTargetVisibility());
  window2.reset();
  EXPECT_FALSE(decoration3->layer()->GetTargetVisibility());

  WaitForOverviewExitAnimation();
  EXPECT_TRUE(decoration3->layer()->GetTargetVisibility());
  EXPECT_TRUE(decoration_controller->IsShadowVisibleForWindow(window3.get()));
}

// Tests that re-entering overview while the previous exit animation is still
// running keeps the decoration of re-entering windows hidden continuously,
// while restoring windows excluded from the new overview session.
TEST_F(WmDecorationControllerDelegateTest, ReenterOverviewDuringExitAnimation) {
  auto* decoration_controller = Shell::Get()->decoration_controller();
  auto window1 = CreateWindowWithAppType(AppType::SYSTEM_APP, {200, 300});
  auto window2 =
      CreateWindowWithAppType(AppType::SYSTEM_APP, {250, 0, 200, 300});
  auto* decoration1 =
      decoration_controller->GetDecorationForWindow(window1.get());
  auto* decoration2 =
      decoration_controller->GetDecorationForWindow(window2.get());
  ASSERT_TRUE(decoration1);
  ASSERT_TRUE(decoration2);

  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  EnterOverview();
  WaitForOverviewEnterAnimation();

  DecorationVisibilityRecorder recorder1(window1.get(), decoration1);
  ExitOverview();
  ASSERT_TRUE(window1->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));
  EXPECT_FALSE(decoration1->layer()->GetTargetVisibility());
  EXPECT_FALSE(decoration2->layer()->GetTargetVisibility());

  // Exclude `window2` from the next overview session before re-entering while
  // the exit animation is still running. `kHideInOverviewKey` hides `window2`
  // during overview, so force an update to verify its exit hider was released.
  window2->SetProperty(kHideInOverviewKey, true);
  EnterOverview();
  EXPECT_FALSE(recorder1.was_decoration_visible());
  EXPECT_FALSE(decoration1->layer()->GetTargetVisibility());
  EXPECT_FALSE(decoration1->fading_layer_for_testing());
  decoration_controller->UpdateDecorationForWindow(window2.get());
  EXPECT_TRUE(decoration2->layer()->GetTargetVisibility());

  WaitForOverviewEnterAnimation();
  ExitOverview();
  WaitForOverviewExitAnimation();
  EXPECT_TRUE(decoration1->layer()->GetTargetVisibility());
  EXPECT_TRUE(decoration2->layer()->GetTargetVisibility());
}

// Tests that the decoration is not copied to the old layer used in the
// cross-fade animation, and is hidden while the animation is running.
TEST_F(WmDecorationControllerDelegateTest, CrossFadeDoesNotCopyDecoration) {
  auto window = CreateWindowWithAppType(AppType::SYSTEM_APP, {200, 300});
  auto* window_state = WindowState::Get(window.get());
  auto* decoration =
      Shell::Get()->decoration_controller()->GetDecorationForWindow(
          window.get());
  ASSERT_TRUE(decoration);
  EXPECT_TRUE(decoration->layer()->GetTargetVisibility());

  auto verify = [&](base::FunctionRef<void()> change_state,
                    bool decoration_visible) {
    gfx::ScopedAnimationDurationScaleMode test_duration_mode(
        gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    ui::Layer* old_window_layer = window->layer();
    ui::Layer* decoration_layer = decoration->layer();

    change_state();

    // The window layer has been recreated for the cross-fade animation, but
    // the decoration layer stays in the new window layer and is hidden.
    ASSERT_NE(old_window_layer, window->layer());
    EXPECT_EQ(decoration_layer, decoration->layer());
    EXPECT_EQ(window->layer(), decoration_layer->parent());
    for (ui::Layer* child : old_window_layer->children()) {
      EXPECT_FALSE(child->name().starts_with("Decoration")) << child->name();
    }
    EXPECT_FALSE(decoration_layer->GetTargetVisibility());

    // The decoration of the new state is shown when the animation ends.
    window->layer()->GetAnimator()->StopAnimating();
    EXPECT_EQ(decoration_visible, decoration->layer()->GetTargetVisibility());
  };

  {
    SCOPED_TRACE("Maximize");
    verify([&] { window_state->Maximize(); }, /*decoration_visible=*/false);
  }
  {
    SCOPED_TRACE("Restore from maximized");
    verify([&] { window_state->Restore(); }, /*decoration_visible=*/true);
  }

  // Entering fullscreen is not animated.
  const WMEvent fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen_event);
  ASSERT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());
  {
    SCOPED_TRACE("Restore from fullscreen");
    verify([&] { window_state->Restore(); }, /*decoration_visible=*/true);
  }
}

namespace {

// The param indicates whether the cross-fade bounds change animation is
// enabled. A change of the origin is animated with a transform animation if it
// is enabled, and with a bounds animation otherwise.
class WmDecorationControllerDelegateBoundsAnimationTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  WmDecorationControllerDelegateBoundsAnimationTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kCrossFadeBoundsChangeAnimation, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Tests that the decoration is hidden while the transform or bounds animation
// is running.
TEST_P(WmDecorationControllerDelegateBoundsAnimationTest,
       HideDecorationDuringAnimation) {
  auto window = CreateWindowWithAppType(AppType::SYSTEM_APP, {200, 300});
  auto* decoration =
      Shell::Get()->decoration_controller()->GetDecorationForWindow(
          window.get());
  ASSERT_TRUE(decoration);
  EXPECT_TRUE(decoration->layer()->GetTargetVisibility());

  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  const SetBoundsWMEvent event({100, 100, 200, 300}, /*animate=*/true);
  WindowState::Get(window.get())->OnWMEvent(&event);
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  ASSERT_TRUE(animator->IsAnimatingProperty(
      GetParam() ? ui::LayerAnimationElement::TRANSFORM
                 : ui::LayerAnimationElement::BOUNDS));
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());

  animator->StopAnimating();
  EXPECT_TRUE(decoration->layer()->GetTargetVisibility());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WmDecorationControllerDelegateBoundsAnimationTest,
                         testing::Bool());

}  // namespace ash
