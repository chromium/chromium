// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"

#include <string>
#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/pip/pip_test_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

std::unique_ptr<views::Widget> CreateWidget(aura::Window* context) {
  std::unique_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  params.delegate = new views::WidgetDelegateView();
  params.context = context;
  widget->Init(std::move(params));
  return widget;
}

}  // namespace

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
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 300, 100, 100)));
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
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 300, 100, 100)));
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
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 200, 100, 100)));
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
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 200, 100, 100)));
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

}  // namespace ash
