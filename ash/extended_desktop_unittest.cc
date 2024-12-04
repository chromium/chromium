// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/window_test_api.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

void SetSecondaryDisplayLayout(display::DisplayPlacement::Position position) {
  std::unique_ptr<display::DisplayLayout> layout =
      Shell::Get()->display_manager()->GetCurrentDisplayLayout().Copy();
  layout->placement_list[0].position = position;
  Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
      std::move(layout));
}

// An event handler which moves the target window to the secondary root window
// at pre-handle phase of a mouse release event.
class MoveWindowByClickEventHandler : public ui::EventHandler {
 public:
  explicit MoveWindowByClickEventHandler(aura::Window* target)
      : target_(target) {}

  MoveWindowByClickEventHandler(const MoveWindowByClickEventHandler&) = delete;
  MoveWindowByClickEventHandler& operator=(
      const MoveWindowByClickEventHandler&) = delete;

  ~MoveWindowByClickEventHandler() override = default;

 private:
  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMouseReleased) {
      aura::Window::Windows root_windows = Shell::GetAllRootWindows();
      DCHECK_LT(1u, root_windows.size());
      root_windows[1]->AddChild(target_);
    }
  }

  raw_ptr<aura::Window> target_;
};

// An event handler which records the event's locations.
class EventLocationRecordingEventHandler : public ui::EventHandler {
 public:
  EventLocationRecordingEventHandler() { Reset(); }

  EventLocationRecordingEventHandler(
      const EventLocationRecordingEventHandler&) = delete;
  EventLocationRecordingEventHandler& operator=(
      const EventLocationRecordingEventHandler&) = delete;

  ~EventLocationRecordingEventHandler() override = default;

  // |location_| is relative to the target window.
  std::string GetLocation() const { return location_.ToString(); }

  // |root_location_| is relative to the display where the event started.
  std::string GetRootLocation() const { return root_location_.ToString(); }

  void Reset() {
    location_.SetPoint(-999, -999);
    root_location_.SetPoint(-999, -999);
  }

 private:
  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMouseMoved ||
        event->type() == ui::EventType::kMouseDragged) {
      location_ = event->location();
      root_location_ = event->root_location();
    }
  }

  gfx::Point root_location_;
  gfx::Point location_;
};

class EventLocationHandler : public ui::EventHandler {
 public:
  EventLocationHandler() = default;

  EventLocationHandler(const EventLocationHandler&) = delete;
  EventLocationHandler& operator=(const EventLocationHandler&) = delete;

  ~EventLocationHandler() override = default;

  const gfx::Point& press_location() const { return press_location_; }
  const gfx::Point& release_location() const { return release_location_; }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMousePressed) {
      press_location_ = event->location();
    } else if (event->type() == ui::EventType::kMouseReleased) {
      release_location_ = event->location();
    }
  }

  gfx::Point press_location_;
  gfx::Point release_location_;
};

}  // namespace

class ExtendedDesktopTest : public AshTestBase {
 public:
  ExtendedDesktopTest() = default;

  ExtendedDesktopTest(const ExtendedDesktopTest&) = delete;
  ExtendedDesktopTest& operator=(const ExtendedDesktopTest&) = delete;

  ~ExtendedDesktopTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Shell hides the cursor by default; show it for these tests.
    Shell::Get()->cursor_manager()->ShowCursor();
  }

  views::Widget* CreateTestWidget(const gfx::Rect& bounds) {
    return TestWidgetBuilder()
        .SetBounds(bounds)
        .SetContext(GetContext())
        .BuildOwnedByNativeWidget();
  }

 protected:
  bool IsBubbleShown() {
    return GetPrimaryUnifiedSystemTray()->IsBubbleShown();
  }

  gfx::Rect GetSystemTrayBoundsInScreen() {
    return GetPrimaryUnifiedSystemTray()->GetBoundsInScreen();
  }
};

// Test conditions that root windows in extended desktop mode must satisfy.
TEST_F(ExtendedDesktopTest, Basic) {
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // All root windows must have a root window controller.
  ASSERT_EQ(2U, root_windows.size());
  EXPECT_TRUE(RootWindowController::ForWindow(root_windows[0]));
  EXPECT_TRUE(RootWindowController::ForWindow(root_windows[1]));
  // Make sure root windows share the same controllers.
  EXPECT_EQ(aura::client::GetFocusClient(root_windows[0]),
            aura::client::GetFocusClient(root_windows[1]));
  EXPECT_EQ(::wm::GetActivationClient(root_windows[0]),
            ::wm::GetActivationClient(root_windows[1]));
  EXPECT_EQ(aura::client::GetCaptureClient(root_windows[0]),
            aura::client::GetCaptureClient(root_windows[1]));
}

TEST_F(ExtendedDesktopTest, Activation) {
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  views::Widget* widget_on_1st = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  views::Widget* widget_on_2nd =
      CreateTestWidget(gfx::Rect(1200, 10, 100, 100));
  EXPECT_EQ(root_windows[0], widget_on_1st->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], widget_on_2nd->GetNativeView()->GetRootWindow());

  EXPECT_EQ(widget_on_2nd->GetNativeView(),
            aura::client::GetFocusClient(root_windows[0])->GetFocusedWindow());
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_2nd->GetNativeView()));

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  // Clicking a window changes the active window and active root window.
  event_generator->MoveMouseToCenterOf(widget_on_1st->GetNativeView());
  event_generator->ClickLeftButton();

  EXPECT_EQ(widget_on_1st->GetNativeView(),
            aura::client::GetFocusClient(root_windows[0])->GetFocusedWindow());
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_1st->GetNativeView()));

  event_generator->MoveMouseToCenterOf(widget_on_2nd->GetNativeView());
  event_generator->ClickLeftButton();

  EXPECT_EQ(widget_on_2nd->GetNativeView(),
            aura::client::GetFocusClient(root_windows[0])->GetFocusedWindow());
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_2nd->GetNativeView()));
}

TEST_F(ExtendedDesktopTest, SystemModal) {
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  views::Widget* widget_on_1st = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_1st->GetNativeView()));
  EXPECT_EQ(root_windows[0], widget_on_1st->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[0], Shell::GetRootWindowForNewWindows());

  // Open system modal. Make sure it's on 2nd root window and active.
  auto delegate = std::make_unique<views::WidgetDelegateView>();
  delegate->SetModalType(ui::mojom::ModalType::kSystem);
  views::Widget* modal_widget = views::Widget::CreateWindowWithContext(
      delegate.release(), GetContext(), gfx::Rect(1200, 100, 100, 100));
  modal_widget->Show();
  EXPECT_TRUE(wm::IsActiveWindow(modal_widget->GetNativeView()));
  EXPECT_EQ(root_windows[1], modal_widget->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], Shell::GetRootWindowForNewWindows());

  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Clicking a widget on widget_on_1st display should not change activation.
  event_generator->MoveMouseToCenterOf(widget_on_1st->GetNativeView());
  event_generator->ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(modal_widget->GetNativeView()));
  EXPECT_EQ(root_windows[1], Shell::GetRootWindowForNewWindows());

  // Close system modal and so clicking a widget should work now.
  modal_widget->Close();
  event_generator->MoveMouseToCenterOf(widget_on_1st->GetNativeView());
  event_generator->ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_1st->GetNativeView()));
  EXPECT_EQ(root_windows[0], Shell::GetRootWindowForNewWindows());
}

TEST_F(ExtendedDesktopTest, TestCursor) {
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::WindowTreeHost* host0 = root_windows[0]->GetHost();
  aura::WindowTreeHost* host1 = root_windows[1]->GetHost();
  EXPECT_EQ(ui::mojom::CursorType::kPointer, host0->last_cursor().type());
  EXPECT_EQ(ui::mojom::CursorType::kNull, host1->last_cursor().type());
  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kCopy);
  EXPECT_EQ(ui::mojom::CursorType::kCopy, host0->last_cursor().type());
  EXPECT_EQ(ui::mojom::CursorType::kCopy, host1->last_cursor().type());
}

TEST_F(ExtendedDesktopTest, TestCursorLocation) {
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::test::WindowTestApi root_window0_test_api(root_windows[0]);
  aura::test::WindowTestApi root_window1_test_api(root_windows[1]);

  root_windows[0]->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("10,10",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());
  EXPECT_TRUE(root_window0_test_api.ContainsMouse());
  EXPECT_FALSE(root_window1_test_api.ContainsMouse());
  root_windows[1]->MoveCursorTo(gfx::Point(10, 20));
  EXPECT_EQ("1010,20",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());
  EXPECT_FALSE(root_window0_test_api.ContainsMouse());
  EXPECT_TRUE(root_window1_test_api.ContainsMouse());
  root_windows[0]->MoveCursorTo(gfx::Point(20, 10));
  EXPECT_EQ("20,10",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());
  EXPECT_TRUE(root_window0_test_api.ContainsMouse());
  EXPECT_FALSE(root_window1_test_api.ContainsMouse());
}

TEST_F(ExtendedDesktopTest, GetRootWindowAt) {
  UpdateDisplay("700x500,600x500");
  SetSecondaryDisplayLayout(display::DisplayPlacement::LEFT);
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  using window_util::GetRootWindowAt;
  EXPECT_EQ(root_windows[1], GetRootWindowAt(gfx::Point(-400, 100)));
  EXPECT_EQ(root_windows[1], GetRootWindowAt(gfx::Point(-1, 100)));
  EXPECT_EQ(root_windows[0], GetRootWindowAt(gfx::Point(0, 300)));
  EXPECT_EQ(root_windows[0], GetRootWindowAt(gfx::Point(700, 300)));

  // Zero origin.
  EXPECT_EQ(root_windows[0], GetRootWindowAt(gfx::Point(0, 0)));

  // Out of range point should return the nearest root window
  EXPECT_EQ(root_windows[1], GetRootWindowAt(gfx::Point(-600, 0)));
  EXPECT_EQ(root_windows[0], GetRootWindowAt(gfx::Point(701, 100)));
}

TEST_F(ExtendedDesktopTest, GetRootWindowMatching) {
  UpdateDisplay("700x500,600x500");
  SetSecondaryDisplayLayout(display::DisplayPlacement::LEFT);

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // Containing rect.
  using window_util::GetRootWindowMatching;
  EXPECT_EQ(root_windows[1],
            GetRootWindowMatching(gfx::Rect(-300, 10, 50, 50)));
  EXPECT_EQ(root_windows[0], GetRootWindowMatching(gfx::Rect(100, 10, 50, 50)));

  // Intersecting rect.
  EXPECT_EQ(root_windows[1],
            GetRootWindowMatching(gfx::Rect(-200, 0, 300, 300)));
  EXPECT_EQ(root_windows[0],
            GetRootWindowMatching(gfx::Rect(-100, 0, 300, 300)));

  // Zero origin.
  EXPECT_EQ(root_windows[0], GetRootWindowMatching(gfx::Rect(0, 0, 0, 0)));
  EXPECT_EQ(root_windows[0], GetRootWindowMatching(gfx::Rect(0, 0, 1, 1)));

  // Empty rect.
  EXPECT_EQ(root_windows[1], GetRootWindowMatching(gfx::Rect(-400, 100, 0, 0)));
  EXPECT_EQ(root_windows[0], GetRootWindowMatching(gfx::Rect(100, 100, 0, 0)));

  // Out of range rect should return the primary root window.
  EXPECT_EQ(root_windows[0],
            GetRootWindowMatching(gfx::Rect(-600, -300, 50, 50)));
  EXPECT_EQ(root_windows[0], GetRootWindowMatching(gfx::Rect(0, 1000, 50, 50)));
}

TEST_F(ExtendedDesktopTest, Capture) {
  // This test deals with input events but not visuals so don't throttle input
  // on visuals.
  aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);

  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  aura::test::EventCountDelegate r1_d1;
  aura::test::EventCountDelegate r1_d2;
  aura::test::EventCountDelegate r2_d1;

  std::unique_ptr<aura::Window> r1_w1(aura::test::CreateTestWindowWithDelegate(
      &r1_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[0]));
  std::unique_ptr<aura::Window> r1_w2(aura::test::CreateTestWindowWithDelegate(
      &r1_d2, 0, gfx::Rect(10, 100, 100, 100), root_windows[0]));
  std::unique_ptr<aura::Window> r2_w1(aura::test::CreateTestWindowWithDelegate(
      &r2_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[1]));

  r1_w1->SetCapture();

  EXPECT_EQ(r1_w1.get(),
            aura::client::GetCaptureWindow(r2_w1->GetRootWindow()));

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseToCenterOf(r2_w1.get());
  // |r1_w1| will receive the events because it has capture.
  EXPECT_EQ("1 1 0", r1_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 0", r1_d2.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 0", r2_d1.GetMouseMotionCountsAndReset());

  generator->ClickLeftButton();
  EXPECT_EQ("0 0 0", r2_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0", r2_d1.GetMouseButtonCountsAndReset());
  // The mouse is outside. On chromeos, the mouse is warped to the
  // dest root window, but it's not implemented on Win yet, so
  // no mouse move event on Win.
  EXPECT_EQ("0 0 0", r1_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1", r1_d1.GetMouseButtonCountsAndReset());

  generator->MoveMouseTo(15, 15);
  EXPECT_EQ("0 1 0", r1_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 0", r1_d2.GetMouseMotionCountsAndReset());

  r1_w2->SetCapture();
  EXPECT_EQ(r1_w2.get(),
            aura::client::GetCaptureWindow(r2_w1->GetRootWindow()));
  generator->MoveMouseBy(10, 10);
  // |r1_w2| has the capture. So it will receive the mouse-move event.
  EXPECT_EQ("0 0 0", r1_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 1 0", r1_d2.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 0", r2_d1.GetMouseMotionCountsAndReset());

  generator->ClickLeftButton();
  EXPECT_EQ("0 0 0", r2_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0", r2_d1.GetMouseButtonCountsAndReset());
  EXPECT_EQ("0 0 0", r1_d2.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1", r1_d2.GetMouseButtonCountsAndReset());

  r1_w2->ReleaseCapture();
  EXPECT_EQ(nullptr, aura::client::GetCaptureWindow(r2_w1->GetRootWindow()));

  generator->MoveMouseToCenterOf(r2_w1.get());
  generator->ClickLeftButton();
  EXPECT_EQ("1 1 0", r2_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1", r2_d1.GetMouseButtonCountsAndReset());
  // Make sure the mouse_moved_handler_ is properly reset.
  EXPECT_EQ("0 0 0", r1_d2.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0", r1_d2.GetMouseButtonCountsAndReset());
}

TEST_F(ExtendedDesktopTest, CaptureEventLocation) {
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  aura::test::EventCountDelegate r1_d1;
  aura::test::EventCountDelegate r1_d2;
  aura::test::EventCountDelegate r2_d1;

  std::unique_ptr<aura::Window> r1_w1(aura::test::CreateTestWindowWithDelegate(
      &r1_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[0]));
  std::unique_ptr<aura::Window> r1_w2(aura::test::CreateTestWindowWithDelegate(
      &r1_d2, 0, gfx::Rect(10, 100, 100, 100), root_windows[0]));
  std::unique_ptr<aura::Window> r2_w1(aura::test::CreateTestWindowWithDelegate(
      &r2_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[1]));

  r1_w1->SetCapture();

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseToCenterOf(r2_w1.get());
  EXPECT_EQ(gfx::Point(1060, 60).ToString(),
            generator->current_screen_location().ToString());

  EventLocationHandler location_handler;
  r1_w1->AddPreTargetHandler(&location_handler);
  generator->ClickLeftButton();
  r1_w1->RemovePreTargetHandler(&location_handler);
  EXPECT_EQ(gfx::Point(1050, 50).ToString(),
            location_handler.press_location().ToString());
  EXPECT_EQ(gfx::Point(1050, 50).ToString(),
            location_handler.release_location().ToString());
}

TEST_F(ExtendedDesktopTest, CaptureEventLocationHighDPI) {
  UpdateDisplay("1000x600*2,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  aura::test::EventCountDelegate r1_d1;
  aura::test::EventCountDelegate r1_d2;
  aura::test::EventCountDelegate r2_d1;

  std::unique_ptr<aura::Window> r1_w1(aura::test::CreateTestWindowWithDelegate(
      &r1_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[0]));
  std::unique_ptr<aura::Window> r1_w2(aura::test::CreateTestWindowWithDelegate(
      &r1_d2, 0, gfx::Rect(10, 100, 100, 100), root_windows[0]));
  std::unique_ptr<aura::Window> r2_w1(aura::test::CreateTestWindowWithDelegate(
      &r2_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[1]));

  r1_w1->SetCapture();

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseToCenterOf(r2_w1.get());
  EXPECT_EQ(gfx::Point(560, 60).ToString(),
            generator->current_screen_location().ToString());

  EventLocationHandler location_handler;
  r1_w1->AddPreTargetHandler(&location_handler);
  generator->ClickLeftButton();
  r1_w1->RemovePreTargetHandler(&location_handler);
  EXPECT_EQ(gfx::Point(550, 50).ToString(),
            location_handler.press_location().ToString());
  EXPECT_EQ(gfx::Point(550, 50).ToString(),
            location_handler.release_location().ToString());
}

TEST_F(ExtendedDesktopTest, CaptureEventLocationHighDPI_2) {
  UpdateDisplay("1000x600,600x400*2");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  aura::test::EventCountDelegate r1_d1;
  aura::test::EventCountDelegate r1_d2;
  aura::test::EventCountDelegate r2_d1;

  std::unique_ptr<aura::Window> r1_w1(aura::test::CreateTestWindowWithDelegate(
      &r1_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[0]));
  std::unique_ptr<aura::Window> r1_w2(aura::test::CreateTestWindowWithDelegate(
      &r1_d2, 0, gfx::Rect(10, 100, 100, 100), root_windows[0]));
  std::unique_ptr<aura::Window> r2_w1(aura::test::CreateTestWindowWithDelegate(
      &r2_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[1]));

  r1_w1->SetCapture();

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseToCenterOf(r2_w1.get());
  EXPECT_EQ(gfx::Point(1060, 60).ToString(),
            generator->current_screen_location().ToString());

  EventLocationHandler location_handler;
  r1_w1->AddPreTargetHandler(&location_handler);
  generator->ClickLeftButton();
  r1_w1->RemovePreTargetHandler(&location_handler);
  // Event-generator dispatches the event in the primary root-window's coord
  // space. Since the location is (1060, 60), it goes to the secondary
  // root-window as (30, 30) since the secondary root-window has a device scale
  // factor of 2.
  EXPECT_EQ(gfx::Point(1020, 20).ToString(),
            location_handler.press_location().ToString());
  EXPECT_EQ(gfx::Point(1020, 20).ToString(),
            location_handler.release_location().ToString());
}

TEST_F(ExtendedDesktopTest, MoveWindow) {
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  views::Widget* d1 = CreateTestWidget(gfx::Rect(10, 10, 100, 100));

  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());

  d1->SetBounds(gfx::Rect(1010, 10, 100, 100));
  EXPECT_EQ("1010,10 100x100", d1->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[1], d1->GetNativeView()->GetRootWindow());

  d1->SetBounds(gfx::Rect(10, 10, 100, 100));
  EXPECT_EQ("10,10 100x100", d1->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());

  // Make sure the bounds which doesn't fit to the root window
  // works correctly.
  d1->SetBounds(gfx::Rect(1560, 30, 100, 100));
  EXPECT_EQ(root_windows[1], d1->GetNativeView()->GetRootWindow());
  EXPECT_EQ("1560,30 100x100", d1->GetWindowBoundsInScreen().ToString());

  // Setting outside of root windows will be moved to primary root window.
  // TODO(oshima): This one probably should pick the closest root window.
  d1->SetBounds(gfx::Rect(200, 10, 100, 100));
  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());
}

// Verifies if the mouse event arrives to the window even when the window
// moves to another root in a pre-target handler.  See: crbug.com/157583
TEST_F(ExtendedDesktopTest, MoveWindowByMouseClick) {
  UpdateDisplay("1000x600,600x400");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::test::EventCountDelegate delegate;
  std::unique_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(10, 10, 100, 100), root_windows[0]));
  MoveWindowByClickEventHandler event_handler(window.get());
  window->AddPreTargetHandler(&event_handler);

  ui::test::EventGenerator* event_generator = GetEventGenerator();

  event_generator->MoveMouseToCenterOf(window.get());
  event_generator->ClickLeftButton();
  // Both mouse pressed and released arrive at the window and its delegate.
  EXPECT_EQ("1 1", delegate.GetMouseButtonCountsAndReset());
  // Also event_handler moves the window to another root at mouse release.
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
}

TEST_F(ExtendedDesktopTest, MoveWindowToDisplay) {
  UpdateDisplay("1000x900,1000x900");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  display::Display display0 = display::Screen::GetScreen()->GetDisplayMatching(
      root_windows[0]->GetBoundsInScreen());
  display::Display display1 = display::Screen::GetScreen()->GetDisplayMatching(
      root_windows[1]->GetBoundsInScreen());
  EXPECT_NE(display0.id(), display1.id());

  views::Widget* d1 = CreateTestWidget(gfx::Rect(10, 10, 1000, 100));
  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());

  // Move the window where the window spans both root windows. Since the second
  // parameter is |display1|, the window should be shown on the secondary root.
  d1->GetNativeWindow()->SetBoundsInScreen(gfx::Rect(500, 10, 1000, 100),
                                           display1);
  EXPECT_EQ("500,10 1000x100", d1->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(root_windows[1], d1->GetNativeView()->GetRootWindow());

  // Move to the primary root.
  d1->GetNativeWindow()->SetBoundsInScreen(gfx::Rect(500, 10, 1000, 100),
                                           display0);
  EXPECT_EQ("500,10 1000x100", d1->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());
}

TEST_F(ExtendedDesktopTest, MoveWindowWithTransient) {
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  // Create and activate a normal window |w1|.
  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 10, 100, 100));
  wm::ActivateWindow(w1);
  // |w1_t1| is a transient child window of |w1|.
  std::unique_ptr<aura::Window> w1_t1 =
      ChildTestWindowBuilder(w1, gfx::Rect(50, 50, 50, 50),
                             desks_util::GetActiveDeskContainerId())
          .Build();
  ::wm::AddTransientChild(w1, w1_t1.get());
  // |w1_t11| is a transient child window of transient child window |w1_t1|.
  std::unique_ptr<aura::Window> w1_t11 =
      ChildTestWindowBuilder(w1_t1.get(), gfx::Rect(2, 7, 35, 35),
                             desks_util::GetActiveDeskContainerId())
          .Build();
  ::wm::AddTransientChild(w1_t1.get(), w1_t11.get());

  // |w11| is a non-transient child window of |w1|.
  std::unique_ptr<aura::Window> w11 =
      ChildTestWindowBuilder(w1, gfx::Rect(10, 10, 40, 40),
                             desks_util::GetActiveDeskContainerId())
          .Build();
  // |w11_t1| is a transient child window of |w11|.
  std::unique_ptr<aura::Window> w11_t1 =
      ChildTestWindowBuilder(w11.get(), gfx::Rect(30, 10, 80, 80),
                             desks_util::GetActiveDeskContainerId())
          .Build();
  ::wm::AddTransientChild(w11.get(), w11_t1.get());

  EXPECT_EQ(root_windows[0], w1->GetRootWindow());
  EXPECT_EQ(root_windows[0], w1_t1->GetRootWindow());
  EXPECT_EQ(root_windows[0], w1_t11->GetRootWindow());
  EXPECT_EQ(root_windows[0], w11->GetRootWindow());
  EXPECT_EQ(root_windows[0], w11_t1->GetRootWindow());
  EXPECT_EQ("10,10 100x100", w1->GetBoundsInScreen().ToString());
  EXPECT_EQ("60,60 50x50", w1_t1->GetBoundsInScreen().ToString());
  EXPECT_EQ("62,67 35x35", w1_t11->GetBoundsInScreen().ToString());
  EXPECT_EQ("20,20 40x40", w11->GetBoundsInScreen().ToString());
  EXPECT_EQ("50,30 80x80", w11_t1->GetBoundsInScreen().ToString());

  w1->SetBoundsInScreen(gfx::Rect(1200, 10, 100, 100), GetSecondaryDisplay());

  EXPECT_EQ(root_windows[1], w1->GetRootWindow());
  EXPECT_EQ(root_windows[1], w1_t1->GetRootWindow());
  EXPECT_EQ(root_windows[1], w1_t11->GetRootWindow());
  EXPECT_EQ(root_windows[1], w11->GetRootWindow());
  EXPECT_EQ(root_windows[1], w11_t1->GetRootWindow());

  EXPECT_EQ("1200,10 100x100", w1->GetBoundsInScreen().ToString());
  EXPECT_EQ("1250,60 50x50", w1_t1->GetBoundsInScreen().ToString());
  EXPECT_EQ("1252,67 35x35", w1_t11->GetBoundsInScreen().ToString());
  EXPECT_EQ("1210,20 40x40", w11->GetBoundsInScreen().ToString());
  EXPECT_EQ("1240,30 80x80", w11_t1->GetBoundsInScreen().ToString());
}

// Test transient child is parented after its transient parent moved to another
// root window.
TEST_F(ExtendedDesktopTest, PostMoveParentTransientChild) {
  UpdateDisplay("600x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  // Create and activate a normal window.
  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 10, 100, 100));
  wm::ActivateWindow(window);
  // Create a transient child window of |window| without parenting to |window|
  // yet.
  std::unique_ptr<aura::Window> child = std::make_unique<aura::Window>(nullptr);
  child->SetType(aura::client::WINDOW_TYPE_NORMAL);
  child->Init(ui::LAYER_TEXTURED);
  child->SetBounds(gfx::Rect(50, 50, 50, 50));
  child->Show();
  ::wm::AddTransientChild(window, child.get());

  // Move |window| to another display and ensure that crash doesn't happen.
  window->SetBoundsInScreen(gfx::Rect(610, 10, 100, 100),
                            GetSecondaryDisplay());

  // Parent |child| to |window| now.
  window->AddChild(child.get());

  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ(root_windows[1], child->GetRootWindow());
  EXPECT_EQ("610,10 100x100", window->GetBoundsInScreen().ToString());
  EXPECT_EQ("660,60 50x50", child->GetBoundsInScreen().ToString());
}

// Test if the Window::ConvertPointToTarget works across root windows.
TEST_F(ExtendedDesktopTest, ConvertPoint) {
  display::Screen* screen = display::Screen::GetScreen();
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  display::Display display_1 = screen->GetDisplayNearestWindow(root_windows[0]);
  EXPECT_EQ("0,0", display_1.bounds().origin().ToString());
  display::Display display_2 = screen->GetDisplayNearestWindow(root_windows[1]);
  EXPECT_EQ("1000,0", display_2.bounds().origin().ToString());

  aura::Window* d1 =
      CreateTestWidget(gfx::Rect(10, 10, 100, 100))->GetNativeView();
  aura::Window* d2 =
      CreateTestWidget(gfx::Rect(1020, 20, 100, 100))->GetNativeView();
  EXPECT_EQ(root_windows[0], d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], d2->GetRootWindow());

  // Convert point in the Root2's window to the Root1's window Coord.
  gfx::Point p(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[1], root_windows[0], &p);
  EXPECT_EQ("1000,0", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d2, d1, &p);
  EXPECT_EQ("1010,10", p.ToString());

  // Convert point in the Root1's window to the Root2's window Coord.
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[0], root_windows[1], &p);
  EXPECT_EQ("-1000,0", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d1, d2, &p);
  EXPECT_EQ("-1010,-10", p.ToString());

  // Move the 2nd display to the bottom and test again.
  SetSecondaryDisplayLayout(display::DisplayPlacement::BOTTOM);

  display_2 = screen->GetDisplayNearestWindow(root_windows[1]);
  EXPECT_EQ("0,600", display_2.bounds().origin().ToString());

  // Convert point in Root2's window to Root1's window Coord.
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[1], root_windows[0], &p);
  EXPECT_EQ("0,600", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d2, d1, &p);
  EXPECT_EQ("10,610", p.ToString());

  // Convert point in Root1's window to Root2's window Coord.
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[0], root_windows[1], &p);
  EXPECT_EQ("0,-600", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d1, d2, &p);
  EXPECT_EQ("-10,-610", p.ToString());
}

TEST_F(ExtendedDesktopTest, OpenSystemTray) {
  // Two displays side by side and both are high enough for tray bubble.
  UpdateDisplay("500x600,600x700");
  ASSERT_FALSE(IsBubbleShown());

  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Opens the tray by a dummy click event and makes sure that adding/removing
  // displays doesn't break anything.
  event_generator->MoveMouseTo(GetSystemTrayBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_TRUE(IsBubbleShown());

  // Verifies that the bubble is within the primary display.
  const gfx::Rect primary_display_bounds = GetPrimaryDisplay().bounds();
  const gfx::Rect tray_bubble_bounds =
      GetPrimaryUnifiedSystemTray()->GetBubbleBoundsInScreen();
  EXPECT_TRUE(primary_display_bounds.Contains(tray_bubble_bounds))
      << "primary display bounds=" << primary_display_bounds.ToString()
      << ", tray bubble bounds=" << tray_bubble_bounds.ToString();

  UpdateDisplay("500x600");
  EXPECT_TRUE(IsBubbleShown());
  UpdateDisplay("500x600,600x700");
  EXPECT_TRUE(IsBubbleShown());

  // Closes the tray and again makes sure that adding/removing displays doesn't
  // break anything.
  event_generator->ClickLeftButton();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsBubbleShown());

  UpdateDisplay("500x600");
  EXPECT_FALSE(IsBubbleShown());
  UpdateDisplay("500x600,600x700");
  EXPECT_FALSE(IsBubbleShown());
}

TEST_F(ExtendedDesktopTest, StayInSameRootWindow) {
  UpdateDisplay("200x100,300x200");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  views::Widget* w1 = CreateTestWidget(gfx::Rect(10, 10, 50, 50));
  EXPECT_EQ(root_windows[0], w1->GetNativeView()->GetRootWindow());
  w1->SetBounds(gfx::Rect(250, 10, 50, 50));
  EXPECT_EQ(root_windows[1], w1->GetNativeView()->GetRootWindow());

  // The widget stays in the same root if kLockedToRootKey is set to true.
  w1->GetNativeView()->SetProperty(kLockedToRootKey, true);
  w1->SetBounds(gfx::Rect(10, 10, 50, 50));
  EXPECT_EQ(root_windows[1], w1->GetNativeView()->GetRootWindow());

  // The widget should now move to the 1st root window without the property.
  w1->GetNativeView()->ClearProperty(kLockedToRootKey);
  w1->SetBounds(gfx::Rect(10, 10, 50, 50));
  EXPECT_EQ(root_windows[0], w1->GetNativeView()->GetRootWindow());

  // a window in SettingsBubbleContainer, ShelfControlContainer and
  // OverviewFocusContainer should not move to another root window
  // regardless of the bounds specified.
  aura::Window* settings_bubble_container =
      Shell::GetPrimaryRootWindowController()->GetContainer(
          kShellWindowId_SettingBubbleContainer);
  aura::Window* window =
      aura::test::CreateTestWindowWithId(100, settings_bubble_container);
  window->SetBoundsInScreen(gfx::Rect(250, 10, 50, 50), GetSecondaryDisplay());
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  aura::Window* status_container =
      Shell::GetPrimaryRootWindowController()->GetContainer(
          kShellWindowId_ShelfContainer);
  window = aura::test::CreateTestWindowWithId(100, status_container);
  window->SetBoundsInScreen(gfx::Rect(250, 10, 50, 50), GetSecondaryDisplay());
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
}

TEST_F(ExtendedDesktopTest, KeyEventsOnLockScreen) {
  UpdateDisplay("200x100,300x200");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // Create normal windows on both displays.
  views::Widget* widget1 = CreateTestWidget(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  widget1->Show();
  EXPECT_EQ(root_windows[0], widget1->GetNativeView()->GetRootWindow());
  views::Widget* widget2 = CreateTestWidget(GetSecondaryDisplay().bounds());
  widget2->Show();
  EXPECT_EQ(root_windows[1], widget2->GetNativeView()->GetRootWindow());

  // Create a LockScreen window.
  views::Widget* lock_widget = CreateTestWidget(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  views::Textfield* textfield = new views::Textfield;
  lock_widget->client_view()->AddChildView(textfield);

  Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                      kShellWindowId_LockScreenContainer)
      ->AddChild(lock_widget->GetNativeView());
  lock_widget->Show();
  textfield->RequestFocus();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(root_windows[0]);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());

  // The lock window should get events on both root windows.
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  event_generator->set_current_target(root_windows[0]);
  event_generator->PressAndReleaseKey(ui::VKEY_A);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());
  EXPECT_EQ("a", base::UTF16ToASCII(textfield->GetText()));

  event_generator->set_current_target(root_windows[1]);
  event_generator->PressAndReleaseKey(ui::VKEY_B);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());
  EXPECT_EQ("ab", base::UTF16ToASCII(textfield->GetText()));

  // Deleting 2nd display. The lock window still should get the events.
  UpdateDisplay("200x100");
  event_generator->set_current_target(root_windows[0]);
  event_generator->PressAndReleaseKey(ui::VKEY_C);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());
  EXPECT_EQ("abc", base::UTF16ToASCII(textfield->GetText()));

  // Creating 2nd display again, and lock window still should get events
  // on both root windows.
  UpdateDisplay("200x100,300x200");
  root_windows = Shell::GetAllRootWindows();
  event_generator->set_current_target(root_windows[0]);
  event_generator->PressAndReleaseKey(ui::VKEY_D);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());
  EXPECT_EQ("abcd", base::UTF16ToASCII(textfield->GetText()));

  event_generator->set_current_target(root_windows[1]);
  event_generator->PressAndReleaseKey(ui::VKEY_E);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());
  EXPECT_EQ("abcde", base::UTF16ToASCII(textfield->GetText()));
}

// Verifies that clicking in the primary display and dragging to the secondary
// display correctly sets the event location (window local) and root_location
// (in screen coordinates) for the mouse event. https://crrev.com/11691010
TEST_F(ExtendedDesktopTest, PassiveGrab) {
  // This test deals with input events but not visuals so don't throttle input
  // on visuals.
  aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);
  EventLocationRecordingEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  // Create large displays so the widget won't be moved after creation.
  // https://crbug.com/890633
  UpdateDisplay("1000x900,1000x900");

  views::Widget* widget = CreateTestWidget(gfx::Rect(50, 50, 200, 200));
  widget->Show();
  ASSERT_EQ("50,50 200x200", widget->GetWindowBoundsInScreen().ToString());

  // Move the mouse to the center of the window.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(150, 150);

  // Location is relative to the window.
  EXPECT_EQ("100,100", event_handler.GetLocation());

  // Root location is relative to the display.
  EXPECT_EQ("150,150", event_handler.GetRootLocation());
  event_handler.Reset();

  // Drag the mouse to the secondary display. Does not drag the window.
  generator->PressLeftButton();
  generator->MoveMouseBy(1000, 0);

  // Location is relative to the original window.
  EXPECT_EQ("1100,100", event_handler.GetLocation());

  // Root location is relative to the original display.
  EXPECT_EQ("1150,150", event_handler.GetRootLocation());
  event_handler.Reset();

  // End the drag.
  generator->ReleaseLeftButton();
  EXPECT_EQ("-999,-999", event_handler.GetRootLocation());
  EXPECT_EQ("-999,-999", event_handler.GetLocation());
  event_handler.Reset();

  // Move the mouse to the top-left of the secondary display.
  generator->MoveMouseTo(1001, 1);

  // Location is relative to the new display.
  EXPECT_EQ("1,1", event_handler.GetLocation());

  // Root location is relative to the new display.
  EXPECT_EQ("1,1", event_handler.GetRootLocation());
  event_handler.Reset();

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

}  // namespace ash
