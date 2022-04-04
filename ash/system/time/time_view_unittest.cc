// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

class TimeViewTest : public AshTestBase {
 public:
  TimeViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  TimeViewTest(const TimeViewTest&) = delete;
  TimeViewTest& operator=(const TimeViewTest&) = delete;
  ~TimeViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  TimeView* time_view() { return time_view_; }

  // Access to private fields of |time_view_|.
  views::View* horizontal_view() { return time_view_->horizontal_view_.get(); }
  views::View* vertical_view() { return time_view_->vertical_view_.get(); }
  views::View* horizontal_date_view() {
    return time_view_->horizontal_date_view_.get();
  }
  views::View* vertical_date_view() {
    return time_view_->vertical_date_view_.get();
  }
  views::Label* horizontal_label() { return time_view_->horizontal_label_; }
  views::Label* vertical_label_hours() {
    return time_view_->vertical_label_hours_;
  }

  views::Label* vertical_label_minutes() {
    return time_view_->vertical_label_minutes_;
  }
  views::Label* horizontal_date_label() {
    return time_view_->horizontal_label_date_;
  }
  VerticalDateView* vertical_date() { return time_view_->date_view_; }

  // Creates a time view with horizontal or vertical |clock_layout|.
  void CreateTimeView(TimeView::ClockLayout clock_layout,
                      TimeView::Type type = TimeView::kTime) {
    time_view_ = widget_->SetContentsView(std::make_unique<TimeView>(
        clock_layout, Shell::Get()->system_tray_model()->clock(), type));
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  // Owned by `widget_`.
  TimeView* time_view_;
  base::WeakPtrFactory<TimeViewTest> weak_factory_{this};
};

class TimeViewObserver : public views::ViewObserver {
 public:
  explicit TimeViewObserver(views::View* observed_view) {
    observation_.Observe(observed_view);
  }
  TimeViewObserver(const TimeViewObserver&) = delete;
  TimeViewObserver& operator=(const TimeViewObserver&) = delete;
  ~TimeViewObserver() override = default;

  void reset_preferred_size_changed_called() {
    preferred_size_changed_called_ = false;
  }

  bool preferred_size_changed_called() const {
    return preferred_size_changed_called_;
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override {
    preferred_size_changed_called_ = true;
  }

 private:
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
  bool preferred_size_changed_called_ = false;
};

// Test the basics of the time view, mostly to ensure we don't leak memory.
TEST_F(TimeViewTest, Basics) {
  // A newly created horizontal clock only has the horizontal label.
  CreateTimeView(TimeView::ClockLayout::HORIZONTAL_CLOCK);
  ASSERT_TRUE(horizontal_label()->parent());
  EXPECT_EQ(time_view(), horizontal_label()->parent()->parent());
  EXPECT_FALSE(horizontal_view());
  ASSERT_TRUE(vertical_view());
  EXPECT_FALSE(vertical_view()->parent());

  // Switching the clock to vertical updates the labels.
  time_view()->UpdateClockLayout(TimeView::ClockLayout::VERTICAL_CLOCK);
  ASSERT_TRUE(horizontal_view());
  EXPECT_FALSE(horizontal_view()->parent());
  EXPECT_FALSE(vertical_view());
  ASSERT_TRUE(vertical_label_hours()->parent());
  ASSERT_TRUE(vertical_label_minutes()->parent());
  EXPECT_EQ(time_view(), vertical_label_hours()->parent()->parent());
  EXPECT_EQ(time_view(), vertical_label_minutes()->parent()->parent());

  // Switching back to horizontal updates the labels again.
  time_view()->UpdateClockLayout(TimeView::ClockLayout::HORIZONTAL_CLOCK);
  ASSERT_TRUE(horizontal_label()->parent());
  EXPECT_EQ(time_view(), horizontal_label()->parent()->parent());
  EXPECT_FALSE(horizontal_view());
  ASSERT_TRUE(vertical_view());
  EXPECT_FALSE(vertical_view()->parent());
}

// Test `PreferredSizeChanged()` is called when there's a size change of the
// `TimeView`.
TEST_F(TimeViewTest, UpdateSize) {
  // Set current time to 8:00AM for testing.
  task_environment()->AdvanceClock(base::Time::Now().LocalMidnight() +
                                   base::Hours(32) - base::Time::Now());

  // A newly created horizontal clock only has the horizontal label.
  CreateTimeView(TimeView::ClockLayout::HORIZONTAL_CLOCK);
  TimeViewObserver test_observer(time_view());
  test_observer.reset_preferred_size_changed_called();

  EXPECT_FALSE(test_observer.preferred_size_changed_called());

  // Move to 9:59AM. There should be no layout change of the `time_view()`.
  task_environment()->FastForwardBy(base::Minutes(119));
  EXPECT_FALSE(test_observer.preferred_size_changed_called());

  // Move to 10:00AM. There should be a layout change of the `time_view()`.
  task_environment()->FastForwardBy(base::Seconds(61));
  EXPECT_TRUE(test_observer.preferred_size_changed_called());
}

// Test the Date view of the time view.
TEST_F(TimeViewTest, DateView) {
  // A newly created horizontal Date only has the horizontal date view.
  CreateTimeView(TimeView::ClockLayout::HORIZONTAL_CLOCK, TimeView::kDate);
  ASSERT_TRUE(horizontal_date_label()->parent());
  EXPECT_EQ(time_view(), horizontal_date_label()->parent()->parent());
  EXPECT_FALSE(horizontal_date_view());
  ASSERT_TRUE(vertical_date_view());
  EXPECT_FALSE(vertical_date_view()->parent());

  // Switching the date to vertical updates the views.
  time_view()->UpdateClockLayout(TimeView::ClockLayout::VERTICAL_CLOCK);
  ASSERT_TRUE(horizontal_date_view());
  EXPECT_FALSE(horizontal_date_view()->parent());
  EXPECT_FALSE(vertical_date_view());
  ASSERT_TRUE(vertical_date()->parent());
  EXPECT_EQ(time_view(), vertical_date()->parent()->parent());

  // Switching back to horizontal updates the views again.
  time_view()->UpdateClockLayout(TimeView::ClockLayout::HORIZONTAL_CLOCK);
  ASSERT_TRUE(horizontal_date_label()->parent());
  EXPECT_EQ(time_view(), horizontal_date_label()->parent()->parent());
  EXPECT_FALSE(horizontal_date_view());
  ASSERT_TRUE(vertical_date_view());
  EXPECT_FALSE(vertical_date_view()->parent());
}

}  // namespace ash
