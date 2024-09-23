// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/ax_event_counter.h"
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
  views::View* horizontal_time_label_container() {
    return time_view_->horizontal_time_label_container_.get();
  }
  views::View* vertical_time_label_container() {
    return time_view_->vertical_time_label_container_.get();
  }
  views::View* horizontal_date_label_container() {
    return time_view_->horizontal_date_label_container_.get();
  }
  views::View* vertical_date_view_container() {
    return time_view_->vertical_date_view_container_.get();
  }
  views::Label* horizontal_time_label_() {
    return time_view_->horizontal_time_label_;
  }
  views::Label* vertical_label_hours() {
    return time_view_->vertical_label_hours_;
  }

  views::Label* vertical_label_minutes() {
    return time_view_->vertical_label_minutes_;
  }
  views::Label* horizontal_date_label() {
    return time_view_->horizontal_date_label_;
  }
  VerticalDateView* vertical_date() { return time_view_->vertical_date_view_; }

  views::Label* vertical_date_label() {
    return time_view_->vertical_date_view_
               ? time_view_->vertical_date_view_->text_label_.get()
               : nullptr;
  }

  void UpdateText() { time_view_->UpdateText(); }

  // Creates a time view with horizontal or vertical |clock_layout|.
  void CreateTimeView(TimeView::ClockLayout clock_layout,
                      TimeView::Type type = TimeView::kTime) {
    time_view_ = widget_->SetContentsView(std::make_unique<TimeView>(
        clock_layout, Shell::Get()->system_tray_model()->clock(), type));
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  // Owned by `widget_`.
  raw_ptr<TimeView, DanglingUntriaged> time_view_;
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
  EXPECT_TRUE(horizontal_time_label_container()->GetVisible());
  ASSERT_FALSE(vertical_time_label_container()->GetVisible());

  // Switching the clock to vertical updates the labels.
  time_view()->UpdateClockLayout(TimeView::ClockLayout::VERTICAL_CLOCK);
  ASSERT_FALSE(horizontal_time_label_container()->GetVisible());
  EXPECT_TRUE(vertical_time_label_container()->GetVisible());

  // Switching back to horizontal updates the labels again.
  time_view()->UpdateClockLayout(TimeView::ClockLayout::HORIZONTAL_CLOCK);
  EXPECT_TRUE(horizontal_time_label_container()->GetVisible());
  ASSERT_FALSE(vertical_time_label_container()->GetVisible());
}

// Test accessibility events emitted by the time view's labels during updates.
TEST_F(TimeViewTest, TimeViewFiresAccessibilityEvents) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());

  CreateTimeView(TimeView::ClockLayout::HORIZONTAL_CLOCK);

  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_time_label_()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_hours()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_minutes()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, time_view()));

  // Set current time to 08:00.
  // There should be one text-changed accessibility event for each time-related
  // label, none for the date-related labels, and one for the time view button.
  task_environment()->AdvanceClock(base::Time::Now().LocalMidnight() +
                                   base::Hours(32) - base::Time::Now());
  UpdateText();

  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_date_label()));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_time_label_()));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_hours()));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_minutes()));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged, time_view()));

  // Changing the layout does not change the text. Hence no text-changed events
  // are fired.
  counter.ResetAllCounts();
  time_view()->UpdateClockLayout(TimeView::ClockLayout::VERTICAL_CLOCK);
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_time_label_()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_hours()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_minutes()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, time_view()));

  // Call update text when the time has not changed. Because the time has not
  // changed, the text has not changed. Hence no text-changed events are fired.
  counter.ResetAllCounts();
  UpdateText();
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_time_label_()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_hours()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_minutes()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, time_view()));

  // Move to 08:01 and update the text again. There should be one text-changed
  // accessibility event for each time-related label whose text has changed,
  // i.e. the horizontal label and the vertical minutes label. The time view
  // button should also fire an event since the displayed text changed.
  counter.ResetAllCounts();
  task_environment()->FastForwardBy(base::Minutes(1));
  UpdateText();
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_date_label()));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_time_label_()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_hours()));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_minutes()));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged, time_view()));
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

// Test that for horizontal clocks, the "AM/PM" text can be enabled and
// disabled.
TEST_F(TimeViewTest, EnableAmPmText) {
  // Set current time to 8:00AM for testing.
  task_environment()->AdvanceClock(base::Time::Now().LocalMidnight() +
                                   base::Hours(32) - base::Time::Now());

  // A newly created horizontal clock only has the horizontal label.
  CreateTimeView(TimeView::ClockLayout::HORIZONTAL_CLOCK, TimeView::kTime);

  // Ensure that the "AM/PM" flag is disabled by default.
  ASSERT_EQ(time_view()->GetAmPmClockTypeForTesting(),
            base::AmPmClockType::kDropAmPm);
  auto* horizontal_label = time_view()->GetHorizontalTimeLabelForTesting();

  // Ensure that the "AM/PM" text isn't visible.
  ASSERT_FALSE(horizontal_label->GetText().ends_with(u"AM"));
  ASSERT_FALSE(horizontal_label->GetText().ends_with(u"PM"));

  // Ensure that the "AM/PM" flag can be enabled.
  time_view()->SetAmPmClockType(base::AmPmClockType::kKeepAmPm);
  ASSERT_EQ(time_view()->GetAmPmClockTypeForTesting(),
            base::AmPmClockType::kKeepAmPm);

  // Ensure that the "AM/PM" text is visible.
  ASSERT_TRUE(horizontal_label->GetText().ends_with(u"AM"));

  // Advance time by 12 hours.
  task_environment()->FastForwardBy(base::Hours(12));

  // Ensure that the transition from "AM" to "PM" occurs as time moves.
  ASSERT_TRUE(horizontal_label->GetText().ends_with(u"PM"));

  // Ensure that the "AM/PM" text isn't visible in vertical clocks.
  time_view()->UpdateClockLayout(TimeView::ClockLayout::VERTICAL_CLOCK);
  auto* vertical_minutes_label =
      time_view()->GetVerticalMinutesLabelForTesting();
  auto* vertical_hours_label = time_view()->GetVerticalHoursLabelForTesting();
  ASSERT_FALSE(vertical_minutes_label->GetText().ends_with(u"AM"));
  ASSERT_FALSE(vertical_hours_label->GetText().ends_with(u"PM"));
}

// Test the Date view of the time view.
TEST_F(TimeViewTest, DateView) {
  // A newly created horizontal Date only has the horizontal date view.
  CreateTimeView(TimeView::ClockLayout::HORIZONTAL_CLOCK, TimeView::kDate);
  EXPECT_TRUE(horizontal_date_label_container()->GetVisible());
  EXPECT_FALSE(vertical_date_view_container()->GetVisible());

  // Switching the date to vertical updates the views.
  time_view()->UpdateClockLayout(TimeView::ClockLayout::VERTICAL_CLOCK);
  EXPECT_FALSE(horizontal_date_label_container()->GetVisible());
  EXPECT_TRUE(vertical_date_view_container()->GetVisible());

  // Switching back to horizontal updates the views again.
  time_view()->UpdateClockLayout(TimeView::ClockLayout::HORIZONTAL_CLOCK);
  EXPECT_TRUE(horizontal_date_label_container()->GetVisible());
  EXPECT_FALSE(vertical_date_view_container()->GetVisible());
}

// Test accessibility events emitted by the date view's labels during updates.
TEST_F(TimeViewTest, DateViewFiresAccessibilityEvents) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());

  CreateTimeView(TimeView::ClockLayout::HORIZONTAL_CLOCK, TimeView::kDate);
  // We shouldn't fire any events through the construction of the view with
  // default values.
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_time_label_()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_hours()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_minutes()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, time_view()));

  // Set current time to 08:00.
  // There should be one text-changed accessibility event for each date-related
  // label, none for the time-related labels, and one for the time view button.
  task_environment()->AdvanceClock(base::Time::Now().LocalMidnight() +
                                   base::Hours(32) - base::Time::Now());
  UpdateText();

  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_time_label_()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_hours()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_minutes()));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_date_label()));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_date_label()));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged, time_view()));

  // Changing the layout doesn't change the text. Hence no text-changed events
  // are fired.
  counter.ResetAllCounts();
  time_view()->UpdateClockLayout(TimeView::ClockLayout::VERTICAL_CLOCK);
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, time_view()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_time_label_()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_hours()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_minutes()));

  // Call update text when the time has not changed. Because the time has not
  // changed, the text has not changed. Hence no text-changed events are fired.
  counter.ResetAllCounts();
  UpdateText();
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_time_label_()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_hours()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_minutes()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, time_view()));

  // Move to 08:01 and update the text again. Because this is the date view, we
  // do not have any accessibility events for text changing in the time-related
  // labels. And because the date has not changed, we should not have any events
  // for the date-related labels either. The time view button should fire an
  // event since the displayed text changed.
  counter.ResetAllCounts();
  task_environment()->FastForwardBy(base::Minutes(1));
  UpdateText();
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_time_label_()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_hours()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_label_minutes()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                horizontal_date_label()));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged,
                                vertical_date_label()));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged, time_view()));
}

TEST_F(TimeViewTest, AccessibleProperties) {
  CreateTimeView(TimeView::ClockLayout::HORIZONTAL_CLOCK);
  ui::AXNodeData data;

  time_view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kTime);
}

}  // namespace ash
