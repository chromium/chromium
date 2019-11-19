// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/screenshot_controller.h"

#include "ash/display/cursor_window_controller.h"
#include "ash/display/mirror_window_test_api.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_screenshot_delegate.h"
#include "ash/wm/window_util.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class ScreenshotControllerTest : public AshTestBase {
 public:
  ScreenshotControllerTest() = default;
  ~ScreenshotControllerTest() override = default;

 protected:
  ScreenshotController* screenshot_controller() {
    return Shell::Get()->screenshot_controller();
  }

  bool TestIfMouseWarpsAt(const gfx::Point& point_in_screen) {
    return AshTestBase::TestIfMouseWarpsAt(GetEventGenerator(),
                                           point_in_screen);
  }

  void StartPartialScreenshotSession() {
    screenshot_controller()->StartPartialScreenshotSession(true);
  }

  void StartWindowScreenshotSession() {
    screenshot_controller()->StartWindowScreenshotSession();
  }

  void Cancel() { screenshot_controller()->CancelScreenshotSession(); }

  bool IsActive() { return screenshot_controller()->in_screenshot_session_; }

  const gfx::Point& GetStartPosition() const {
    return Shell::Get()->screenshot_controller()->start_position_;
  }

  const aura::Window* GetCurrentSelectedWindow() const {
    return Shell::Get()->screenshot_controller()->selected_;
  }

  aura::Window* CreateSelectableWindow(const gfx::Rect& rect) {
    return CreateTestWindowInShell(SK_ColorGRAY, 0, rect);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenshotControllerTest);
};

using WindowScreenshotControllerTest = ScreenshotControllerTest;
using PartialScreenshotControllerTest = ScreenshotControllerTest;

TEST_F(PartialScreenshotControllerTest, BasicMouse) {
  StartPartialScreenshotSession();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseTo(100, 100);
  generator.PressLeftButton();
  EXPECT_EQ(gfx::Point(100, 100), GetStartPosition());
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  generator.MoveMouseTo(200, 200);
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  generator.ReleaseLeftButton();
  EXPECT_EQ(gfx::Rect(100, 100, 100, 100),
            GetScreenshotDelegate()->last_rect());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

// Starting the screenshot session while mouse is pressed, releasing the mouse
// without moving it used to cause a crash. Make sure this doesn't happen again.
// crbug.com/581432.
TEST_F(PartialScreenshotControllerTest, StartSessionWhileMousePressed) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();

  generator.MoveMouseTo(100, 100);
  generator.PressLeftButton();

  // The following used to cause a crash. Now it should remain in the
  // screenshot session.
  StartPartialScreenshotSession();
  generator.ReleaseLeftButton();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_TRUE(IsActive());

  // Pressing again, moving, and releasing should take a screenshot.
  generator.PressLeftButton();
  generator.MoveMouseTo(200, 200);
  generator.ReleaseLeftButton();
  EXPECT_EQ(1, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_FALSE(IsActive());

  // Starting the screenshot session while mouse is pressed, moving the mouse
  // and releasing should take a screenshot normally.
  generator.MoveMouseTo(100, 100);
  generator.PressLeftButton();
  StartPartialScreenshotSession();
  generator.MoveMouseTo(150, 150);
  generator.MoveMouseTo(200, 200);
  EXPECT_TRUE(IsActive());
  generator.ReleaseLeftButton();
  EXPECT_EQ(2, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_FALSE(IsActive());
}

TEST_F(PartialScreenshotControllerTest, JustClick) {
  StartPartialScreenshotSession();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseTo(100, 100);

  // No moves, just clicking at the same position.
  generator.ClickLeftButton();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

TEST_F(PartialScreenshotControllerTest, BasicTouch) {
  StartPartialScreenshotSession();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.PressTouch();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(100, 100), GetStartPosition());

  generator.MoveTouch(gfx::Point(200, 200));
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  generator.ReleaseTouch();
  EXPECT_EQ(gfx::Rect(100, 100, 100, 100),
            GetScreenshotDelegate()->last_rect());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

// Verifies that pointer events can be used to take a screenshot when
// pointer-only move is set to true. Verifies that pointer-only mode is
// automatically reset to false.
TEST_F(PartialScreenshotControllerTest,
       PointerEventsWorkWhenPointerOnlyActive) {
  StartPartialScreenshotSession();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  screenshot_controller()->set_pen_events_only(true);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.EnterPenPointerMode();
  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.PressTouch();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(100, 100), GetStartPosition());

  generator.MoveTouch(gfx::Point(300, 300));
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  generator.ReleaseTouch();
  EXPECT_EQ(gfx::Rect(100, 100, 200, 200),
            GetScreenshotDelegate()->last_rect());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());
  EXPECT_FALSE(screenshot_controller()->pen_events_only());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

// Verifies that only pointer press/release events can be used to take a
// screenshot when pointer only mode is active.
TEST_F(PartialScreenshotControllerTest,
       TouchMousePointerHoverIgnoredWithPointerEvents) {
  StartPartialScreenshotSession();
  screenshot_controller()->set_pen_events_only(true);
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.set_current_screen_location(gfx::Point(100, 100));

  // Verify touch is ignored.
  generator.PressTouch();
  generator.MoveTouch(gfx::Point(50, 50));
  generator.ReleaseTouch();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(0, 0), GetStartPosition());

  // Verify mouse is ignored.
  generator.DragMouseBy(10, 10);
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(0, 0), GetStartPosition());

  // Verify pointer enter/exit is ignored.
  generator.EnterPenPointerMode();
  generator.SendMouseEnter();
  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.SendMouseExit();
  generator.ExitPenPointerMode();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(0, 0), GetStartPosition());

  Cancel();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

TEST_F(PartialScreenshotControllerTest, TwoFingerTouch) {
  StartPartialScreenshotSession();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.PressTouch();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(100, 100), GetStartPosition());

  generator.set_current_screen_location(gfx::Point(200, 200));
  generator.PressTouchId(1);
  EXPECT_EQ(gfx::Rect(100, 100, 100, 100),
            GetScreenshotDelegate()->last_rect());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

// Make sure ScreenshotController doesn't allow taking screenshot
// across multiple monitors
// cursor. See http://crbug.com/462229
TEST_F(PartialScreenshotControllerTest, MouseWarpTest) {
  // Create two displays.
  Shell* shell = Shell::Get();
  UpdateDisplay("500x500,500x500");
  EXPECT_EQ(2U, shell->display_manager()->GetNumDisplays());

  StartPartialScreenshotSession();
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ(gfx::Point(499, 11),
            aura::Env::GetInstance()->last_mouse_location());

  Cancel();
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ(gfx::Point(501, 11),
            aura::Env::GetInstance()->last_mouse_location());
}

TEST_F(PartialScreenshotControllerTest, CursorVisibilityTest) {
  aura::client::CursorClient* client = Shell::Get()->cursor_manager();

  GetEventGenerator()->PressKey(ui::VKEY_A, 0);
  GetEventGenerator()->ReleaseKey(ui::VKEY_A, 0);

  EXPECT_FALSE(client->IsCursorVisible());

  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(client->IsCursorVisible());

  // Platform's Cursor should be hidden while dragging.
  GetEventGenerator()->PressLeftButton();
  EXPECT_TRUE(IsActive());
  EXPECT_FALSE(client->IsCursorVisible());

  Cancel();
  EXPECT_TRUE(client->IsCursorVisible());
}

// Make sure ScreenshotController doesn't prevent handling of large
// cursor. See http://crbug.com/459214
TEST_F(PartialScreenshotControllerTest, LargeCursor) {
  Shell::Get()->cursor_manager()->SetCursorSize(ui::CursorSize::kLarge);
  Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->SetCursorCompositingEnabled(true);

  // Large cursor is represented as cursor window.
  MirrorWindowTestApi test_api;
  ASSERT_NE(nullptr, test_api.GetCursorWindow());

  ui::test::EventGenerator event_generator(Shell::GetPrimaryRootWindow());
  gfx::Point cursor_location;
  event_generator.MoveMouseTo(cursor_location);
  EXPECT_EQ(cursor_location, test_api.GetCursorLocation());

  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());

  cursor_location += gfx::Vector2d(1, 1);
  event_generator.MoveMouseTo(cursor_location);
  EXPECT_EQ(cursor_location, test_api.GetCursorLocation());

  event_generator.PressLeftButton();
  cursor_location += gfx::Vector2d(5, 5);
  event_generator.MoveMouseTo(cursor_location);
  EXPECT_EQ(cursor_location, test_api.GetCursorLocation());

  event_generator.ReleaseLeftButton();

  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Rect(1, 1, 5, 5), GetScreenshotDelegate()->last_rect());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

TEST_F(WindowScreenshotControllerTest, KeyboardOperation) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();

  StartWindowScreenshotSession();
  generator->PressKey(ui::VKEY_ESCAPE, 0);
  generator->ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());

  StartWindowScreenshotSession();
  generator->PressKey(ui::VKEY_RETURN, 0);
  generator->ReleaseKey(ui::VKEY_RETURN, 0);
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());

  std::unique_ptr<aura::Window> window1(
      CreateSelectableWindow(gfx::Rect(5, 5, 20, 20)));
  wm::ActivateWindow(window1.get());
  StartWindowScreenshotSession();
  generator->PressKey(ui::VKEY_ESCAPE, 0);
  generator->ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());

  StartWindowScreenshotSession();
  generator->PressKey(ui::VKEY_RETURN, 0);
  generator->ReleaseKey(ui::VKEY_RETURN, 0);
  EXPECT_FALSE(IsActive());
  EXPECT_EQ(window1.get(), test_delegate->GetSelectedWindowAndReset());
  // Make sure it's properly reset.
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());
}

TEST_F(WindowScreenshotControllerTest, MouseOperation) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  StartWindowScreenshotSession();
  EXPECT_TRUE(IsActive());
  generator->ClickLeftButton();
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());

  std::unique_ptr<aura::Window> window1(
      CreateSelectableWindow(gfx::Rect(5, 5, 20, 20)));
  std::unique_ptr<aura::Window> window2(
      CreateSelectableWindow(gfx::Rect(100, 100, 100, 100)));
  wm::ActivateWindow(window1.get());
  StartWindowScreenshotSession();
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->MoveMouseTo(150, 150);
  EXPECT_EQ(window2.get(), GetCurrentSelectedWindow());
  generator->MoveMouseTo(400, 0);
  EXPECT_FALSE(GetCurrentSelectedWindow());
  generator->MoveMouseTo(10, 10);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->ClickLeftButton();
  EXPECT_EQ(window1.get(), test_delegate->GetSelectedWindowAndReset());

  // Window selection should work even with Capture.
  window2->SetCapture();
  wm::ActivateWindow(window2.get());
  StartWindowScreenshotSession();
  EXPECT_EQ(window2.get(), GetCurrentSelectedWindow());
  generator->MoveMouseTo(10, 10);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->MoveMouseTo(400, 0);
  EXPECT_FALSE(GetCurrentSelectedWindow());
  generator->MoveMouseTo(10, 10);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->ClickLeftButton();
  EXPECT_EQ(window1.get(), test_delegate->GetSelectedWindowAndReset());

  // Remove window.
  StartWindowScreenshotSession();
  generator->MoveMouseTo(10, 10);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  window1.reset();
  EXPECT_FALSE(GetCurrentSelectedWindow());
  generator->ClickLeftButton();
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());
}

TEST_F(WindowScreenshotControllerTest, MultiDisplays) {
  UpdateDisplay("400x400,500x500");

  ui::test::EventGenerator* generator = GetEventGenerator();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();

  std::unique_ptr<aura::Window> window1(
      CreateSelectableWindow(gfx::Rect(100, 100, 100, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateSelectableWindow(gfx::Rect(600, 200, 100, 100)));
  EXPECT_NE(window1.get()->GetRootWindow(), window2.get()->GetRootWindow());

  StartWindowScreenshotSession();
  generator->MoveMouseTo(150, 150);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->MoveMouseTo(650, 250);
  EXPECT_EQ(window2.get(), GetCurrentSelectedWindow());
  generator->ClickLeftButton();
  EXPECT_EQ(window2.get(), test_delegate->GetSelectedWindowAndReset());

  window2->SetCapture();
  wm::ActivateWindow(window2.get());
  StartWindowScreenshotSession();
  generator->MoveMouseTo(150, 150);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->ClickLeftButton();
  EXPECT_EQ(window1.get(), test_delegate->GetSelectedWindowAndReset());
}

TEST_F(ScreenshotControllerTest, MultipleDisplays) {
  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());
  UpdateDisplay("400x400,500x500");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());

  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());
  UpdateDisplay("400x400");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());

  StartWindowScreenshotSession();
  EXPECT_TRUE(IsActive());
  UpdateDisplay("400x400,500x500");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());

  StartWindowScreenshotSession();
  EXPECT_TRUE(IsActive());
  UpdateDisplay("400x400");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

// Windows that take capture can misbehave due to a screenshot session. Break
// mouse capture when the screenshot session is over. See crbug.com/651939
TEST_F(ScreenshotControllerTest, BreaksCapture) {
  std::unique_ptr<aura::Window> window(
      CreateSelectableWindow(gfx::Rect(100, 100, 100, 100)));
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());
  StartWindowScreenshotSession();
  EXPECT_TRUE(window->HasCapture());
  Cancel();
  EXPECT_FALSE(window->HasCapture());
}

}  // namespace ash
