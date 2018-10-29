// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_positioner.h"

#include <memory>
#include <string>

#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {

namespace {

// WindowState based on a given initial state.
class FakeWindowState : public wm::WindowState::State {
 public:
  explicit FakeWindowState(mojom::WindowStateType initial_state_type)
      : state_type_(initial_state_type) {}
  ~FakeWindowState() override = default;

  // WindowState::State overrides:
  void OnWMEvent(wm::WindowState* window_state,
                 const wm::WMEvent* event) override {}
  mojom::WindowStateType GetType() const override { return state_type_; }
  void AttachState(wm::WindowState* window_state,
                   wm::WindowState::State* previous_state) override {}
  void DetachState(wm::WindowState* window_state) override {}

 private:
  mojom::WindowStateType state_type_;

  DISALLOW_COPY_AND_ASSIGN(FakeWindowState);
};

}  // namespace

class PipPositionerTest : public AshTestBase {
 public:
  PipPositionerTest() = default;
  ~PipPositionerTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    keyboard::SetTouchKeyboardEnabled(true);
    Shell::Get()->EnableKeyboard();

    UpdateWorkArea("400x400");
    window_ = CreateTestWindowInShellWithBounds(gfx::Rect(200, 200, 100, 100));
    wm::WindowState* window_state = wm::GetWindowState(window_);
    test_state_ = new FakeWindowState(mojom::WindowStateType::PIP);
    window_state->SetStateObject(
        std::unique_ptr<wm::WindowState::State>(test_state_));
  }

  void TearDown() override {
    keyboard::SetTouchKeyboardEnabled(false);
    AshTestBase::TearDown();
  }

  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    aura::Window* root = Shell::GetPrimaryRootWindow();
    Shell::Get()->SetDisplayWorkAreaInsets(root, gfx::Insets());
  }

 protected:
  aura::Window* window() { return window_; }
  wm::WindowState* window_state() { return wm::GetWindowState(window_); }
  FakeWindowState* test_state() { return test_state_; }

 private:
  aura::Window* window_;
  FakeWindowState* test_state_;

  DISALLOW_COPY_AND_ASSIGN(PipPositionerTest);
};

TEST_F(PipPositionerTest, PipMovementAreaIsInset) {
  gfx::Rect area = PipPositioner::GetMovementArea(window_state()->GetDisplay());
  EXPECT_EQ(gfx::Rect(8, 8, 384, 384), area);
}

TEST_F(PipPositionerTest, PipMovementAreaIncludesKeyboardIfKeyboardIsShown) {
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboard(true /* lock */);
  keyboard_controller->NotifyKeyboardWindowLoaded();

  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 300, 400, 100));

  gfx::Rect area = PipPositioner::GetMovementArea(window_state()->GetDisplay());
  EXPECT_EQ(gfx::Rect(8, 8, 384, 284 - ShelfConstants::shelf_size()), area);
}

TEST_F(PipPositionerTest, PipRestingPositionSnapsToClosestEdge) {
  auto display = window_state()->GetDisplay();

  // Snap near top edge to top.
  EXPECT_EQ(
      gfx::Rect(100, 8, 100, 100),
      PipPositioner::GetRestingPosition(display, gfx::Rect(100, 50, 100, 100)));

  // Snap near bottom edge to bottom.
  EXPECT_EQ(gfx::Rect(100, 292, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(100, 250, 100, 100)));

  // Snap near left edge to left.
  EXPECT_EQ(
      gfx::Rect(8, 100, 100, 100),
      PipPositioner::GetRestingPosition(display, gfx::Rect(50, 100, 100, 100)));

  // Snap near right edge to right.
  EXPECT_EQ(gfx::Rect(292, 100, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(250, 100, 100, 100)));
}

TEST_F(PipPositionerTest, PipRestingPositionSnapsInsideDisplay) {
  auto display = window_state()->GetDisplay();

  // Snap near top edge outside movement area to top.
  EXPECT_EQ(gfx::Rect(100, 8, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(100, -50, 100, 100)));

  // Snap near bottom edge outside movement area to bottom.
  EXPECT_EQ(gfx::Rect(100, 292, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(100, 450, 100, 100)));

  // Snap near left edge outside movement area to left.
  EXPECT_EQ(gfx::Rect(8, 100, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(-50, 100, 100, 100)));

  // Snap near right edge outside movement area to right.
  EXPECT_EQ(gfx::Rect(292, 100, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(450, 100, 100, 100)));
}

TEST_F(PipPositionerTest,
       PipRestingPositionSnapsInDisplayWithLargeAspectRatio) {
  UpdateDisplay("1600x400");
  auto display = window_state()->GetDisplay();

  // Snap to the top edge instead of the far left edge.
  EXPECT_EQ(gfx::Rect(500, 8, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(500, 100, 100, 100)));
}

TEST_F(PipPositionerTest, PipAdjustPositionForDragClampsToMovementArea) {
  auto display = window_state()->GetDisplay();

  // Adjust near top edge outside movement area.
  EXPECT_EQ(
      gfx::Rect(100, 8, 100, 100),
      PipPositioner::GetBoundsForDrag(display, gfx::Rect(100, -50, 100, 100)));

  // Adjust near bottom edge outside movement area.
  EXPECT_EQ(
      gfx::Rect(100, 292, 100, 100),
      PipPositioner::GetBoundsForDrag(display, gfx::Rect(100, 450, 100, 100)));

  // Adjust near left edge outside movement area.
  EXPECT_EQ(
      gfx::Rect(8, 100, 100, 100),
      PipPositioner::GetBoundsForDrag(display, gfx::Rect(-50, 100, 100, 100)));

  // Adjust near right edge outside movement area.
  EXPECT_EQ(
      gfx::Rect(292, 100, 100, 100),
      PipPositioner::GetBoundsForDrag(display, gfx::Rect(450, 100, 100, 100)));
}

TEST_F(PipPositionerTest, PipRestingPositionWorksIfKeyboardIsDisabled) {
  Shell::Get()->DisableKeyboard();
  auto display = window_state()->GetDisplay();

  // Snap near top edge to top.
  EXPECT_EQ(
      gfx::Rect(100, 8, 100, 100),
      PipPositioner::GetRestingPosition(display, gfx::Rect(100, 50, 100, 100)));
}

TEST_F(PipPositionerTest, PipDismissedPositionDoesNotMoveAnExcessiveDistance) {
  auto display = window_state()->GetDisplay();

  EXPECT_EQ(gfx::Rect(100, 100, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(100, 100, 100, 100)));
}

TEST_F(PipPositionerTest, PipDismissedPositionChosesClosestEdge) {
  auto display = window_state()->GetDisplay();

  // Dismiss near top edge outside movement area towards top.
  EXPECT_EQ(gfx::Rect(100, -100, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(100, 50, 100, 100)));

  // Dismiss near bottom edge outside movement area towards bottom.
  EXPECT_EQ(gfx::Rect(100, 400, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(100, 250, 100, 100)));

  // Dismiss near left edge outside movement area towards left.
  EXPECT_EQ(gfx::Rect(-100, 100, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(50, 100, 100, 100)));

  // Dismiss near right edge outside movement area towards right.
  EXPECT_EQ(gfx::Rect(400, 100, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(250, 100, 100, 100)));
}

// Verify that if two edges are equally close, the PIP window prefers dismissing
// out horizontally.
TEST_F(PipPositionerTest, PipDismissedPositionPrefersHorizontal) {
  auto display = window_state()->GetDisplay();

  // Top left corner.
  EXPECT_EQ(
      gfx::Rect(-150, 0, 100, 100),
      PipPositioner::GetDismissedPosition(display, gfx::Rect(0, 0, 100, 100)));

  // Top right corner.
  EXPECT_EQ(gfx::Rect(450, 0, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(300, 0, 100, 100)));

  // Bottom left corner.
  EXPECT_EQ(gfx::Rect(-150, 300, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(0, 300, 100, 100)));

  // Bottom right corner.
  EXPECT_EQ(gfx::Rect(450, 300, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(300, 300, 100, 100)));
}

TEST_F(PipPositionerTest,
       PipRestoresToPreviousBoundsOnMovementAreaChangeIfTheyExist) {
  // Position the PIP window on the side of the screen where it will be next
  // to an edge and therefore in a resting position for the whole test.
  const gfx::Rect bounds = gfx::Rect(292, 200, 100, 100);
  // Set restore position to where the window currently is.
  window()->SetBounds(bounds);
  window_state()->SetRestoreBoundsInScreen(bounds);
  EXPECT_TRUE(window_state()->HasRestoreBounds());

  // Update the work area so that the PIP window should be pushed upward.
  UpdateWorkArea("400x200");

  // Set PIP to the updated constrained bounds.
  const gfx::Rect constrained_bounds =
      PipPositioner::GetPositionAfterMovementAreaChange(window_state());
  EXPECT_EQ(gfx::Rect(292, 92, 100, 100), constrained_bounds);
  window()->SetBounds(constrained_bounds);

  // Restore the original work area.
  UpdateWorkArea("400x400");

  // Expect that the PIP window is put back to where it was before.
  EXPECT_EQ(gfx::Rect(292, 200, 100, 100),
            PipPositioner::GetPositionAfterMovementAreaChange(window_state()));
}

}  // namespace ash
