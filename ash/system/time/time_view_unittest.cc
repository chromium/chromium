// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_view.h"

#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace tray {

class TimeViewTest : public AshTestBase {
 public:
  TimeViewTest() = default;
  ~TimeViewTest() override = default;

  void TearDown() override {
    time_view_.reset();
    AshTestBase::TearDown();
  }

  TimeView* time_view() { return time_view_.get(); }

  // Access to private fields of |time_view_|.
  views::View* horizontal_view() { return time_view_->horizontal_view_.get(); }
  views::View* vertical_view() { return time_view_->vertical_view_.get(); }
  views::Label* horizontal_label() { return time_view_->horizontal_label_; }
  views::Label* vertical_label_hours() {
    return time_view_->vertical_label_hours_;
  }
  views::Label* vertical_label_minutes() {
    return time_view_->vertical_label_minutes_;
  }

  // Creates a time view with horizontal or vertical |clock_layout|.
  void CreateTimeView(TimeView::ClockLayout clock_layout) {
    time_view_.reset(
        new TimeView(clock_layout, Shell::Get()->system_tray_model()->clock()));
  }

 private:
  std::unique_ptr<TimeView> time_view_;

  DISALLOW_COPY_AND_ASSIGN(TimeViewTest);
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

}  // namespace tray
}  // namespace ash
