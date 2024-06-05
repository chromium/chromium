// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/ui_metrics_recorder.h"

#include <memory>
#include <utility>

#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/mock_ime_engine_handler.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {
namespace {
// TestIMEEngineHandler invokes the callback synchronously for ProcessKeyEvent.
class TestIMEEngineHandler : public MockIMEEngineHandler {
 public:
  // MockIMEEngineHandler overrides:
  void ProcessKeyEvent(const ui::KeyEvent& key_event,
                       KeyEventDoneCallback callback) override {
    ++received_key_event_;
    MockIMEEngineHandler::ProcessKeyEvent(key_event, std::move(callback));
    last_passed_callback().Run(ui::ime::KeyEventHandledState::kNotHandled);
  }

  int GetReceivedKeyEvent() const { return received_key_event_; }

 private:
  int received_key_event_ = 0;
};

// WidgetDestroyHandler destroys a given widget synchronously on key events.
class WidgetDestroyHandler : public ui::test::TestEventHandler {
 public:
  explicit WidgetDestroyHandler(std::unique_ptr<views::Widget> widget)
      : widget_(std::move(widget)) {
    widget_->GetNativeWindow()->AddPreTargetHandler(this);
  }

  // ui::test::TestEventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    ++received_key_event_;
    event->SetHandled();
    widget_.reset();
  }

  int GetReceivedKeyEvent() const { return received_key_event_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  int received_key_event_ = 0;
};

// FakeTestView to consume MouseEvent and trigger a force redraw.
// Derived directly from `View` so no additional frames would be generated.
class FakeTestView : public views::View {
 public:
  FakeTestView() {
    GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
    GetViewAccessibility().SetName(u"FakeTestView");
  }
  ~FakeTestView() override = default;

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    // Schedule a draw with no damaged rect to create a did-not-produce-frame
    // case.
    GetWidget()->GetCompositor()->ScheduleDraw();
    return true;
  }
};

// FakeTextField to consumer all events and trigger a paint. Derived
// from `Textfield` so that IME related tests dispatches key events to IME
// engine.
class FakeTextField : public views::Textfield {
 public:
  FakeTextField() { GetViewAccessibility().SetName(u"FakeTextField"); }
  ~FakeTextField() override = default;

  // views::View:
  void OnEvent(ui::Event* event) override {
    if (!do_nothing_in_event_handling_) {
      views::View::OnEvent(event);
      SchedulePaint();
    }

    event->SetHandled();
  }

  void OnKeyEvent(ui::KeyEvent* event) override { ++received_key_event_; }

  int GetReceivedKeyEvent() const { return received_key_event_; }

  void set_do_nothing_in_event_handling(bool do_nothing) {
    do_nothing_in_event_handling_ = do_nothing;
  }

 protected:
  int received_key_event_ = 0;
  bool do_nothing_in_event_handling_ = false;
};

class UiMetricsRecorderTest : public AshTestBase {
 public:
  UiMetricsRecorderTest() = default;
  UiMetricsRecorderTest(const UiMetricsRecorderTest&) = delete;
  UiMetricsRecorderTest& operator=(const UiMetricsRecorderTest&) = delete;
  ~UiMetricsRecorderTest() override = default;

  std::unique_ptr<views::Widget> CreateTestWindowWidget() {
    return TestWidgetBuilder()
        .SetDelegate(nullptr)
        .SetBounds(gfx::Rect(0, 0, 100, 100))
        .SetShow(true)
        .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
        .BuildOwnsNativeWidget();
  }
};

// Ash.EventLatency metrics should be recorded when key events generating
// UI changes.
TEST_F(UiMetricsRecorderTest, KeyEvent) {
  std::unique_ptr<views::Widget> widget = CreateTestWindowWidget();
  FakeTextField* view =
      widget->SetContentsView(std::make_unique<FakeTextField>());
  widget->GetFocusManager()->SetFocusedView(view);

  base::HistogramTester histogram_tester;

  EXPECT_EQ(view->GetReceivedKeyEvent(), 0);
  PressAndReleaseKey(ui::VKEY_A);
  // Expect to receive two key events: KeyPressed and KeyRelease.
  EXPECT_EQ(view->GetReceivedKeyEvent(), 2);
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(widget->GetCompositor()));

  histogram_tester.ExpectTotalCount("Ash.EventLatency.KeyPressed.TotalLatency",
                                    1);
  histogram_tester.ExpectTotalCount("Ash.EventLatency.KeyReleased.TotalLatency",
                                    1);
  histogram_tester.ExpectTotalCount("Ash.EventLatency.TotalLatency", 2);
}

TEST_F(UiMetricsRecorderTest, Gestures) {
  std::unique_ptr<views::Widget> widget = CreateTestWindowWidget();
  FakeTextField* view =
      widget->SetContentsView(std::make_unique<FakeTextField>());
  const gfx::Rect bounds = view->GetBoundsInScreen();

  {
    // Tap.
    base::HistogramTester histogram_tester;

    GestureTapOn(view);
    EXPECT_TRUE(ui::WaitForNextFrameToBePresented(widget->GetCompositor()));

    histogram_tester.ExpectTotalCount(
        "Ash.EventLatency.GestureTapDown.TotalLatency", 1);
    histogram_tester.ExpectTotalCount(
        "Ash.EventLatency.GestureShowPress.TotalLatency", 1);
    histogram_tester.ExpectTotalCount(
        "Ash.EventLatency.GestureTap.TotalLatency", 1);
  }

  {
    // Scroll.
    base::HistogramTester histogram_tester;

    constexpr int kNumOfTouches = 5;
    GetEventGenerator()->GestureScrollSequence(
        bounds.top_center(), bounds.bottom_center(), base::Milliseconds(100),
        kNumOfTouches);
    EXPECT_TRUE(ui::WaitForNextFrameToBePresented(widget->GetCompositor()));

    histogram_tester.ExpectTotalCount(
        "Ash.EventLatency.GestureTapDown.TotalLatency", 1);
    histogram_tester.ExpectTotalCount(
        "Ash.EventLatency.GestureTapCancel.TotalLatency", 1);
    histogram_tester.ExpectTotalCount(
        "Ash.EventLatency.GestureScrollBegin.TotalLatency", 1);
    histogram_tester.GetBucketCount(
        "Ash.EventLatency.GestureScrollUpdate.TotalLatency", kNumOfTouches);
    histogram_tester.ExpectTotalCount(
        "Ash.EventLatency.GestureScrollEnd.TotalLatency", 1);
  }

  {
    // Pinch.
    base::HistogramTester histogram_tester;

    GetEventGenerator()->PressTouchId(1, bounds.origin());
    GetEventGenerator()->PressTouchId(2, bounds.bottom_right());

    GetEventGenerator()->MoveTouchId(bounds.CenterPoint(), 1);
    GetEventGenerator()->MoveTouchId(bounds.CenterPoint(), 2);

    GetEventGenerator()->ReleaseTouchId(1);
    GetEventGenerator()->ReleaseTouchId(2);

    EXPECT_TRUE(ui::WaitForNextFrameToBePresented(widget->GetCompositor()));

    histogram_tester.ExpectTotalCount(
        "Ash.EventLatency.GesturePinchBegin.TotalLatency", 1);
    histogram_tester.ExpectTotalCount(
        "Ash.EventLatency.GesturePinchUpdate.TotalLatency", 1);
    histogram_tester.ExpectTotalCount(
        "Ash.EventLatency.GesturePinchEnd.TotalLatency", 1);
  }
}

// Verifies no crashes when `EventTarget` is destroyed through a synchronous IME
// `TextInputMethod::ProcessKeyEvent` call. See http://crbug.com/1392491.
TEST_F(UiMetricsRecorderTest, TargetDestroyedWithSyncIME) {
  // Setup.
  auto ime_engine = std::make_unique<TestIMEEngineHandler>();
  IMEBridge::Get()->SetCurrentEngineHandler(ime_engine.get());

  std::unique_ptr<views::Widget> widget = CreateTestWindowWidget();
  FakeTextField* view =
      widget->SetContentsView(std::make_unique<FakeTextField>());
  widget->GetFocusManager()->SetFocusedView(view);

  // Create an event handler on the test widget to close it synchronously.
  WidgetDestroyHandler destroyer(std::move(widget));

  // Press a key and no crash should happen.
  PressAndReleaseKey(ui::VKEY_A);

  // IME engine and `destroyer` should get the key event.
  EXPECT_EQ(ime_engine->GetReceivedKeyEvent(), 1);
  EXPECT_EQ(destroyer.GetReceivedKeyEvent(), 1);

  // Teardown.
  IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
}

// Verifies that event latency is not recorded if UI handling does not cause
// screen updates.
TEST_F(UiMetricsRecorderTest, NoScreenUpdateNoLatency) {
  std::unique_ptr<views::Widget> widget = CreateTestWindowWidget();
  FakeTextField* view =
      widget->SetContentsView(std::make_unique<FakeTextField>());

  base::HistogramTester histogram_tester;

  // No screen update is created during event handling.
  view->set_do_nothing_in_event_handling(/*do_nothing=*/true);
  LeftClickOn(view);

  // Force one frame out side event handling to ensure no latency is reported.
  auto* compositor = widget->GetCompositor();
  compositor->ScheduleFullRedraw();
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(compositor));

  histogram_tester.ExpectTotalCount("Ash.EventLatency.TotalLatency", 0);
}

// Verifies that the event latency is not recorded when its frame has no damage.
TEST_F(UiMetricsRecorderTest, NoDamageNoLatency) {
  std::unique_ptr<views::Widget> widget = CreateTestWindowWidget();
  FakeTestView* view =
      widget->SetContentsView(std::make_unique<FakeTestView>());

  base::HistogramTester histogram_tester;
  auto* compositor = widget->GetCompositor();

  // Force one frame to ensure that the screen is updated.
  compositor->ScheduleFullRedraw();
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(compositor));

  // Simulate an event that triggers commit but there is no damage.
  LeftClickOn(view);

  // Wait for the event metrics to be picked up.
  ASSERT_EQ(compositor->saved_events_metrics_count_for_testing(), 1u);
  while (compositor->saved_events_metrics_count_for_testing() != 0) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
    run_loop.Run();
  }

  // Force one frame out side event handling to ensure no latency is reported.
  compositor->ScheduleFullRedraw();
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(compositor));

  histogram_tester.ExpectTotalCount("Ash.EventLatency.TotalLatency", 0);
}

}  // namespace
}  // namespace ash
