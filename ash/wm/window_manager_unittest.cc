// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/test/test_activation_delegate.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/activation_delegate.h"
#include "ui/wm/test/testing_cursor_client_observer.h"

namespace {

// A slightly changed TestEventHandler which can be configured to return a
// specified value for key/mouse event handling.
class CustomEventHandler : public ui::test::TestEventHandler {
 public:
  CustomEventHandler()
      : key_result_(ui::ER_UNHANDLED), mouse_result_(ui::ER_UNHANDLED) {}

  CustomEventHandler(const CustomEventHandler&) = delete;
  CustomEventHandler& operator=(const CustomEventHandler&) = delete;

  ~CustomEventHandler() override = default;

  void set_key_event_handling_result(ui::EventResult result) {
    key_result_ = result;
  }

  void set_mouse_event_handling_result(ui::EventResult result) {
    mouse_result_ = result;
  }

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    ui::test::TestEventHandler::OnKeyEvent(event);
    if (key_result_ & ui::ER_HANDLED)
      event->SetHandled();
    if (key_result_ & ui::ER_CONSUMED)
      event->StopPropagation();
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    ui::test::TestEventHandler::OnMouseEvent(event);
    if (mouse_result_ & ui::ER_HANDLED)
      event->SetHandled();
    if (mouse_result_ & ui::ER_CONSUMED)
      event->StopPropagation();
  }

 private:
  ui::EventResult key_result_;
  ui::EventResult mouse_result_;
};

}  // namespace

namespace ash {

class WindowManagerTest : public AshTestBase {
 public:
  WindowManagerTest() = default;

  WindowManagerTest(const WindowManagerTest&) = delete;
  WindowManagerTest& operator=(const WindowManagerTest&) = delete;

  ~WindowManagerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Shell hides the cursor by default; show it for these tests.
    Shell::Get()->cursor_manager()->ShowCursor();
  }
};

TEST_F(WindowManagerTest, Focus) {
  // The IME event filter interferes with the basic key event propagation we
  // attempt to do here, so we disable it.
  //
  DisableIME();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  root_window->SetBounds(gfx::Rect(0, 0, 510, 510));

  // Supplied ids are negative so as not to collide with shell ids.
  // TODO(beng): maybe introduce a MAKE_SHELL_ID() macro that generates a safe
  //             id beyond shell id max?
  std::unique_ptr<aura::Window> w1 = TestWindowBuilder()
                                         .SetColorWindowDelegate(SK_ColorWHITE)
                                         .SetBounds(gfx::Rect(10, 10, 500, 500))
                                         .Build();
  std::unique_ptr<aura::Window> w11(aura::test::CreateTestWindow(
      SK_ColorGREEN, -11, gfx::Rect(5, 5, 100, 100), w1.get()));
  std::unique_ptr<aura::Window> w111(aura::test::CreateTestWindow(
      SK_ColorCYAN, -111, gfx::Rect(5, 5, 75, 75), w11.get()));
  std::unique_ptr<aura::Window> w1111(aura::test::CreateTestWindow(
      SK_ColorRED, -1111, gfx::Rect(5, 5, 50, 50), w111.get()));
  std::unique_ptr<aura::Window> w12(aura::test::CreateTestWindow(
      SK_ColorMAGENTA, -12, gfx::Rect(10, 420, 25, 25), w1.get()));
  aura::test::ColorTestWindowDelegate* w121delegate =
      new aura::test::ColorTestWindowDelegate(SK_ColorYELLOW);
  std::unique_ptr<aura::Window> w121(aura::test::CreateTestWindowWithDelegate(
      w121delegate, -121, gfx::Rect(5, 5, 5, 5), w12.get()));
  aura::test::ColorTestWindowDelegate* w122delegate =
      new aura::test::ColorTestWindowDelegate(SK_ColorRED);
  std::unique_ptr<aura::Window> w122(aura::test::CreateTestWindowWithDelegate(
      w122delegate, -122, gfx::Rect(10, 5, 5, 5), w12.get()));
  aura::test::ColorTestWindowDelegate* w123delegate =
      new aura::test::ColorTestWindowDelegate(SK_ColorRED);
  std::unique_ptr<aura::Window> w123(aura::test::CreateTestWindowWithDelegate(
      w123delegate, -123, gfx::Rect(15, 5, 5, 5), w12.get()));
  std::unique_ptr<aura::Window> w13(aura::test::CreateTestWindow(
      SK_ColorGRAY, -13, gfx::Rect(5, 470, 50, 50), w1.get()));

  // Click on a sub-window (w121) to focus it.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), w121.get());
  generator.ClickLeftButton();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(w121.get());
  EXPECT_EQ(w121.get(), focus_client->GetFocusedWindow());

  ui::EventSink* sink = root_window->GetHost()->GetEventSink();

  // The key press should be sent to the focused sub-window.
  ui::KeyEvent keyev(ui::EventType::kKeyPressed, ui::VKEY_E, ui::EF_NONE);
  ui::EventDispatchDetails details = sink->OnEventFromSource(&keyev);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(ui::VKEY_E, w121delegate->last_key_code());

  // Touch on a sub-window (w122) to focus it.
  gfx::Point click_point = w122->bounds().CenterPoint();
  aura::Window::ConvertPointToTarget(w122->parent(), root_window, &click_point);
  ui::TouchEvent touchev(ui::EventType::kTouchPressed, click_point,
                         ui::EventTimeForNow(),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  details = sink->OnEventFromSource(&touchev);
  ASSERT_FALSE(details.dispatcher_destroyed);
  focus_client = aura::client::GetFocusClient(w122.get());
  EXPECT_EQ(w122.get(), focus_client->GetFocusedWindow());

  // The key press should be sent to the focused sub-window.
  keyev = ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_E, ui::EF_NONE);
  details = sink->OnEventFromSource(&keyev);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(ui::VKEY_E, w122delegate->last_key_code());

  // Hiding the focused window will set the focus to its parent if
  // it's focusable.
  w122->Hide();
  EXPECT_EQ(aura::client::GetFocusClient(w12.get()),
            aura::client::GetFocusClient(w122.get()));
  EXPECT_EQ(w12.get(),
            aura::client::GetFocusClient(w12.get())->GetFocusedWindow());

  // Sets the focus back to w122.
  w122->Show();
  w122->Focus();
  EXPECT_EQ(w122.get(),
            aura::client::GetFocusClient(w12.get())->GetFocusedWindow());

  // Removing the focused window from parent should set the focus to
  // its parent if it's focusable.
  w12->RemoveChild(w122.get());
  EXPECT_EQ(NULL, aura::client::GetFocusClient(w122.get()));
  EXPECT_EQ(w12.get(),
            aura::client::GetFocusClient(w12.get())->GetFocusedWindow());

  // Set the focus to w123, but make the w1 not activatable.
  TestActivationDelegate activation_delegate(false);
  w123->Focus();
  EXPECT_EQ(w123.get(),
            aura::client::GetFocusClient(w12.get())->GetFocusedWindow());
  ::wm::SetActivationDelegate(w1.get(), &activation_delegate);

  // Hiding the focused window will set the focus to NULL because
  // parent window is not focusable.
  w123->Hide();
  EXPECT_EQ(aura::client::GetFocusClient(w12.get()),
            aura::client::GetFocusClient(w123.get()));
  EXPECT_EQ(NULL, aura::client::GetFocusClient(w12.get())->GetFocusedWindow());
  keyev = ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_E, ui::EF_NONE);
  details = sink->OnEventFromSource(&keyev);
  EXPECT_FALSE(keyev.handled() || details.dispatcher_destroyed);

  // Set the focus back to w123
  ::wm::SetActivationDelegate(w1.get(), NULL);
  w123->Show();
  w123->Focus();
  EXPECT_EQ(w123.get(),
            aura::client::GetFocusClient(w12.get())->GetFocusedWindow());
  ::wm::SetActivationDelegate(w1.get(), &activation_delegate);

  // Removing the focused window will set the focus to NULL because
  // parent window is not focusable.
  w12->RemoveChild(w123.get());
  EXPECT_EQ(NULL, aura::client::GetFocusClient(w123.get()));
  keyev = ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_E, ui::EF_NONE);
  details = sink->OnEventFromSource(&keyev);
  EXPECT_FALSE(keyev.handled() || details.dispatcher_destroyed);

  // Must set to NULL since the activation delegate will be destroyed before
  // the windows.
  ::wm::SetActivationDelegate(w1.get(), NULL);
}

// Various assertion testing for activating windows.
TEST_F(WindowManagerTest, ActivateOnMouse) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  TestActivationDelegate d1;
  aura::test::TestWindowDelegate wd;
  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithDelegate(&wd, -1, gfx::Rect(10, 10, 50, 50)));
  d1.SetWindow(w1.get());
  TestActivationDelegate d2;
  std::unique_ptr<aura::Window> w2(
      CreateTestWindowInShellWithDelegate(&wd, -2, gfx::Rect(70, 70, 50, 50)));
  d2.SetWindow(w2.get());

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(w1.get());

  d1.Clear();
  d2.Clear();

  // Activate window1.
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(w1.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  d1.Clear();

  {
    // Click on window2.
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), w2.get());
    generator.ClickLeftButton();

    // Window2 should have become active.
    EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
    EXPECT_EQ(w2.get(), focus_client->GetFocusedWindow());
    EXPECT_EQ(0, d1.activated_count());
    EXPECT_EQ(1, d1.lost_active_count());
    EXPECT_EQ(1, d2.activated_count());
    EXPECT_EQ(0, d2.lost_active_count());
    d1.Clear();
    d2.Clear();
  }

  {
    // Click back on window1, but set it up so w1 doesn't activate on click.
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), w1.get());
    d1.set_activate(false);
    generator.ClickLeftButton();

    // Window2 should still be active and focused.
    EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
    EXPECT_EQ(w2.get(), focus_client->GetFocusedWindow());
    EXPECT_EQ(0, d1.activated_count());
    EXPECT_EQ(0, d1.lost_active_count());
    EXPECT_EQ(0, d2.activated_count());
    EXPECT_EQ(0, d2.lost_active_count());
    d1.Clear();
    d2.Clear();
  }

  // Destroy window2, this should make window1 active.
  d1.set_activate(true);
  w2.reset();
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(1, d2.lost_active_count());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(w1.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());

  // Clicking an active window with a child shouldn't steal the
  // focus from the child.
  {
    std::unique_ptr<aura::Window> w11(CreateTestWindowWithDelegate(
        &wd, -11, gfx::Rect(10, 10, 10, 10), w1.get()));
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       w11.get());
    // First set the focus to the child |w11|.
    generator.ClickLeftButton();
    EXPECT_EQ(w11.get(), focus_client->GetFocusedWindow());
    EXPECT_EQ(w1.get(), window_util::GetActiveWindow());

    // Then click the parent active window. The focus shouldn't move.
    gfx::Point left_top = w1->bounds().origin();
    aura::Window::ConvertPointToTarget(w1->parent(), root_window, &left_top);
    left_top.Offset(1, 1);
    generator.MoveMouseTo(left_top);
    generator.ClickLeftButton();
    EXPECT_EQ(w11.get(), focus_client->GetFocusedWindow());
    EXPECT_EQ(w1.get(), window_util::GetActiveWindow());
  }

  // Clicking on a non-focusable window inside a background window should still
  // give focus to the background window.
  {
    aura::test::TestWindowDelegate non_focusable_delegate;
    non_focusable_delegate.set_can_focus(false);
    std::unique_ptr<aura::Window> w11(CreateTestWindowWithDelegate(
        &non_focusable_delegate, -1, gfx::Rect(10, 10, 10, 10), w1.get()));
    // Move focus to |w2| first.
    w2.reset(CreateTestWindowInShellWithDelegate(&wd, -1,
                                                 gfx::Rect(70, 70, 50, 50)));
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), w2.get());
    generator.ClickLeftButton();
    EXPECT_EQ(w2.get(), focus_client->GetFocusedWindow());
    EXPECT_FALSE(w11->CanFocus());

    // Click on |w11|. This should focus w1.
    generator.MoveMouseToCenterOf(w11.get());
    generator.ClickLeftButton();
    EXPECT_EQ(w1.get(), focus_client->GetFocusedWindow());
  }
}

// Tests that Set window property |kActivateOnPointerKey| to false could
// properly ignore pointer window activation.
TEST_F(WindowManagerTest, ActivateOnPointerWindowProperty) {
  // Create two test windows, window1 and window2.
  aura::test::TestWindowDelegate wd;
  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithDelegate(&wd, -1, gfx::Rect(10, 10, 50, 50)));
  std::unique_ptr<aura::Window> w2(
      CreateTestWindowInShellWithDelegate(&wd, -2, gfx::Rect(70, 70, 50, 50)));

  // Activate window1.
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));

  // Set window2 not pointer activatable.
  w2->SetProperty(aura::client::kActivateOnPointerKey, false);
  // Mouse click on window2.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), w2.get());
  generator.ClickLeftButton();
  // Window2 should not become active.
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Gesture a tap at window2.
  generator.GestureTapAt(w2->bounds().CenterPoint());
  // Window2 should not become active.
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Set window2 now pointer activatable.
  w2->SetProperty(aura::client::kActivateOnPointerKey, true);
  // Mouse click on window2.
  generator.ClickLeftButton();
  // Window2 should become active.
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_FALSE(wm::IsActiveWindow(w1.get()));
}

// Essentially the same as ActivateOnMouse, but for touch events.
TEST_F(WindowManagerTest, ActivateOnTouch) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  TestActivationDelegate d1;
  aura::test::TestWindowDelegate wd;
  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithDelegate(&wd, -1, gfx::Rect(10, 10, 50, 50)));
  d1.SetWindow(w1.get());
  TestActivationDelegate d2;
  std::unique_ptr<aura::Window> w2(
      CreateTestWindowInShellWithDelegate(&wd, -2, gfx::Rect(70, 70, 50, 50)));
  d2.SetWindow(w2.get());

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(w1.get());

  d1.Clear();
  d2.Clear();

  // Activate window1.
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(w1.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  d1.Clear();

  // Touch window2.
  gfx::Point press_point = w2->bounds().CenterPoint();
  aura::Window::ConvertPointToTarget(w2->parent(), root_window, &press_point);
  ui::TouchEvent touchev1(ui::EventType::kTouchPressed, press_point,
                          ui::EventTimeForNow(),
                          ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  ui::EventSink* sink = root_window->GetHost()->GetEventSink();
  ui::EventDispatchDetails details = sink->OnEventFromSource(&touchev1);
  ASSERT_FALSE(details.dispatcher_destroyed);

  // Window2 should have become active.
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_EQ(w2.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(0, d1.activated_count());
  EXPECT_EQ(1, d1.lost_active_count());
  EXPECT_EQ(1, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  d1.Clear();
  d2.Clear();

  // Touch window1, but set it up so w1 doesn't activate on touch.
  press_point = w1->bounds().CenterPoint();
  aura::Window::ConvertPointToTarget(w1->parent(), root_window, &press_point);
  d1.set_activate(false);
  ui::TouchEvent touchev2(ui::EventType::kTouchPressed, press_point,
                          ui::EventTimeForNow(),
                          ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  details = sink->OnEventFromSource(&touchev2);
  ASSERT_FALSE(details.dispatcher_destroyed);

  // Window2 should still be active and focused.
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_EQ(w2.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(0, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  d1.Clear();
  d2.Clear();

  // Destroy window2, this should make window1 active.
  d1.set_activate(true);
  w2.reset();
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(1, d2.lost_active_count());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(w1.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
}

TEST_F(WindowManagerTest, MouseEventCursors) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  // Create a window.
  const int kWindowLeft = 123;
  const int kWindowTop = 45;
  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, -1, gfx::Rect(kWindowLeft, kWindowTop, 640, 480)));

  // Create two mouse movement events we can switch between.
  gfx::Point point1(kWindowLeft, kWindowTop);
  aura::Window::ConvertPointToTarget(window->parent(), root_window, &point1);

  gfx::Point point2(kWindowLeft + 1, kWindowTop + 1);
  aura::Window::ConvertPointToTarget(window->parent(), root_window, &point2);

  aura::WindowTreeHost* host = root_window->GetHost();
  ui::EventSink* sink = host->GetEventSink();

  // Cursor starts as a pointer (set during Shell::Init()).
  EXPECT_EQ(ui::mojom::CursorType::kPointer, host->last_cursor().type());

  {
    // Resize edges and corners show proper cursors.
    window_delegate.set_window_component(HTBOTTOM);
    ui::MouseEvent move1(ui::EventType::kMouseMoved, point1, point1,
                         ui::EventTimeForNow(), 0, 0);
    ui::EventDispatchDetails details = sink->OnEventFromSource(&move1);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(ui::mojom::CursorType::kSouthResize, host->last_cursor().type());
  }

  {
    window_delegate.set_window_component(HTBOTTOMLEFT);
    ui::MouseEvent move2(ui::EventType::kMouseMoved, point2, point2,
                         ui::EventTimeForNow(), 0, 0);
    ui::EventDispatchDetails details = sink->OnEventFromSource(&move2);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(ui::mojom::CursorType::kSouthWestResize,
              host->last_cursor().type());
  }

  {
    window_delegate.set_window_component(HTBOTTOMRIGHT);
    ui::MouseEvent move1(ui::EventType::kMouseMoved, point1, point1,
                         ui::EventTimeForNow(), 0, 0);
    ui::EventDispatchDetails details = sink->OnEventFromSource(&move1);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(ui::mojom::CursorType::kSouthEastResize,
              host->last_cursor().type());
  }

  {
    window_delegate.set_window_component(HTLEFT);
    ui::MouseEvent move2(ui::EventType::kMouseMoved, point2, point2,
                         ui::EventTimeForNow(), 0, 0);
    ui::EventDispatchDetails details = sink->OnEventFromSource(&move2);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(ui::mojom::CursorType::kWestResize, host->last_cursor().type());
  }

  {
    window_delegate.set_window_component(HTRIGHT);
    ui::MouseEvent move1(ui::EventType::kMouseMoved, point1, point1,
                         ui::EventTimeForNow(), 0, 0);
    ui::EventDispatchDetails details = sink->OnEventFromSource(&move1);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(ui::mojom::CursorType::kEastResize, host->last_cursor().type());
  }

  {
    window_delegate.set_window_component(HTTOP);
    ui::MouseEvent move2(ui::EventType::kMouseMoved, point2, point2,
                         ui::EventTimeForNow(), 0, 0);
    ui::EventDispatchDetails details = sink->OnEventFromSource(&move2);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(ui::mojom::CursorType::kNorthResize, host->last_cursor().type());
  }

  {
    window_delegate.set_window_component(HTTOPLEFT);
    ui::MouseEvent move1(ui::EventType::kMouseMoved, point1, point1,
                         ui::EventTimeForNow(), 0, 0);
    ui::EventDispatchDetails details = sink->OnEventFromSource(&move1);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(ui::mojom::CursorType::kNorthWestResize,
              host->last_cursor().type());
  }

  {
    window_delegate.set_window_component(HTTOPRIGHT);
    ui::MouseEvent move2(ui::EventType::kMouseMoved, point2, point2,
                         ui::EventTimeForNow(), 0, 0);
    ui::EventDispatchDetails details = sink->OnEventFromSource(&move2);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(ui::mojom::CursorType::kNorthEastResize,
              host->last_cursor().type());
  }

  {
    // Client area uses null cursor.
    window_delegate.set_window_component(HTCLIENT);
    ui::MouseEvent move1(ui::EventType::kMouseMoved, point1, point1,
                         ui::EventTimeForNow(), 0, 0);
    ui::EventDispatchDetails details = sink->OnEventFromSource(&move1);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(ui::mojom::CursorType::kNull, host->last_cursor().type());
  }
}

TEST_F(WindowManagerTest, TransformActivate) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  gfx::Size size = root_window->bounds().size();
  EXPECT_EQ(gfx::Rect(size).ToString(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point())
                .bounds()
                .ToString());

  // Rotate it clock-wise 90 degrees.
  gfx::Transform transform;
  transform.Translate(size.width(), 0);
  transform.Rotate(90.0f);
  root_window->GetHost()->SetRootTransform(transform);

  TestActivationDelegate d1;
  aura::test::TestWindowDelegate wd;
  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithDelegate(&wd, 1, gfx::Rect(0, 15, 50, 50)));
  d1.SetWindow(w1.get());
  w1->Show();

  gfx::Point miss_point(5, 5);
  miss_point = transform.MapPoint(miss_point);
  ui::MouseEvent mouseev1(ui::EventType::kMousePressed, miss_point, miss_point,
                          ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                          ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventSink* sink = root_window->GetHost()->GetEventSink();
  ui::EventDispatchDetails details = sink->OnEventFromSource(&mouseev1);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(NULL, aura::client::GetFocusClient(w1.get())->GetFocusedWindow());
  ui::MouseEvent mouseup(ui::EventType::kMouseReleased, miss_point, miss_point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  details = sink->OnEventFromSource(&mouseup);
  ASSERT_FALSE(details.dispatcher_destroyed);

  gfx::Point hit_point(5, 15);
  hit_point = transform.MapPoint(hit_point);
  ui::MouseEvent mouseev2(ui::EventType::kMousePressed, hit_point, hit_point,
                          ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                          ui::EF_LEFT_MOUSE_BUTTON);
  details = sink->OnEventFromSource(&mouseev2);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(w1.get(),
            aura::client::GetFocusClient(w1.get())->GetFocusedWindow());
}

TEST_F(WindowManagerTest, AdditionalFilters) {
  // The IME event filter interferes with the basic key event propagation we
  // attempt to do here, so we disable it.
  DisableIME();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  // Creates a window and make it active
  std::unique_ptr<aura::Window> w1 =
      TestWindowBuilder().SetBounds(gfx::Rect(0, 0, 100, 100)).Build();
  wm::ActivateWindow(w1.get());

  // Creates two addition filters
  std::unique_ptr<CustomEventHandler> f1(new CustomEventHandler);
  std::unique_ptr<CustomEventHandler> f2(new CustomEventHandler);

  // Adds them to root window event filter.
  ::wm::CompoundEventFilter* env_filter = Shell::Get()->env_filter();
  env_filter->AddHandler(f1.get());
  env_filter->AddHandler(f2.get());

  // Dispatches mouse and keyboard events.
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  ui::EventSink* sink = root_window->GetHost()->GetEventSink();
  ui::EventDispatchDetails details = sink->OnEventFromSource(&key_event);
  ASSERT_FALSE(details.dispatcher_destroyed);
  ui::MouseEvent mouse_pressed(ui::EventType::kMousePressed, gfx::Point(0, 0),
                               gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
  details = sink->OnEventFromSource(&mouse_pressed);
  ASSERT_FALSE(details.dispatcher_destroyed);

  // Both filters should get the events.
  EXPECT_EQ(1, f1->num_key_events());
  EXPECT_EQ(1, f1->num_mouse_events());
  EXPECT_EQ(1, f2->num_key_events());
  EXPECT_EQ(1, f2->num_mouse_events());

  f1->Reset();
  f2->Reset();

  // Makes f1 consume events.
  f1->set_key_event_handling_result(ui::ER_CONSUMED);
  f1->set_mouse_event_handling_result(ui::ER_CONSUMED);

  // Dispatches events.
  details = sink->OnEventFromSource(&key_event);
  ASSERT_FALSE(details.dispatcher_destroyed);
  ui::MouseEvent mouse_released(ui::EventType::kMouseReleased, gfx::Point(0, 0),
                                gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
  details = sink->OnEventFromSource(&mouse_released);
  ASSERT_FALSE(details.dispatcher_destroyed);

  // f1 should still get the events but f2 no longer gets them.
  EXPECT_EQ(1, f1->num_key_events());
  EXPECT_EQ(1, f1->num_mouse_events());
  EXPECT_EQ(0, f2->num_key_events());
  EXPECT_EQ(0, f2->num_mouse_events());

  f1->Reset();
  f2->Reset();

  // Remove f1 from additonal filters list.
  env_filter->RemoveHandler(f1.get());

  // Dispatches events.
  details = sink->OnEventFromSource(&key_event);
  ASSERT_FALSE(details.dispatcher_destroyed);
  details = sink->OnEventFromSource(&mouse_pressed);
  ASSERT_FALSE(details.dispatcher_destroyed);

  // f1 should get no events since it's out and f2 should get them.
  EXPECT_EQ(0, f1->num_key_events());
  EXPECT_EQ(0, f1->num_mouse_events());
  EXPECT_EQ(1, f2->num_key_events());
  EXPECT_EQ(1, f2->num_mouse_events());

  env_filter->RemoveHandler(f2.get());
}

// Touch visually hides the cursor.
TEST_F(WindowManagerTest, UpdateCursorVisibility) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();

  generator->MoveMouseTo(gfx::Point(0, 0));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  generator->PressTouch();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
  generator->MoveMouseTo(gfx::Point(0, 0));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  generator->ReleaseTouch();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
}

// Tests cursor visibility on key pressed event.
TEST_F(WindowManagerTest, UpdateCursorVisibilityOnKeyEvent) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();

  // Pressing a key hides the cursor but does not disable mouse events.
  generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  // Moving mouse shows the cursor.
  generator->MoveMouseTo(gfx::Point(0, 0));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  // Releasing a key does does not hide the cursor and does not disable mouse
  // events.
  generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  // Pressing a key with mouse button pressed does not hide the cursor and does
  // not disable mouse events.
  generator->PressLeftButton();
  generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  generator->ReleaseLeftButton();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
}

// Test that pressing an accelerator does not hide the cursor.
TEST_F(WindowManagerTest, UpdateCursorVisibilityAccelerator) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();

  ASSERT_TRUE(cursor_manager->IsCursorVisible());

  // Press Ctrl+A, release A first.
  generator->PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  generator->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  generator->ReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Press Ctrl+A, release Ctrl first.
  generator->PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  generator->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

TEST_F(WindowManagerTest, TestCursorClientObserver) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();

  std::unique_ptr<aura::Window> w1 = TestWindowBuilder()
                                         .SetColorWindowDelegate(SK_ColorWHITE)
                                         .SetBounds(gfx::Rect(0, 0, 100, 100))
                                         .Build();
  wm::ActivateWindow(w1.get());

  // Add two observers. Both should have OnCursorVisibilityChanged()
  // invoked when an event changes the visibility of the cursor.
  ::wm::TestingCursorClientObserver observer_a;
  ::wm::TestingCursorClientObserver observer_b;
  cursor_manager->AddObserver(&observer_a);
  cursor_manager->AddObserver(&observer_b);

  // Initial state before any events have been sent.
  observer_a.reset();
  observer_b.reset();
  EXPECT_FALSE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());
  EXPECT_FALSE(observer_b.is_cursor_visible());
  EXPECT_FALSE(observer_a.did_cursor_size_change());
  EXPECT_FALSE(observer_b.did_cursor_size_change());

  // Keypress should hide the cursor.
  generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_TRUE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());
  EXPECT_FALSE(observer_b.is_cursor_visible());

  // Set cursor set.
  cursor_manager->SetCursorSize(ui::CursorSize::kLarge);
  EXPECT_TRUE(observer_a.did_cursor_size_change());
  EXPECT_EQ(ui::CursorSize::kLarge, observer_a.cursor_size());
  EXPECT_TRUE(observer_b.did_cursor_size_change());
  EXPECT_EQ(ui::CursorSize::kLarge, observer_b.cursor_size());

  // Mouse move should show the cursor.
  observer_a.reset();
  observer_b.reset();
  generator->MoveMouseTo(50, 50);
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_TRUE(observer_b.did_visibility_change());
  EXPECT_TRUE(observer_a.is_cursor_visible());
  EXPECT_TRUE(observer_b.is_cursor_visible());

  // Remove observer_b. Its OnCursorVisibilityChanged() should
  // not be invoked past this point.
  cursor_manager->RemoveObserver(&observer_b);

  // Gesture tap should hide the cursor.
  observer_a.reset();
  observer_b.reset();
  generator->GestureTapAt(gfx::Point(25, 25));
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());

  // Set back cursor set to normal.
  cursor_manager->SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_TRUE(observer_a.did_cursor_size_change());
  EXPECT_EQ(ui::CursorSize::kNormal, observer_a.cursor_size());
  EXPECT_FALSE(observer_b.did_cursor_size_change());

  // Mouse move should show the cursor.
  observer_a.reset();
  observer_b.reset();
  generator->MoveMouseTo(50, 50);
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_TRUE(observer_a.is_cursor_visible());

  cursor_manager->RemoveObserver(&observer_a);
}

}  // namespace ash
