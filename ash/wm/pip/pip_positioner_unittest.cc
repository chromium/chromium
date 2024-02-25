// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_positioner.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/pip/pip_test_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

display::Display GetDisplayForWindow(aura::Window* window) {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window);
}

gfx::Rect ConvertToScreenForWindow(aura::Window* window,
                                   const gfx::Rect& bounds) {
  gfx::Rect new_bounds = bounds;
  ::wm::ConvertRectToScreen(window->GetRootWindow(), &new_bounds);
  return new_bounds;
}

}  // namespace

class PipPositionerDisplayTest : public AshTestBase,
                                 public ::testing::WithParamInterface<
                                     std::tuple<std::string, std::size_t>> {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();

    const std::string& display_string = std::get<0>(GetParam());
    const std::size_t root_window_index = std::get<1>(GetParam());
    UpdateWorkArea(display_string);
    ASSERT_LT(root_window_index, Shell::GetAllRootWindows().size());
    root_window_ = Shell::GetAllRootWindows()[root_window_index].get();
    scoped_display_ =
        std::make_unique<display::ScopedDisplayForNewWindows>(root_window_);
    ForceHideShelvesForTest();
  }

  void TearDown() override {
    scoped_display_.reset();
    AshTestBase::TearDown();
  }

 protected:
  display::Display GetDisplay() { return GetDisplayForWindow(root_window_); }

  aura::Window* root_window() { return root_window_; }

  gfx::Rect ConvertToScreen(const gfx::Rect& bounds) {
    return ConvertToScreenForWindow(root_window_, bounds);
  }

  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
          root, gfx::Rect(), gfx::Insets(), gfx::Insets());
    }
  }

 private:
  std::unique_ptr<display::ScopedDisplayForNewWindows> scoped_display_;
  raw_ptr<aura::Window, DanglingUntriaged> root_window_;
};

TEST_P(PipPositionerDisplayTest, PipAdjustPositionForDragClampsToMovementArea) {
  auto display = GetDisplay();
  int right = display.bounds().width();
  int bottom = display.bounds().height();

  // Adjust near top edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 8, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(100, -50, 100, 100)),
                gfx::Transform()));

  // Adjust near bottom edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, bottom - 108, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(100, bottom + 50, 100, 100)),
                gfx::Transform()));

  // Adjust near left edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(8, 100, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(-50, 100, 100, 100)),
                gfx::Transform()));

  // Adjust near right edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(right - 108, 100, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(right + 50, 100, 100, 100)),
                gfx::Transform()));
}

TEST_P(PipPositionerDisplayTest,
       PipDismissedPositionDoesNotMoveAnExcessiveDistance) {
  auto display = GetDisplay();

  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(100, 100, 100, 100))));
}

TEST_P(PipPositionerDisplayTest, PipDismissedPositionChosesClosestEdge) {
  auto display = GetDisplay();
  int right = display.bounds().width();
  int bottom = display.bounds().height();

  // Dismiss near top edge outside movement area towards top.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, -100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(100, 50, 100, 100))));

  // Dismiss near bottom edge outside movement area towards bottom.
  EXPECT_EQ(
      ConvertToScreen(gfx::Rect(100, bottom, 100, 100)),
      PipPositioner::GetDismissedPosition(
          display, ConvertToScreen(gfx::Rect(100, bottom - 150, 100, 100))));

  // Dismiss near left edge outside movement area towards left.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(-100, 100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(50, 100, 100, 100))));

  // Dismiss near right edge outside movement area towards right.
  EXPECT_EQ(
      ConvertToScreen(gfx::Rect(right, 100, 100, 100)),
      PipPositioner::GetDismissedPosition(
          display, ConvertToScreen(gfx::Rect(right - 150, 100, 100, 100))));
}

// Verify that if two edges are equally close, the PIP window prefers dismissing
// out horizontally.
TEST_P(PipPositionerDisplayTest, PipDismissedPositionPrefersHorizontal) {
  auto display = GetDisplay();
  int right = display.bounds().width();
  int bottom = display.bounds().height();

  // Top left corner.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(-150, 0, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(0, 0, 100, 100))));

  // Top right corner.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(right + 50, 0, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(right - 100, 0, 100, 100))));

  // Bottom left corner.
  EXPECT_EQ(
      ConvertToScreen(gfx::Rect(-150, bottom - 100, 100, 100)),
      PipPositioner::GetDismissedPosition(
          display, ConvertToScreen(gfx::Rect(0, bottom - 100, 100, 100))));

  // Bottom right corner.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(right + 50, bottom - 100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(
                             gfx::Rect(right - 100, bottom - 100, 100, 100))));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PipPositionerDisplayTest,
    testing::Values(std::make_tuple("500x400", 0u),
                    std::make_tuple("500x400/r", 0u),
                    std::make_tuple("500x400/u", 0u),
                    std::make_tuple("500x400/l", 0u),
                    std::make_tuple("800x700*2", 0u),
                    std::make_tuple("500x400,500x400", 0u),
                    std::make_tuple("500x400,500x400", 1u)));

}  // namespace ash
