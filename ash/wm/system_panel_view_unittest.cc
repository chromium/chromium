// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_panel_view.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/window.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kPanelWidth = 200;
constexpr int kPanelHeight = 300;
constexpr gfx::Rect kInitialBounds(100, 100, kPanelWidth, kPanelHeight);

std::unique_ptr<views::Widget> CreateWidget() {
  auto widget = std::make_unique<views::Widget>();

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));
  widget->SetContentsView(std::make_unique<SystemPanelView>());
  widget->Show();
  return widget;
}

// namespace

using SystemPanelViewTest = AshTestBase;

TEST_F(SystemPanelViewTest, MouseDragRepositionsPanel) {
  auto panel_widget = CreateWidget();
  panel_widget->SetBounds(kInitialBounds);

  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset(20, 10);
  GetEventGenerator()->DragMouseBy(kDragOffset.x(), kDragOffset.y());

  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset);
}

TEST_F(SystemPanelViewTest, GestureDragRepositionsPanel) {
  auto panel_widget = CreateWidget();
  panel_widget->SetBounds(kInitialBounds);

  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset(20, 10);
  GetEventGenerator()->PressMoveAndReleaseTouchBy(kDragOffset.x(),
                                                  kDragOffset.y());

  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset);
}

TEST_F(SystemPanelViewTest, MouseDragOutOfTheScreenEdge) {
  UpdateDisplay("1000x500");
  auto panel_widget = CreateWidget();
  panel_widget->SetBounds(kInitialBounds);

  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset(1100, 600);
  GetEventGenerator()->DragMouseBy(kDragOffset.x(), kDragOffset.y());

  // If the panel is dragged out of the edge, it should show 1/3 of its height
  // and width on the screen.
  constexpr gfx::Rect kMaxBounds(1000 /*display width*/ - kPanelWidth / 3.0,
                                 500 /*display height*/ - kPanelHeight / 3.0,
                                 kPanelWidth, kPanelHeight);
  EXPECT_NE(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset);
  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(), kMaxBounds);
}

TEST_F(SystemPanelViewTest, MouseDragInMultiScreens) {
  // Adds 3 displays.
  UpdateDisplay("1000x500,1000x500,1000x500");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // Puts the second display on the right of the first one and the third display
  // on the bottom of the second one.
  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(3u, list.size());
  ASSERT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(), list[0]);
  display::DisplayLayoutBuilder builder(list[0]);
  builder.AddDisplayPlacement(list[1], list[0],
                              display::DisplayPlacement::RIGHT, 0);
  builder.AddDisplayPlacement(list[2], list[1],
                              display::DisplayPlacement::BOTTOM, 0);
  display_manager()->SetLayoutForCurrentDisplays(builder.Build());

  auto panel_widget = CreateWidget();
  panel_widget->SetBounds(kInitialBounds);

  // The panel is on the first display.
  EXPECT_EQ(panel_widget->GetNativeWindow()->GetRootWindow(), root_windows[0]);

  // Drags the panel from the first screen to the second screen.
  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset1(1500, 50);
  GetEventGenerator()->DragMouseBy(kDragOffset1.x(), kDragOffset1.y());

  // The panel is on the second display now.
  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset1);
  EXPECT_EQ(panel_widget->GetNativeWindow()->GetRootWindow(), root_windows[1]);

  // Drags the panel from the second screen to the third screen.
  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset2(50, 600);
  GetEventGenerator()->DragMouseBy(kDragOffset2.x(), kDragOffset2.y());

  // The panel is on the third display now.
  EXPECT_EQ(panel_widget->GetNativeWindow()->GetRootWindow(), root_windows[2]);
  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset1 + kDragOffset2);
}

}  // namespace

}  // namespace ash
