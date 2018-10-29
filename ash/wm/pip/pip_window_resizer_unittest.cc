// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"

#include <string>

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {
namespace wm {

namespace {

// WindowState based on a given initial state. Records the last resize bounds.
class FakeWindowState : public wm::WindowState::State {
 public:
  explicit FakeWindowState(mojom::WindowStateType initial_state_type)
      : state_type_(initial_state_type) {}
  ~FakeWindowState() override = default;

  // WindowState::State overrides:
  void OnWMEvent(wm::WindowState* window_state,
                 const wm::WMEvent* event) override {
    if (event->IsBoundsEvent()) {
      if (event->type() == wm::WM_EVENT_SET_BOUNDS) {
        const auto* set_bounds_event =
            static_cast<const wm::SetBoundsEvent*>(event);
        last_bounds_ = set_bounds_event->requested_bounds();
      }
    }
  }
  mojom::WindowStateType GetType() const override { return state_type_; }
  void AttachState(wm::WindowState* window_state,
                   wm::WindowState::State* previous_state) override {}
  void DetachState(wm::WindowState* window_state) override {}

  const gfx::Rect& last_bounds() { return last_bounds_; }

 private:
  mojom::WindowStateType state_type_;
  gfx::Rect last_bounds_;

  DISALLOW_COPY_AND_ASSIGN(FakeWindowState);
};

}  // namespace

class PipWindowResizerTest : public AshTestBase {
 public:
  PipWindowResizerTest() = default;
  ~PipWindowResizerTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    keyboard::SetTouchKeyboardEnabled(true);
    Shell::Get()->EnableKeyboard();

    window_.reset(
        CreateTestWindowInShellWithBounds(gfx::Rect(200, 200, 100, 100)));
    wm::WindowState* window_state = wm::GetWindowState(window_.get());
    test_state_ = new FakeWindowState(mojom::WindowStateType::PIP);
    window_state->SetStateObject(
        std::unique_ptr<wm::WindowState::State>(test_state_));
    window_->SetProperty(aura::client::kAlwaysOnTopKey, true);
  }

  void TearDown() override {
    window_.reset();

    keyboard::SetTouchKeyboardEnabled(false);
    AshTestBase::TearDown();
  }

 protected:
  aura::Window* window() { return window_.get(); }
  FakeWindowState* test_state() { return test_state_; }

  PipWindowResizer* CreateResizerForTest(int window_component) {
    wm::WindowState* window_state = wm::GetWindowState(window());
    window_state->CreateDragDetails(gfx::Point(), window_component,
                                    ::wm::WINDOW_MOVE_SOURCE_MOUSE);
    return new PipWindowResizer(window_state);
  }

  gfx::Point CalculateDragPoint(const WindowResizer& resizer,
                                int delta_x,
                                int delta_y) const {
    gfx::Point location = resizer.GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    aura::Window* root = Shell::GetPrimaryRootWindow();
    Shell::Get()->SetDisplayWorkAreaInsets(root, gfx::Insets());
  }

 private:
  std::unique_ptr<aura::Window> window_;
  FakeWindowState* test_state_;

  DISALLOW_COPY_AND_ASSIGN(PipWindowResizerTest);
};

TEST_F(PipWindowResizerTest, PipWindowCanDrag) {
  UpdateWorkArea("400x800");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 210, 100, 100), test_state()->last_bounds());
}

TEST_F(PipWindowResizerTest, PipWindowCanResize) {
  UpdateWorkArea("400x800");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 200, 100, 110), test_state()->last_bounds());
}

TEST_F(PipWindowResizerTest, PipWindowDragIsRestrictedToWorkArea) {
  UpdateWorkArea("400x400");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Drag to the right.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 0), 0);
  EXPECT_EQ(gfx::Rect(292, 200, 100, 100), test_state()->last_bounds());

  // Drag down.
  resizer->Drag(CalculateDragPoint(*resizer, 0, 800), 0);
  EXPECT_EQ(gfx::Rect(200, 292, 100, 100), test_state()->last_bounds());

  // Drag to the left.
  resizer->Drag(CalculateDragPoint(*resizer, -800, 0), 0);
  EXPECT_EQ(gfx::Rect(8, 200, 100, 100), test_state()->last_bounds());

  // Drag up.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -800), 0);
  EXPECT_EQ(gfx::Rect(200, 8, 100, 100), test_state()->last_bounds());
}

TEST_F(PipWindowResizerTest, PipWindowCanBeDraggedInTabletMode) {
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  UpdateWorkArea("400x800");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 210, 100, 100), test_state()->last_bounds());
}

TEST_F(PipWindowResizerTest, PipWindowCanBeResizedInTabletMode) {
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  UpdateWorkArea("400x800");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 200, 100, 110), test_state()->last_bounds());
}

}  // namespace wm
}  // namespace ash
