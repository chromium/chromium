// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/timestamp_view.h"

#include "ash/test/ash_test_base.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/widget/widget.h"

namespace ash {

class TimestampViewTest : public AshTestBase {
 public:
  TimestampViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  TimestampViewTest(const TimestampViewTest&) = delete;
  TimestampViewTest& operator=(const TimestampViewTest&) = delete;
  ~TimestampViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // `widget_` owns `timestamp_view_`.
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    timestamp_view_ = widget_->GetContentsView()->AddChildView(
        std::make_unique<TimestampView>());
  }

  void TearDown() override {
    timestamp_view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

 protected:
  TimestampView* timestamp_view() { return timestamp_view_.get(); }

 private:
  raw_ptr<TimestampView> timestamp_view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(TimestampViewTest, TimestampTextUpdatedOverTime) {
  EXPECT_TRUE(timestamp_view()->GetVisible());

  timestamp_view()->SetTimestamp(base::Time::Now() + base::Hours(3) +
                                 base::Minutes(30));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 3),
            timestamp_view()->GetText());

  task_environment()->AdvanceClock(base::Hours(3));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 30),
            timestamp_view()->GetText());

  task_environment()->AdvanceClock(base::Minutes(30));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      timestamp_view()->GetText());

  task_environment()->AdvanceClock(base::Days(2));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST, 2),
            timestamp_view()->GetText());
}

}  // namespace ash
