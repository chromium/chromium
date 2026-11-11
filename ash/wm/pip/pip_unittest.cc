// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/pip/pip_controller.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/pip/pip_test_utils.h"
#include "ash/wm/pip/pip_window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class PipTest : public AshTestBase {
 public:
  PipTest() = default;

  PipTest(const PipTest&) = delete;
  PipTest& operator=(const PipTest&) = delete;

  ~PipTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
  }

  void TearDown() override { AshTestBase::TearDown(); }

  static std::unique_ptr<views::Widget> CreateWidget(aura::Window* context) {
    std::unique_ptr<views::Widget> widget(new views::Widget);
    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    params.delegate = new views::WidgetDelegateView(
        views::WidgetDelegateView::CreatePassKey());
    params.context = context;
    widget->Init(std::move(params));
    return widget;
  }
};

TEST_F(PipTest, ShowInactive) {
  auto widget = CreateWidget(Shell::GetPrimaryRootWindow());
  const WMEvent pip_event(WM_EVENT_PIP);
  auto* window_state = WindowState::Get(widget->GetNativeWindow());
  window_state->OnWMEvent(&pip_event);
  ASSERT_TRUE(window_state->IsPip());
  ASSERT_FALSE(widget->IsVisible());
  widget->Show();
  ASSERT_TRUE(widget->IsVisible());
  EXPECT_FALSE(widget->IsActive());

  widget->Activate();
  EXPECT_FALSE(widget->IsActive());

  const WMEvent normal_event(WM_EVENT_NORMAL);
  window_state->OnWMEvent(&normal_event);
  EXPECT_FALSE(window_state->IsPip());
  EXPECT_FALSE(widget->IsActive());

  widget->Activate();
  EXPECT_TRUE(widget->IsActive());

  window_state->OnWMEvent(&pip_event);
  EXPECT_FALSE(widget->IsActive());
}

TEST_F(PipTest, ActivatablePipWindow) {
  auto widget = CreateWidget(Shell::GetPrimaryRootWindow());
  widget->GetNativeWindow()->SetProperty(kPipParamsKey,
                                         PipParams{.activatable = true});
  auto* window_state = WindowState::Get(widget->GetNativeWindow());
  const WMEvent pip_event(WM_EVENT_PIP);
  window_state->OnWMEvent(&pip_event);
  ASSERT_TRUE(window_state->IsPip());
  widget->Show();

  widget->Activate();
  EXPECT_TRUE(widget->IsActive());

  // The window stays activatable after losing activation.
  auto other_widget = CreateWidget(Shell::GetPrimaryRootWindow());
  other_widget->Show();
  other_widget->Activate();
  ASSERT_FALSE(widget->IsActive());
  widget->Activate();
  EXPECT_TRUE(widget->IsActive());
}

// Like other windows, the PiP window receives events just outside of its
// bounds so that it can be resized from there.
TEST_F(PipTest, PipWindowUsesExtendedHitRegion) {
  ForceHideShelvesForTest();
  UpdateDisplay("500x400");
  aura::test::EventCountDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShell(
      {.delegate = &delegate, .bounds = {200, 150, 100, 100}}));
  // A PiP window is always on top, which places it in the PiP container.
  window->SetProperty(aura::client::kZOrderingKey,
                      ui::ZOrderLevel::kFloatingWindow);
  const WMEvent enter_pip(WM_EVENT_PIP);
  WindowState::Get(window.get())->OnWMEvent(&enter_pip);
  ASSERT_EQ(kShellWindowId_PipContainer, window->parent()->GetId());

  const gfx::Rect bounds = window->GetBoundsInScreen();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      bounds.right() + chromeos::kResizeOutsideBoundsSize - 1,
      bounds.CenterPoint().y());
  EXPECT_EQ("1 1 0", delegate.GetMouseMotionCountsAndReset());

  // Too far from the window.
  generator->MoveMouseTo(
      bounds.right() + chromeos::kResizeOutsideBoundsSize + 10,
      bounds.CenterPoint().y());
  EXPECT_EQ("0 0 1", delegate.GetMouseMotionCountsAndReset());
}

TEST_F(PipTest, ShortcutNavigation) {
  auto widget = CreateWidget(Shell::GetPrimaryRootWindow());
  auto pip_widget = CreateWidget(Shell::GetPrimaryRootWindow());
  widget->Show();
  pip_widget->Show();
  const WMEvent pip_event(WM_EVENT_PIP);
  auto* pip_window_state = WindowState::Get(pip_widget->GetNativeWindow());
  pip_window_state->OnWMEvent(&pip_event);
  EXPECT_TRUE(pip_window_state->IsPip());
  EXPECT_FALSE(pip_widget->IsActive());
  ASSERT_TRUE(widget->IsActive());

  auto* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_V, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_TRUE(pip_widget->IsActive());
  EXPECT_FALSE(widget->IsActive());

  auto* navigation_widget = AshTestBase::GetPrimaryShelf()->navigation_widget();
  auto* hotseat_widget = AshTestBase::GetPrimaryShelf()->hotseat_widget();
  auto* status_area =
      Shell::GetPrimaryRootWindowController()->GetStatusAreaWidget();

  // Cycle Backward.
  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(status_area->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(hotseat_widget->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(navigation_widget->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(pip_widget->IsActive());

  // Forward
  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(navigation_widget->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(hotseat_widget->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(status_area->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(pip_widget->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget->IsActive());
}

TEST_F(PipTest, PipInitialPositionAvoidsObstacles) {
  UpdateDisplay("500x400");
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell({.bounds = {100, 300, 100, 100}}));
  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->Show();

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(/*lock=*/true);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 300, 400, 100));

  // Expect the PIP position is shifted below the keyboard.
  EXPECT_TRUE(window_state->IsPip());
  EXPECT_TRUE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(100, 192, 100, 100), window->layer()->GetTargetBounds());
}

TEST_F(PipTest, TargetBoundsAffectedByWorkAreaChange) {
  UpdateDisplay("500x400");

  // Place a keyboard window at the initial position of a PIP window.
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(/*lock=*/true);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 300, 400, 100));

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell({.bounds = {100, 300, 100, 100}}));
  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->Show();

  // Ensure the initial PIP position is shifted below the keyboard.
  EXPECT_TRUE(window_state->IsPip());
  EXPECT_TRUE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(100, 192, 100, 100), window->bounds());
}

TEST_F(PipTest, PipRestoresToPreviousBoundsOnMovementAreaChangeIfTheyExist) {
  ForceHideShelvesForTest();
  UpdateDisplay("500x400");
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell({.bounds = {200, 200, 100, 100}}));
  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->Show();

  // Position the PIP window on the side of the screen where it will be next
  // to an edge and therefore in a resting position for the whole test.
  const gfx::Rect bounds = gfx::Rect(392, 200, 100, 100);
  // Set restore position to where the window currently is.
  window->SetBounds(bounds);
  PipPositioner::SaveSnapFraction(window_state, window->GetBoundsInScreen());
  EXPECT_TRUE(PipPositioner::HasSnapFraction(window_state));

  // Update the work area so that the PIP window should be pushed upward.
  UpdateDisplay("400x200");
  ForceHideShelvesForTest();

  // PIP should move up to accommodate the new work area.
  EXPECT_EQ(gfx::Rect(292, 76, 100, 100), window->GetBoundsInScreen());

  // Restore the original work area.
  UpdateDisplay("500x400");
  ForceHideShelvesForTest();

  // Changing the work area with the same PIP size causes snap fraction change,
  // so PIP doesn't restore to the original position. Instead ensure that the
  // fraction is calculated correctly.
  EXPECT_EQ(gfx::Rect(392, 239, 100, 100), window->GetBoundsInScreen());
}

TEST_F(
    PipTest,
    PipRestoresToPreviousBoundsOnMovementAreaChangeIfTheyExistOnExternalDisplay) {
  UpdateDisplay("500x400,500x400");
  ForceHideShelvesForTest();
  auto* root_window = Shell::GetAllRootWindows()[1].get();

  // Position the PIP window on the side of the screen where it will be next
  // to an edge and therefore in a resting position for the whole test.
  auto widget = CreateWidget(root_window);
  auto* window = widget->GetNativeWindow();
  WindowState* window_state = WindowState::Get(window);
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->Show();
  window->SetBounds(gfx::Rect(8, 292, 100, 100));

  // Set restore position to where the window currently is.
  PipPositioner::SaveSnapFraction(window_state, window->GetBoundsInScreen());
  EXPECT_TRUE(PipPositioner::HasSnapFraction(window_state));

  // Update the work area so that the PIP window should be pushed upward.
  UpdateDisplay("500x400,400x200");
  ForceHideShelvesForTest();

  // PIP should move up to accommodate the new work area.
  EXPECT_EQ(gfx::Rect(508, 92, 100, 100), window->GetBoundsInScreen());

  // Restore the original work area.
  UpdateDisplay("500x400,500x400");
  ForceHideShelvesForTest();

  // Changing the work area with the same PIP size causes snap fraction change,
  // so PIP doesn't restore to the original position. Instead ensure that the
  // fraction is calculated correctly.
  EXPECT_EQ(gfx::Rect(508, 292, 100, 100), window->GetBoundsInScreen());
}

TEST_F(PipTest, PipRestoreOnWorkAreaChangeDoesNotChangeWindowSize) {
  ForceHideShelvesForTest();
  UpdateDisplay("500x400");
  // Create a new PiP window using TestWindowBuilder().
  // Set SetShow to false upon creation to simulate the window being created
  // as a PiP rather than being changed to PiP.
  // Position the PIP window on the side of the screen where it will be next
  // to an edge and therefore in a resting position for the whole test.
  std::unique_ptr<aura::Window> pip_window(TestWindowBuilder()
                                               .AllowAllWindowStates()
                                               .SetShow(false)
                                               .Build()
                                               .release());
  WindowState* window_state = WindowState::Get(pip_window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  pip_window->SetBounds(gfx::Rect(392, 200, 100, 100));
  EXPECT_TRUE(window_state->IsPip());
  pip_window->Show();

  // Update the work area so that the PIP window should be pushed upward.
  UpdateDisplay("400x200");
  ForceHideShelvesForTest();

  // The PIP snap position should be applied and the relative position
  // along the edge shouldn't change.
  EXPECT_EQ(gfx::Rect(292, 76, 100, 100), pip_window->GetBoundsInScreen());
}

TEST_F(PipTest, PipSnappedToEdgeWhenSavingSnapFraction) {
  ForceHideShelvesForTest();
  UpdateDisplay("500x400");
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell({.bounds = {200, 200, 100, 100}}));
  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->Show();

  // Show the floating keyboard and make the PIP window detached from the screen
  // edges.
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboardInDisplay(window_state->GetDisplay());
  ASSERT_TRUE(keyboard::test::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 300, 400, 100));

  window->SetBounds(gfx::Rect(100, 192, 100, 100));

  // Set restore position to where the window currently is.
  PipPositioner::SaveSnapFraction(window_state, window->GetBoundsInScreen());
  EXPECT_TRUE(PipPositioner::HasSnapFraction(window_state));

  // Ensure that the correct value is saved as snap fraction even when the PIP
  // bounds is detached from the screen edge.
  EXPECT_EQ(gfx::Rect(100, 192, 100, 100),
            PipPositioner::GetSnapFractionAppliedBounds(window_state));
}

// Tests for the PIP auto layout mode toggle (`PipParams::auto_layout`).
class PipAutoLayoutTest : public PipTest {
 public:
  PipAutoLayoutTest() = default;
  PipAutoLayoutTest(const PipAutoLayoutTest&) = delete;
  PipAutoLayoutTest& operator=(const PipAutoLayoutTest&) = delete;
  ~PipAutoLayoutTest() override = default;

  void SetUp() override {
    PipTest::SetUp();
    ForceHideShelvesForTest();
    UpdateDisplay("500x400");
  }

 protected:
  std::unique_ptr<aura::Window> CreatePipWindow(const gfx::Rect& bounds,
                                                bool auto_layout) {
    std::unique_ptr<aura::Window> window =
        CreateTestWindowInShell({.delegate = &delegate_, .bounds = bounds});
    if (!auto_layout) {
      window->SetProperty(kPipParamsKey, PipParams{.auto_layout = false});
    }
    const WMEvent enter_pip(WM_EVENT_PIP);
    WindowState::Get(window.get())->OnWMEvent(&enter_pip);
    window->Show();
    return window;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kPipAutoLayoutToggle};
  aura::test::TestWindowDelegate delegate_;
};

class PipAutoLayoutToggleDisabledTest : public PipTest {
 public:
  PipAutoLayoutToggleDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kPipAutoLayoutToggle);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PipAutoLayoutToggleDisabledTest, AutoLayoutPropertyIsIgnored) {
  ForceHideShelvesForTest();
  UpdateDisplay("500x400");
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell({.bounds = {100, 150, 100, 100}}));
  window->SetProperty(kPipParamsKey, PipParams{.auto_layout = false});
  const WMEvent enter_pip(WM_EVENT_PIP);
  WindowState::Get(window.get())->OnWMEvent(&enter_pip);

  // The window snaps to the closest (left) edge.
  EXPECT_EQ(gfx::Rect(8, 150, 100, 100), window->bounds());
}

TEST_F(PipAutoLayoutTest, AutoLayoutOffDoesNotSnapWhenEnteringPip) {
  std::unique_ptr<aura::Window> window =
      CreatePipWindow({100, 150, 100, 100}, /*auto_layout=*/false);
  WindowState* window_state = WindowState::Get(window.get());
  ASSERT_TRUE(window_state->IsPip());

  EXPECT_EQ(gfx::Rect(100, 150, 100, 100), window->bounds());
  EXPECT_FALSE(PipPositioner::HasSnapFraction(window_state));
}

TEST_F(PipAutoLayoutTest, AutoLayoutOffDoesNotMoveOnMovementAreaChange) {
  std::unique_ptr<aura::Window> window =
      CreatePipWindow({100, 150, 100, 100}, /*auto_layout=*/false);

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(/*lock=*/true);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());
  keyboard_controller->GetKeyboardWindow()->SetBounds({0, 200, 500, 200});

  // The window is not pushed into the movement area or snapped to the edge.
  EXPECT_EQ(gfx::Rect(100, 150, 100, 100), window->bounds());
}

TEST_F(PipAutoLayoutTest, AutoLayoutOffDoesNotClampOrSnapOnSizeChange) {
  std::unique_ptr<aura::Window> window =
      CreatePipWindow({100, 150, 100, 100}, /*auto_layout=*/false);

  window->SetBounds({100, 150, 200, 150});
  EXPECT_EQ(gfx::Rect(100, 150, 200, 150), window->bounds());

  // Bounds extending outside the movement area are not clamped or snapped.
  window->SetBounds({300, 150, 250, 150});
  EXPECT_EQ(gfx::Rect(300, 150, 250, 150), window->bounds());
}

TEST_F(PipAutoLayoutTest, TurningAutoLayoutOffKeepsBounds) {
  std::unique_ptr<aura::Window> window =
      CreatePipWindow({100, 150, 100, 100}, /*auto_layout=*/true);
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_EQ(gfx::Rect(8, 150, 100, 100), window->bounds());
  PipPositioner::SaveSnapFraction(window_state, window->GetBoundsInScreen());

  window->SetProperty(kPipParamsKey, PipParams{.auto_layout = false});

  EXPECT_EQ(gfx::Rect(8, 150, 100, 100), window->bounds());
  EXPECT_FALSE(PipPositioner::HasSnapFraction(window_state));
}

TEST_F(PipAutoLayoutTest, TurningAutoLayoutOffUntucksWindow) {
  auto widget = CreateWidget(Shell::GetPrimaryRootWindow());
  aura::Window* window = widget->GetNativeWindow();
  window->SetBounds({100, 150, 100, 100});
  const WMEvent enter_pip(WM_EVENT_PIP);
  WindowState::Get(window)->OnWMEvent(&enter_pip);
  widget->Show();

  auto* pip_controller = Shell::Get()->pip_controller();
  pip_controller->TuckWindow(/*left=*/true);
  ASSERT_TRUE(pip_controller->is_tucked());
  ASSERT_LT(window->bounds().x(), 0);

  window->SetProperty(kPipParamsKey, PipParams{.auto_layout = false});

  EXPECT_FALSE(pip_controller->is_tucked());
  EXPECT_EQ(gfx::Rect(8, 150, 100, 100), window->bounds());
}

TEST_F(PipAutoLayoutTest, TurningAutoLayoutOnAnimatesToRestingPosition) {
  std::unique_ptr<aura::Window> window =
      CreatePipWindow({100, 150, 100, 100}, /*auto_layout=*/false);
  WindowState* window_state = WindowState::Get(window.get());
  ASSERT_EQ(gfx::Rect(100, 150, 100, 100), window->bounds());

  gfx::ScopedAnimationDurationScaleMode animation_duration(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  window->SetProperty(kPipParamsKey, PipParams{.auto_layout = true});

  // The window snaps to the closest (left) edge with a position animation.
  EXPECT_EQ(gfx::Rect(8, 150, 100, 100), window->bounds());
  EXPECT_TRUE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));
  EXPECT_TRUE(PipPositioner::HasSnapFraction(window_state));
}

}  // namespace ash
