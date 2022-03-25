// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/touch_mode_mouse_rewriter.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/compat_mode/metrics.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
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
    if (event->type() == ui::ET_SCROLL_FLING_START)
      fling_started_ = true;
    else if (event->type() == ui::ET_SCROLL_FLING_CANCEL)
      fling_cancelled_ = true;
    else if (event->type() == ui::ET_SCROLL)
      smooth_scrolled_ = true;
  }

  bool fling_started() const { return fling_started_; }
  bool fling_cancelled() const { return fling_cancelled_; }
  bool smooth_scrolled() const { return smooth_scrolled_; }

 private:
  bool fling_started_ = false;
  bool fling_cancelled_ = false;
  bool smooth_scrolled_ = false;
};

}  // namespace

class TouchModeMouseRewriterTest : public views::ViewsTestBase {
 public:
  TouchModeMouseRewriterTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TouchModeMouseRewriterTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    feature_list_.InitWithFeatures({arc::kRightClickLongPress}, {});
  }

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester;
};

TEST_F(TouchModeMouseRewriterTest, RightClickConvertedToLongPress) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::TYPE_CONTROL);
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

  histogram_tester.ExpectUniqueSample(
      "Arc.CompatMode.RightClickConversion",
      RightClickConversionResultHistogramResult::kConverted, 1);

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

TEST_F(TouchModeMouseRewriterTest, FeatureIsDisabled) {
  // Disable kRightClickLongPress
  feature_list_.Reset();
  feature_list_.InitWithFeatures({}, {arc::kRightClickLongPress});

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::TYPE_CONTROL);
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_FALSE(view->left_pressed());
  EXPECT_FALSE(view->right_pressed());

  // Press the right button.
  generator.PressRightButton();
  EXPECT_TRUE(view->right_pressed());

  histogram_tester.ExpectUniqueSample(
      "Arc.CompatMode.RightClickConversion",
      RightClickConversionResultHistogramResult::kDisabled, 1);

  // Immediately release the right button.
  generator.ReleaseRightButton();
  EXPECT_FALSE(view->right_pressed());

  touch_mode_mouse_rewriter.DisableForWindow(widget->GetNativeWindow());
}

TEST_F(TouchModeMouseRewriterTest, DisabledForWindow) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::TYPE_CONTROL);
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  touch_mode_mouse_rewriter.EnableForWindow(widget->GetNativeWindow());

  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(views::Widget::InitParams::TYPE_CONTROL);
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

  histogram_tester.ExpectUniqueSample(
      "Arc.CompatMode.RightClickConversion",
      RightClickConversionResultHistogramResult::kNotConverted, 1);

  // Immediately release the right button.
  generator.ReleaseRightButton();
  EXPECT_FALSE(view2->right_pressed());
}

TEST_F(TouchModeMouseRewriterTest, LeftPressedBeforeRightClick) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::TYPE_CONTROL);
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
      CreateTestWidget(views::Widget::InitParams::TYPE_CONTROL);
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
      CreateTestWidget(views::Widget::InitParams::TYPE_CONTROL);
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
      CreateTestWidget(views::Widget::InitParams::TYPE_CONTROL);
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
      CreateTestWidget(views::Widget::InitParams::TYPE_CONTROL);
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

TEST_F(TouchModeMouseRewriterTest, WheelScrollConvertedToSmoothScroll) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::TYPE_CONTROL);
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

  // Smooth scrolling ended.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(view->fling_cancelled());
  EXPECT_TRUE(view->smooth_scrolled());
  EXPECT_TRUE(view->fling_started());

  touch_mode_mouse_rewriter.DisableForWindow(widget->GetNativeWindow());
}

}  // namespace arc
