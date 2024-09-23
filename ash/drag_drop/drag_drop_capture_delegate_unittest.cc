// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_capture_delegate.h"

#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/drag_drop/toplevel_window_drag_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/gestures/motion_event_aura.h"

namespace ash {
namespace {

class DragDropCaptureDelegateTest : public AshTestBase {
 public:
  DragDropCaptureDelegateTest() = default;

  DragDropCaptureDelegateTest(const DragDropCaptureDelegateTest&) = delete;
  DragDropCaptureDelegateTest& operator=(const DragDropCaptureDelegateTest&) =
      delete;

  ~DragDropCaptureDelegateTest() override = default;

  // AshTestBase:
  void SetUp() override {
    drag_drop_capture_delegate_.reset(new DragDropCaptureDelegate());
    AshTestBase::SetUp(std::make_unique<TestShellDelegate>());
  }

  void TearDown() override {
    drag_drop_capture_delegate_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<DragDropCaptureDelegate> drag_drop_capture_delegate_;
};

class TestWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  TestWindowDelegate() = default;
  TestWindowDelegate(const TestWindowDelegate&) = delete;
  TestWindowDelegate& operator=(const TestWindowDelegate&) = delete;
  ~TestWindowDelegate() override = default;

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) final {
    motion_event.OnTouch(*event);
    if (event->type() == ui::EventType::kTouchCancelled) {
      touch_cancel_received = true;
    }
  }

  ui::MotionEventAura motion_event;
  bool touch_cancel_received = false;
};

class TestDragDelegate : public ToplevelWindowDragDelegate {
 public:
  TestDragDelegate() = default;
  TestDragDelegate(const TestDragDelegate&) = delete;
  TestDragDelegate& operator=(const TestDragDelegate&) = delete;
  ~TestDragDelegate() override = default;

  // ToplevelWindowDragDelegate:
  void OnToplevelWindowDragStarted(const gfx::PointF& start_location,
                                   ui::mojom::DragEventSource source,
                                   aura::Window* source_window) override {}

  ::ui::mojom::DragOperation OnToplevelWindowDragDropped() override {
    return ::ui::mojom::DragOperation::kMove;
  }

  void OnToplevelWindowDragCancelled() override {}

  void OnToplevelWindowDragEvent(ui::LocatedEvent* event) override {}
};

}  // namespace

TEST_F(DragDropCaptureDelegateTest, CanTakeCaptureAndConvertToOriginalWindow) {
  TestWindowDelegate source_window_delegate;

  auto source_window = base::WrapUnique(CreateTestWindowInShellWithDelegate(
      &source_window_delegate, -1, gfx::Rect(0, 0, 100, 100)));
  source_window->Show();
  EXPECT_FALSE(source_window_delegate.touch_cancel_received);

  drag_drop_capture_delegate_->TakeCapture(
      source_window->GetRootWindow(), source_window.get(),
      base::BindLambdaForTesting([]() {}),
      ui::TransferTouchesBehavior::kCancel);

  EXPECT_TRUE(drag_drop_capture_delegate_->capture_window()->HasCapture());
  EXPECT_TRUE(source_window_delegate.touch_cancel_received);

  ui::GestureEventDetails event_details(ui::EventType::kGestureScrollUpdate);
  ui::GestureEvent gesture_event(0, 0, 0, ui::EventTimeForNow(), event_details);
  ui::Event::DispatcherApi(&gesture_event)
      .set_target(drag_drop_capture_delegate_->capture_window());
  auto* converted_target =
      drag_drop_capture_delegate_->GetTarget(gesture_event);

  EXPECT_EQ(converted_target, source_window.get());
}

// Make sure that state of the MotionEvent on source window will be set to
// cancled when the drag and drop is started with gesture.
TEST_F(DragDropCaptureDelegateTest, CanTakeCaptureAndConvertToOriginalWindow2) {
  TestWindowDelegate source_window_delegate;

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &source_window_delegate, -1, gfx::Rect(0, 0, 100, 100)));

  ui::test::EventGenerator generator(window->GetRootWindow(), window.get());

  gfx::Point point(5, 5);
  generator.PressTouch(point);

  EXPECT_EQ(source_window_delegate.motion_event.GetAction(),
            ui::MotionEvent::Action::DOWN);
  point.Offset(1, 1);
  generator.MoveTouch(point);

  EXPECT_EQ(source_window_delegate.motion_event.GetAction(),
            ui::MotionEvent::Action::MOVE);

  auto data = std::make_unique<ui::OSExchangeData>();
  auto* drag_drop_controller = static_cast<DragDropController*>(
      aura::client::GetDragDropClient(window->GetRootWindow()));

  TestDragDelegate test_drag_delegate;
  drag_drop_controller->set_toplevel_window_drag_delegate(&test_drag_delegate);
  drag_drop_controller->SetDisableNestedLoopForTesting(true);
  drag_drop_controller->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window.get(), point,
      ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kTouch);

  EXPECT_EQ(source_window_delegate.motion_event.GetAction(),
            ui::MotionEvent::Action::CANCEL);

  drag_drop_controller->DragCancel();
}

TEST_F(DragDropCaptureDelegateTest, ReleaseCapture) {
  TestWindowDelegate source_window_delegate;

  auto source_window = base::WrapUnique(CreateTestWindowInShellWithDelegate(
      &source_window_delegate, -1, gfx::Rect(0, 0, 100, 100)));
  source_window->Show();
  EXPECT_FALSE(source_window_delegate.touch_cancel_received);

  drag_drop_capture_delegate_->TakeCapture(
      source_window->GetRootWindow(), source_window.get(),
      base::BindLambdaForTesting([]() {}),
      ui::TransferTouchesBehavior::kCancel);

  EXPECT_TRUE(ash::window_util::GetCaptureWindow());

  drag_drop_capture_delegate_->ReleaseCapture();

  EXPECT_FALSE(ash::window_util::GetCaptureWindow());
}

}  // namespace ash
