// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/touch_mode_mouse_rewriter.h"

#include "ash/components/arc/compat_mode/metrics.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

class LongPressReceiverView : public views::View {
 public:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsLeftMouseButton()) {
      left_pressed_ = true;
      ++press_count_;
      return true;
    } else if (event.IsRightMouseButton()) {
      right_pressed_ = true;
      ++press_count_;
      return true;
    }
    return false;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (event.IsLeftMouseButton()) {
      left_pressed_ = false;
      ++release_count_;
    } else if (event.IsRightMouseButton()) {
      right_pressed_ = false;
      ++release_count_;
    }
  }

  bool left_pressed() const { return left_pressed_; }
  bool right_pressed() const { return right_pressed_; }
  int press_count() const { return press_count_; }
  int release_count() const { return release_count_; }

 private:
  bool left_pressed_ = false;
  bool right_pressed_ = false;
  int press_count_ = 0;
  int release_count_ = 0;
};

class ScrollReceiverView : public views::View {
 public:
  void OnScrollEvent(ui::ScrollEvent* event) override {
    if (event->type() == ui::EventType::kScrollFlingStart) {
      fling_started_ = true;
    } else if (event->type() == ui::EventType::kScrollFlingCancel) {
      fling_cancelled_ = true;
    } else if (event->type() == ui::EventType::kScroll) {
      smooth_scrolled_ = true;
      x_offset_ += event->x_offset();
      y_offset_ += event->y_offset();
      scroll_timestamps.push_back(base::TimeTicks::Now());
    }
  }

  bool fling_started() const { return fling_started_; }
  bool fling_cancelled() const { return fling_cancelled_; }
  bool smooth_scrolled() const { return smooth_scrolled_; }
  int x_scroll_offset() { return x_offset_; }
  int y_scroll_offset() { return y_offset_; }
  std::vector<base::TimeTicks> get_scroll_timestamps() {
    return scroll_timestamps;
  }

 private:
  bool fling_started_ = false;
  bool fling_cancelled_ = false;
  bool smooth_scrolled_ = false;
  int x_offset_ = 0;
  int y_offset_ = 0;
  std::vector<base::TimeTicks> scroll_timestamps;
};

}  // namespace

class TouchModeMouseRewriterTest : public views::ViewsTestBase {
 public:
  TouchModeMouseRewriterTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TouchModeMouseRewriterTest() override = default;
};

TEST_F(TouchModeMouseRewriterTest, RightClickConvertedToLongPress) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_CONTROL);
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_FALSE(view->left_pressed());
  EXPECT_FALSE(view->right_pressed());

  // Press the right button. It will immediately generate a synthesized left
  // press event.
  generator.PressRightButton();
  EXPECT_TRUE(view->left_pressed());
  EXPECT_FALSE(view->right_pressed());

  // Immediately release the right button. It will not generate any event.
  generator.ReleaseRightButton();
  EXPECT_TRUE(view->left_pressed());
  EXPECT_FALSE(view->right_pressed());

  // After a while, the synthesized left press will be released.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(view->left_pressed());
  EXPECT_FALSE(view->right_pressed());

  touch_mode_mouse_rewriter.DisableForWindow(widget->GetNativeWindow());
}

TEST_F(TouchModeMouseRewriterTest, DisabledForWindow) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_CONTROL);
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());

  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_CONTROL);
  LongPressReceiverView* view2 =
      widget2->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget2->Show();
  // Not enabled for the widget2.
  ui::test::EventGenerator generator(GetContext(), widget2->GetNativeWindow());
  EXPECT_FALSE(view->left_pressed());
  EXPECT_FALSE(view->right_pressed());

  // Press the right button.
  generator.PressRightButton();
  EXPECT_TRUE(view2->right_pressed());

  // Immediately release the right button.
  generator.ReleaseRightButton();
  EXPECT_FALSE(view2->right_pressed());
}

TEST_F(TouchModeMouseRewriterTest, LeftPressedBeforeRightClick) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_CONTROL);
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_EQ(0, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.PressLeftButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  // This right click should be ignored.
  generator.PressRightButton();
  generator.ReleaseRightButton();

  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.ReleaseLeftButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  touch_mode_mouse_rewriter.DisableForWindow(widget->GetNativeWindow());
}

TEST_F(TouchModeMouseRewriterTest, RightClickDuringLeftPress) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_CONTROL);
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_EQ(0, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.PressLeftButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  // This right click should be ignored.
  generator.PressRightButton();
  generator.ReleaseRightButton();

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.ReleaseLeftButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  touch_mode_mouse_rewriter.DisableForWindow(widget->GetNativeWindow());
}

TEST_F(TouchModeMouseRewriterTest, LeftClickedAfterRightClick) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_CONTROL);
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_EQ(0, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.PressRightButton();
  generator.ReleaseRightButton();

  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  task_environment()->FastForwardBy(base::Milliseconds(200));
  // This left click should be ignored.
  generator.PressLeftButton();
  generator.ReleaseLeftButton();
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  touch_mode_mouse_rewriter.DisableForWindow(widget->GetNativeWindow());
}

TEST_F(TouchModeMouseRewriterTest, LeftLongPressedAfterRightClick) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_CONTROL);
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_EQ(0, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.PressRightButton();
  generator.ReleaseRightButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  task_environment()->FastForwardBy(base::Milliseconds(200));

  // This left long press should be ignored.
  generator.PressLeftButton();
  task_environment()->FastForwardBy(base::Seconds(1));
  generator.ReleaseLeftButton();

  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  touch_mode_mouse_rewriter.DisableForWindow(widget->GetNativeWindow());
}

TEST_F(TouchModeMouseRewriterTest, RightClickedTwice) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_CONTROL);
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_EQ(0, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.PressRightButton();
  generator.ReleaseRightButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  task_environment()->FastForwardBy(base::Milliseconds(200));

  // This right click should be ignored.
  generator.PressRightButton();
  generator.ReleaseRightButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  touch_mode_mouse_rewriter.DisableForWindow(widget->GetNativeWindow());
}

TEST_F(TouchModeMouseRewriterTest, VerticalWheelScrollConvertedToSmoothScroll) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_CONTROL);
  ScrollReceiverView* view =
      widget->SetContentsView(std::make_unique<ScrollReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_FALSE(view->fling_cancelled());
  EXPECT_FALSE(view->smooth_scrolled());
  EXPECT_FALSE(view->fling_started());

  generator.MoveMouseWheel(0, 10);
  generator.MoveMouseWheel(0, 10);
  base::RunLoop().RunUntilIdle();
  // Smooth scrolling started.
  EXPECT_TRUE(view->fling_cancelled());
  EXPECT_TRUE(view->smooth_scrolled());
  EXPECT_FALSE(view->fling_started());
  // y_wheel_scroll * kWheelToSmoothScrollScale * kSmoothScrollEventInterval /
  // kSmoothScrollTimeout == 2
  EXPECT_EQ(2, view->y_scroll_offset());

  // Smooth scrolling ended.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(view->fling_cancelled());
  EXPECT_TRUE(view->smooth_scrolled());
  EXPECT_TRUE(view->fling_started());
  // y_wheel_scroll * kWheelToSmoothScrollScale == 60
  EXPECT_EQ(60, view->y_scroll_offset());

  touch_mode_mouse_rewriter.DisableForWindow(widget->GetNativeWindow());
}

TEST_F(TouchModeMouseRewriterTest,
       HorizontalWheelScrollConvertedToSmoothScroll) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_CONTROL);
  ScrollReceiverView* view =
      widget->SetContentsView(std::make_unique<ScrollReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_FALSE(view->fling_cancelled());
  EXPECT_FALSE(view->smooth_scrolled());
  EXPECT_FALSE(view->fling_started());

  generator.MoveMouseWheel(10, 0);
  generator.MoveMouseWheel(10, 0);
  base::RunLoop().RunUntilIdle();
  // Smooth scrolling started.
  EXPECT_TRUE(view->fling_cancelled());
  EXPECT_TRUE(view->smooth_scrolled());
  EXPECT_FALSE(view->fling_started());
  // x_wheel_scroll * kWheelToSmoothScrollScale * kSmoothScrollEventInterval /
  // kSmoothScrollTimeout == 2
  EXPECT_EQ(2, view->x_scroll_offset());

  // Smooth scrolling ended.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(view->fling_cancelled());
  EXPECT_TRUE(view->smooth_scrolled());
  EXPECT_TRUE(view->fling_started());
  // x_wheel_scroll * kWheelToSmoothScrollScale == 60
  EXPECT_EQ(60, view->x_scroll_offset());

  touch_mode_mouse_rewriter.DisableForWindow(widget->GetNativeWindow());
}

TEST_F(TouchModeMouseRewriterTest, VerticalWheelScrollCorrectInterval) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_CONTROL);
  ScrollReceiverView* view =
      widget->SetContentsView(std::make_unique<ScrollReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());

  // Generate 3 scrolls at 5ms after another (0ms, 5ms, 10ms).
  // The scroll event should still be sent at (0ms, 20ms, 40ms, ...),
  // and not (0ms, 5ms, 10ms, 20ms, 25ms, 30ms).
  // Interval is set at kSmoothScrollEventInterval.
  generator.MoveMouseWheel(0, 10);
  base::RunLoop().RunUntilIdle();
  task_environment()->FastForwardBy(base::Milliseconds(5));
  generator.MoveMouseWheel(0, 10);
  base::RunLoop().RunUntilIdle();
  task_environment()->FastForwardBy(base::Milliseconds(5));
  generator.MoveMouseWheel(0, 10);
  base::RunLoop().RunUntilIdle();

  // Smooth scrolling ended.
  task_environment()->FastForwardBy(base::Seconds(1));

  std::vector<base::TimeTicks> timestamps = view->get_scroll_timestamps();
  EXPECT_TRUE(!timestamps.empty());
  for (size_t i = 1; i < timestamps.size(); i++) {
    base::TimeDelta interval = timestamps[i] - timestamps[i - 1];
    EXPECT_EQ(base::Milliseconds(20), interval);
  }

  touch_mode_mouse_rewriter.DisableForWindow(widget->GetNativeWindow());
}

}  // namespace arc
