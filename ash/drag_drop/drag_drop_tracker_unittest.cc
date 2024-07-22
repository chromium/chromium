// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_tracker.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/events/event_utils.h"

namespace ash {

class DragDropTrackerTest : public AshTestBase {
 public:
  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    return TestWindowBuilder()
        .SetBounds(bounds)
        .SetTestWindowDelegate()
        .Build()
        .release();
  }

  static aura::Window* GetTarget(const gfx::Point& location) {
    std::unique_ptr<DragDropTracker> tracker(new DragDropTracker(
        Shell::GetPrimaryRootWindow(), base::BindLambdaForTesting([&]() {})));
    ui::MouseEvent e(ui::EventType::kMouseDragged, location, location,
                     ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
    aura::Window* target = tracker->GetTarget(e);
    return target;
  }

  static std::unique_ptr<ui::LocatedEvent> ConvertEvent(
      aura::Window* target,
      const ui::MouseEvent& event) {
    std::unique_ptr<DragDropTracker> tracker(new DragDropTracker(
        Shell::GetPrimaryRootWindow(), base::BindLambdaForTesting([&]() {})));
    return tracker->ConvertEvent(target, event);
  }
};

TEST_F(DragDropTrackerTest, GetTarget) {
  UpdateDisplay("200x300,400x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  std::unique_ptr<aura::Window> window0(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  window0->Show();

  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(300, 100, 100, 100)));
  window1->Show();
  EXPECT_EQ(root_windows[0], window0->GetRootWindow());
  EXPECT_EQ(root_windows[1], window1->GetRootWindow());
  EXPECT_EQ("0,0 100x100", window0->GetBoundsInScreen().ToString());
  EXPECT_EQ("300,100 100x100", window1->GetBoundsInScreen().ToString());

  // RootWindow0 is active so the capture window is parented to it.
  EXPECT_EQ(root_windows[0], Shell::GetRootWindowForNewWindows());

  // Start tracking from the RootWindow1 and check the point on RootWindow0 that
  // |window0| covers.
  EXPECT_EQ(window0.get(), GetTarget(gfx::Point(50, 50)));

  // Start tracking from the RootWindow0 and check the point on RootWindow0 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(gfx::Point(150, 150)));
  EXPECT_NE(window1.get(), GetTarget(gfx::Point(150, 150)));

  // Start tracking from the RootWindow0 and check the point on RootWindow1 that
  // |window1| covers.
  EXPECT_EQ(window1.get(), GetTarget(gfx::Point(350, 150)));

  // Start tracking from the RootWindow0 and check the point on RootWindow1 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(gfx::Point(50, 250)));
  EXPECT_NE(window1.get(), GetTarget(gfx::Point(50, 250)));

  // Make RootWindow1 active so that capture window is parented to it.
  display::ScopedDisplayForNewWindows display_for_new_windows(root_windows[1]);

  // Start tracking from the RootWindow1 and check the point on RootWindow0 that
  // |window0| covers.
  EXPECT_EQ(window0.get(), GetTarget(gfx::Point(-150, 50)));

  // Start tracking from the RootWindow1 and check the point on RootWindow0 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(gfx::Point(150, -50)));
  EXPECT_NE(window1.get(), GetTarget(gfx::Point(150, -50)));

  // Start tracking from the RootWindow1 and check the point on RootWindow1 that
  // |window1| covers.
  EXPECT_EQ(window1.get(), GetTarget(gfx::Point(150, 150)));

  // Start tracking from the RootWindow1 and check the point on RootWindow1 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(gfx::Point(50, 50)));
  EXPECT_NE(window1.get(), GetTarget(gfx::Point(50, 50)));
}

TEST_F(DragDropTrackerTest, ConvertEvent) {
  UpdateDisplay("200x300,400x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  std::unique_ptr<aura::Window> window0(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  window0->Show();

  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(300, 100, 100, 100)));
  window1->Show();

  // RootWindow0 is active so the capture window is parented to it.
  EXPECT_EQ(root_windows[0], Shell::GetRootWindowForNewWindows());

  // Start tracking from the RootWindow0 and converts the mouse event into
  // |window0|'s coodinates.
  ui::MouseEvent original00(ui::EventType::kMouseDragged, gfx::Point(50, 50),
                            gfx::Point(50, 50), ui::EventTimeForNow(),
                            ui::EF_NONE, ui::EF_NONE);
  std::unique_ptr<ui::LocatedEvent> converted00(
      ConvertEvent(window0.get(), original00));
  EXPECT_EQ(original00.type(), converted00->type());
  EXPECT_EQ("50,50", converted00->location().ToString());
  EXPECT_EQ("50,50", converted00->root_location().ToString());
  EXPECT_EQ(original00.flags(), converted00->flags());

  // Start tracking from the RootWindow0 and converts the mouse event into
  // |window1|'s coodinates.
  ui::MouseEvent original01(ui::EventType::kMouseDragged, gfx::Point(350, 150),
                            gfx::Point(350, 150), ui::EventTimeForNow(),
                            ui::EF_NONE, ui::EF_NONE);
  std::unique_ptr<ui::LocatedEvent> converted01(
      ConvertEvent(window1.get(), original01));
  EXPECT_EQ(original01.type(), converted01->type());
  EXPECT_EQ("50,50", converted01->location().ToString());
  EXPECT_EQ("150,150", converted01->root_location().ToString());
  EXPECT_EQ(original01.flags(), converted01->flags());

  // Make RootWindow1 active so that capture window is parented to it.
  display::ScopedDisplayForNewWindows display_for_new_windows(root_windows[1]);

  // Start tracking from the RootWindow1 and converts the mouse event into
  // |window0|'s coodinates.
  ui::MouseEvent original10(ui::EventType::kMouseDragged, gfx::Point(-150, 50),
                            gfx::Point(-150, 50), ui::EventTimeForNow(),
                            ui::EF_NONE, ui::EF_NONE);
  std::unique_ptr<ui::LocatedEvent> converted10(
      ConvertEvent(window0.get(), original10));
  EXPECT_EQ(original10.type(), converted10->type());
  EXPECT_EQ("50,50", converted10->location().ToString());
  EXPECT_EQ("50,50", converted10->root_location().ToString());
  EXPECT_EQ(original10.flags(), converted10->flags());

  // Start tracking from the RootWindow1 and converts the mouse event into
  // |window1|'s coodinates.
  ui::MouseEvent original11(ui::EventType::kMouseDragged, gfx::Point(150, 150),
                            gfx::Point(150, 150), ui::EventTimeForNow(),
                            ui::EF_NONE, ui::EF_NONE);
  std::unique_ptr<ui::LocatedEvent> converted11(
      ConvertEvent(window1.get(), original11));
  EXPECT_EQ(original11.type(), converted11->type());
  EXPECT_EQ("50,50", converted11->location().ToString());
  EXPECT_EQ("150,150", converted11->root_location().ToString());
  EXPECT_EQ(original11.flags(), converted11->flags());
}

}  // namespace ash
