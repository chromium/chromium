// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_util.h"

#include <memory>

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/unified_desktop_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

using ScreenUtilTest = AshTestBase;

TEST_F(ScreenUtilTest, Bounds) {
  UpdateDisplay("700x600,600x500");
  views::Widget* primary = views::Widget::CreateWindowWithContext(
      nullptr, GetContext(), gfx::Rect(10, 10, 100, 100));
  primary->Show();
  views::Widget* secondary = views::Widget::CreateWindowWithContext(
      nullptr, GetContext(), gfx::Rect(710, 10, 100, 100));
  secondary->Show();

  // Maximized bounds.
  const int bottom_inset_first = 600 - ShelfConfig::Get()->shelf_size();
  const int bottom_inset_second = 500 - ShelfConfig::Get()->shelf_size();
  EXPECT_EQ(
      gfx::Rect(0, 0, 700, bottom_inset_first).ToString(),
      screen_util::GetMaximizedWindowBoundsInParent(primary->GetNativeView())
          .ToString());
  EXPECT_EQ(
      gfx::Rect(0, 0, 600, bottom_inset_second).ToString(),
      screen_util::GetMaximizedWindowBoundsInParent(secondary->GetNativeView())
          .ToString());

  // Display bounds
  EXPECT_EQ("0,0 700x600",
            screen_util::GetDisplayBoundsInParent(primary->GetNativeView())
                .ToString());
  EXPECT_EQ("0,0 600x500",
            screen_util::GetDisplayBoundsInParent(secondary->GetNativeView())
                .ToString());

  // Work area bounds
  EXPECT_EQ(
      gfx::Rect(0, 0, 700, bottom_inset_first).ToString(),
      screen_util::GetDisplayWorkAreaBoundsInParent(primary->GetNativeView())
          .ToString());
  EXPECT_EQ(
      gfx::Rect(0, 0, 600, bottom_inset_second).ToString(),
      screen_util::GetDisplayWorkAreaBoundsInParent(secondary->GetNativeView())
          .ToString());
}

// Test verifies a stable handling of secondary screen widget changes
// (crbug.com/226132).
TEST_F(ScreenUtilTest, StabilityTest) {
  UpdateDisplay("700x600,600x500");
  views::Widget* secondary = views::Widget::CreateWindowWithContext(
      nullptr, GetContext(), gfx::Rect(710, 10, 100, 100));
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
            secondary->GetNativeView()->GetRootWindow());
  secondary->Show();
  secondary->Maximize();
  secondary->Show();
  secondary->SetFullscreen(true);
  secondary->Hide();
  secondary->Close();
}

TEST_F(ScreenUtilTest, ConvertRect) {
  UpdateDisplay("700x600,600x500");

  views::Widget* primary = views::Widget::CreateWindowWithContext(
      nullptr, GetContext(), gfx::Rect(10, 10, 100, 100));
  primary->Show();
  views::Widget* secondary = views::Widget::CreateWindowWithContext(
      nullptr, GetContext(), gfx::Rect(710, 10, 100, 100));
  secondary->Show();

  gfx::Rect r1(10, 10, 100, 100);
  ::wm::ConvertRectFromScreen(primary->GetNativeView(), &r1);
  EXPECT_EQ("0,0 100x100", r1.ToString());

  gfx::Rect r2(720, 20, 100, 100);
  ::wm::ConvertRectFromScreen(secondary->GetNativeView(), &r2);
  EXPECT_EQ("10,10 100x100", r2.ToString());

  gfx::Rect r3(30, 30, 100, 100);
  ::wm::ConvertRectToScreen(primary->GetNativeView(), &r3);
  EXPECT_EQ("40,40 100x100", r3.ToString());

  gfx::Rect r4(40, 40, 100, 100);
  ::wm::ConvertRectToScreen(secondary->GetNativeView(), &r4);
  EXPECT_EQ("750,50 100x100", r4.ToString());
}

TEST_F(ScreenUtilTest, ShelfDisplayBoundsInUnifiedDesktop) {
  display_manager()->SetUnifiedDesktopEnabled(true);

  views::Widget* widget = views::Widget::CreateWindowWithContext(
      nullptr, GetContext(), gfx::Rect(10, 10, 100, 100));
  aura::Window* window = widget->GetNativeWindow();

  UpdateDisplay("500x400");
  EXPECT_EQ("0,0 500x400",
            screen_util::GetDisplayBoundsWithShelf(window).ToString());

  UpdateDisplay("500x400,600x400");
  EXPECT_EQ("0,0 500x400",
            screen_util::GetDisplayBoundsWithShelf(window).ToString());

  // Move to the 2nd physical display. Shelf's display still should be
  // the first.
  widget->SetBounds(gfx::Rect(800, 0, 100, 100));
  ASSERT_EQ("800,0 100x100", widget->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ("0,0 500x400",
            screen_util::GetDisplayBoundsWithShelf(window).ToString());

  UpdateDisplay("600x500");
  EXPECT_EQ("0,0 600x500",
            screen_util::GetDisplayBoundsWithShelf(window).ToString());
}

TEST_F(ScreenUtilTest, ShelfDisplayBoundsInUnifiedDesktopGrid) {
  UpdateDisplay("500x400,400x600,300x600,200x300,600x200,350x400");
  display_manager()->SetUnifiedDesktopEnabled(true);

  views::Widget* widget = views::Widget::CreateWindowWithContext(
      nullptr, GetContext(), gfx::Rect(10, 10, 100, 100));
  aura::Window* window = widget->GetNativeWindow();

  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(6u, list.size());
  // Create a 3 x 2 vertical layout matrix and set it.
  // [500 x 400] [400 x 600]
  // [300 x 600] [200 x 300]
  // [600 x 200] [350 x 400]
  display::UnifiedDesktopLayoutMatrix matrix;
  matrix.resize(3u);
  matrix[0].emplace_back(list[0]);
  matrix[0].emplace_back(list[1]);
  matrix[1].emplace_back(list[2]);
  matrix[1].emplace_back(list[3]);
  matrix[2].emplace_back(list[4]);
  matrix[2].emplace_back(list[5]);
  display_manager()->SetUnifiedDesktopMatrix(matrix);
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(gfx::Size(766, 1254), screen->GetPrimaryDisplay().size());

  Shelf* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  EXPECT_EQ(shelf->alignment(), ShelfAlignment::kBottom);

  // Regardless of where the window is, the shelf with a bottom alignment is
  // always in the bottom left display in the matrix.
  EXPECT_EQ(gfx::Rect(0, 1057, 593, 198),
            screen_util::GetDisplayBoundsWithShelf(window));

  // Move to the bottom right display.
  widget->SetBounds(gfx::Rect(620, 940, 100, 100));
  EXPECT_EQ(gfx::Rect(0, 1057, 593, 198),
            screen_util::GetDisplayBoundsWithShelf(window));

  // Change the shelf alignment to left, and expect that it now resides in the
  // top left display in the matrix.
  shelf->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_EQ(gfx::Rect(0, 0, 499, 400),
            screen_util::GetDisplayBoundsWithShelf(window));

  // Change the shelf alignment to right, and expect that it now resides in the
  // top right display in the matrix.
  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(gfx::Rect(499, 0, 267, 400),
            screen_util::GetDisplayBoundsWithShelf(window));

  // Change alignment back to bottom and change the unified display zoom factor.
  // Expect that the display with shelf bounds will take into account the zoom
  // factor.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  display_manager()->UpdateZoomFactor(display::kUnifiedDisplayId, 3.f);
  const display::Display unified_display =
      display_manager()->GetDisplayForId(display::kUnifiedDisplayId);
  EXPECT_FLOAT_EQ(unified_display.device_scale_factor(), 3.f);
  EXPECT_EQ(gfx::Rect(0, 352, 198, 67),
            screen_util::GetDisplayBoundsWithShelf(window));
}

TEST_F(ScreenUtilTest, SnapBoundsToDisplayEdge) {
  UpdateDisplay("2400x1600*1.5");

  gfx::Rect bounds(1555, 0, 45, 1066);
  views::Widget* widget =
      views::Widget::CreateWindowWithContext(nullptr, GetContext(), bounds);
  aura::Window* window = widget->GetNativeWindow();

  gfx::Rect snapped_bounds =
      screen_util::SnapBoundsToDisplayEdge(bounds, window);

  EXPECT_EQ(snapped_bounds, gfx::Rect(1555, 0, 45, 1067));

  bounds = gfx::Rect(5, 1000, 1595, 66);
  snapped_bounds = screen_util::SnapBoundsToDisplayEdge(bounds, window);
  EXPECT_EQ(snapped_bounds, gfx::Rect(5, 1000, 1595, 67));

  UpdateDisplay("800x600");
  bounds = gfx::Rect(0, 552, 800, 48);
  snapped_bounds = screen_util::SnapBoundsToDisplayEdge(bounds, window);
  EXPECT_EQ(snapped_bounds, gfx::Rect(0, 552, 800, 48));

  UpdateDisplay("2400x1800*1.8/r");
  EXPECT_EQ(gfx::Size(1000, 1333),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());
  bounds = gfx::Rect(950, 0, 50, 1333);
  snapped_bounds = screen_util::SnapBoundsToDisplayEdge(bounds, window);
  EXPECT_EQ(snapped_bounds, gfx::Rect(950, 0, 50, 1334));
}

// Tests that making a window fullscreen while the Docked Magnifier is enabled
// won't make its bounds occupy the entire screen bounds, but will take into
// account the Docked Magnifier height.
TEST_F(ScreenUtilTest, FullscreenWindowBoundsWithDockedMagnifier) {
  UpdateDisplay("1366x768");

  std::unique_ptr<aura::Window> window = CreateToplevelTestWindow(
      gfx::Rect(300, 300, 200, 150), desks_util::GetActiveDeskContainerId());

  auto* docked_magnifier_controller =
      Shell::Get()->docked_magnifier_controller();
  docked_magnifier_controller->SetEnabled(true);

  const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window.get())->OnWMEvent(&event);

  constexpr gfx::Rect kDisplayBounds{1366, 768};
  EXPECT_NE(window->bounds(), kDisplayBounds);

  gfx::Rect expected_bounds = kDisplayBounds;
  expected_bounds.Inset(gfx::Insets().set_top(
      docked_magnifier_controller->GetTotalMagnifierHeight()));
  EXPECT_EQ(expected_bounds, window->bounds());
}

}  // namespace ash
