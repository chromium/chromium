// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/ui_metrics_recorder.h"

#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/event.h"

namespace ash {
namespace {

class FakeTestView : public views::View {
 public:
  FakeTestView() = default;
  ~FakeTestView() override = default;

  // views::View:
  void OnEvent(ui::Event* event) override {
    views::View::OnEvent(event);
    SchedulePaint();
    event->SetHandled();
  }

  void OnKeyEvent(ui::KeyEvent* event) override { received_key_event_ += 1; }

  int GetReceivedKeyEvent() const { return received_key_event_; }

 protected:
  int received_key_event_ = 0;
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
  FakeTestView* view =
      widget->SetContentsView(std::make_unique<FakeTestView>());
  widget->GetFocusManager()->SetFocusedView(view);

  base::HistogramTester histogram_tester;

  // TODO(b/258382822): Each key event generates one extra latency data for IME.
  // Ideally we should have only one latency data for each event.
  DisableIME();

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
  FakeTestView* view =
      widget->SetContentsView(std::make_unique<FakeTestView>());
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

}  // namespace
}  // namespace ash
