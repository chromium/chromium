// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_capture_delegate.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/bind.h"
#include "base/test/bind.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/gesture_types.h"

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

}  // namespace

TEST_F(DragDropCaptureDelegateTest, CanTakeCaptureAndConvertToOriginalWindow) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), -1,
      gfx::Rect(0, 0, 100, 100)));

  drag_drop_capture_delegate_->TakeCapture(
      window->GetRootWindow(), window.get(),
      base::BindLambdaForTesting([]() {}),
      ui::TransferTouchesBehavior::kCancel);

  EXPECT_TRUE(drag_drop_capture_delegate_->capture_window()->HasCapture());

  ui::GestureEventDetails event_details(ui::ET_GESTURE_SCROLL_UPDATE);
  ui::GestureEvent gesture_event(0, 0, 0, ui::EventTimeForNow(), event_details);
  ui::Event::DispatcherApi(&gesture_event)
      .set_target(drag_drop_capture_delegate_->capture_window());
  auto* converted_target =
      drag_drop_capture_delegate_->GetTarget(gesture_event);

  EXPECT_EQ(converted_target, window.get());
}

}  // namespace ash
