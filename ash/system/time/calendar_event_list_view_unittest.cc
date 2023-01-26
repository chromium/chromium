// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_view.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_event_list_item_view.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

std::unique_ptr<google_apis::calendar::EventList> CreateMockEventList() {
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("Greenwich Mean Time");
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_0", "summary_0", "18 Nov 2021 8:30 GMT", "18 Nov 2021 9:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_1", "summary_1", "18 Nov 2021 8:15 GMT", "18 Nov 2021 11:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_2", "summary_2", "18 Nov 2021 11:30 GMT", "18 Nov 2021 12:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_3", "", "19 Nov 2021 8:30 GMT", "19 Nov 2021 10:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_4", "summary_4", "21 Nov 2021 8:30 GMT", "21 Nov 2021 9:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_5", "summary_5", "21 Nov 2021 10:30 GMT", "21 Nov 2021 11:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_6", "summary_6", "22 Nov 2021 20:30 GMT", "22 Nov 2021 21:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_7", "summary_7", "22 Nov 2021 23:30 GMT", "23 Nov 2021 0:30 GMT"));

  return event_list;
}

}  // namespace

class CalendarViewEventListViewTest : public AshTestBase {
 public:
  CalendarViewEventListViewTest() = default;
  CalendarViewEventListViewTest(const CalendarViewEventListViewTest&) = delete;
  CalendarViewEventListViewTest& operator=(
      const CalendarViewEventListViewTest&) = delete;
  ~CalendarViewEventListViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<CalendarViewController>();
  }

  void TearDown() override {
    event_list_view_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateEventListView(base::Time date) {
    event_list_view_.reset();
    controller_->UpdateMonth(date);
    Shell::Get()->system_tray_model()->calendar_model()->OnEventsFetched(
        calendar_utils::GetStartOfMonthUTC(date),
        google_apis::ApiErrorCode::HTTP_SUCCESS, CreateMockEventList().get());
    controller_->selected_date_ = date;
    event_list_view_ =
        std::make_unique<CalendarEventListView>(controller_.get());
  }

  void RefetchEvents(base::Time start_of_month,
                     const google_apis::calendar::EventList* events) {
    Shell::Get()->system_tray_model()->calendar_model()->OnEventsFetched(
        start_of_month, google_apis::HTTP_SUCCESS, events);
  }

  void SetSelectedDate(base::Time date) {
    controller_->selected_date_ = date;
    controller_->ShowEventListView(/*calendar_date_cell_view=*/nullptr, date,
                                   /*row_index=*/0);
  }

  CalendarEventListView* event_list_view() { return event_list_view_.get(); }
  views::View* content_view() { return event_list_view_->content_view_; }
  CalendarViewController* controller() { return controller_.get(); }

  views::Label* GetSummary(int child_index) {
    return static_cast<views::Label*>(
        static_cast<CalendarEventListItemView*>(
            content_view()->children()[child_index])
            ->summary_);
  }

  views::Label* GetTimeRange(int child_index) {
    return static_cast<views::Label*>(
        static_cast<CalendarEventListItemView*>(
            content_view()->children()[child_index])
            ->time_range_);
  }

  std::u16string GetEmptyLabel() {
    return static_cast<views::LabelButton*>(
               content_view()->children()[0]->children()[0])
        ->GetText();
  }

 private:
  std::unique_ptr<CalendarEventListView> event_list_view_;
  std::unique_ptr<CalendarViewController> controller_;
  base::test::ScopedFeatureList features_;
};

TEST_F(CalendarViewEventListViewTest, ShowEvents) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));

  CreateEventListView(date - base::Days(1));

  // No events on 17 Nov 2021, so we see the empty list default.
  EXPECT_EQ(1u, content_view()->children().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS),
            GetEmptyLabel());

  SetSelectedDate(date);

  // 3 events on 18 Nov 2021. And they should be sorted by the start time.
  EXPECT_EQ(3u, content_view()->children().size());
  EXPECT_EQ(u"summary_1", GetSummary(0)->GetText());
  EXPECT_EQ(u"summary_0", GetSummary(1)->GetText());
  EXPECT_EQ(u"summary_2", GetSummary(2)->GetText());

  SetSelectedDate(date + base::Days(1));

  // 1 event on 19 Nov 2021. For no title mettings, shows "No title" as the
  // meeting summary.
  EXPECT_EQ(1u, content_view()->children().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_TITLE),
            GetSummary(0)->GetText());

  SetSelectedDate(date + base::Days(2));

  // 0 event on 20 Nov 2021.
  EXPECT_EQ(1u, content_view()->children().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS),
            GetEmptyLabel());

  SetSelectedDate(date + base::Days(3));

  // 2 events on 21 Nov 2021.
  EXPECT_EQ(2u, content_view()->children().size());
  EXPECT_EQ(u"summary_4", GetSummary(0)->GetText());
  EXPECT_EQ(u"summary_5", GetSummary(1)->GetText());
}

TEST_F(CalendarViewEventListViewTest, LaunchEmptyList) {
  base::HistogramTester histogram_tester;
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  CreateEventListView(date - base::Days(1));

  // No events, so we see the empty list by default.
  EXPECT_EQ(1u, content_view()->children().size());
  views::Button* empty_list_button =
      static_cast<views::Button*>(content_view()->children()[0]->children()[0]);
  empty_list_button->AcceleratorPressed(
      ui::Accelerator(ui::KeyboardCode::VKEY_SPACE, /*modifiers=*/0));

  histogram_tester.ExpectTotalCount(
      "Ash.Calendar.UserJourneyTime.EventLaunched", 1);
}

TEST_F(CalendarViewEventListViewTest, LaunchItem) {
  base::HistogramTester histogram_tester;
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  CreateEventListView(date);

  SetSelectedDate(date);
  EXPECT_EQ(3u, content_view()->children().size());

  // Launch the first item.
  views::Button* first_item =
      static_cast<views::Button*>(content_view()->children()[0]);
  first_item->AcceleratorPressed(
      ui::Accelerator(ui::KeyboardCode::VKEY_SPACE, /*modifiers=*/0));

  histogram_tester.ExpectTotalCount(
      "Ash.Calendar.UserJourneyTime.EventLaunched", 1);
  histogram_tester.ExpectTotalCount("Ash.Calendar.EventListItem.Activated", 1);
}

TEST_F(CalendarViewEventListViewTest, CheckTimeFormat) {
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  // Date of first day which holds a normal event and a multi-day event.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 10:00 GMT", &date));

  // Date of the second day which holds the second day of the multi-day event.
  base::Time date_2;
  ASSERT_TRUE(base::Time::FromString("23 Nov 2021 10:00 GMT", &date_2));

  // Set the time in AM/PM format.
  Shell::Get()->system_tray_model()->SetUse24HourClock(false);

  CreateEventListView(date);

  SetSelectedDate(date);
  EXPECT_EQ(u"8:30\u2009–\u20099:30\u202fPM", GetTimeRange(0)->GetText());
  EXPECT_EQ(u"11:30\u2009–\u200911:59\u202fPM", GetTimeRange(1)->GetText());

  // Select the second day of the multi-day event.
  SetSelectedDate(date_2);
  EXPECT_EQ(u"12:00\u2009–\u200912:30\u202fAM", GetTimeRange(0)->GetText());

  // Set the time in 24 hour format.
  Shell::Get()->system_tray_model()->SetUse24HourClock(true);

  // Regenerate the event list to refresh events time range.
  CreateEventListView(date);

  SetSelectedDate(date);
  EXPECT_EQ(u"20:30\u2009–\u200921:30", GetTimeRange(0)->GetText());
  EXPECT_EQ(u"23:30\u2009–\u200923:59", GetTimeRange(1)->GetText());

  SetSelectedDate(date_2);
  EXPECT_EQ(u"00:00\u2009–\u200900:30", GetTimeRange(0)->GetText());
}

TEST_F(CalendarViewEventListViewTest, RefreshEvents) {
  // Sets the timezone to "America/Los_Angeles".
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  CreateEventListView(date);

  SetSelectedDate(date);

  // With the initial event list there should be 3 events on the 18th.
  EXPECT_EQ(3u, content_view()->children().size());

  base::Time start_of_month = calendar_utils::GetStartOfMonthUTC(
      controller()->selected_date_midnight());
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_4", "summary_4", "21 Nov 2021 8:30 GMT", "21 Nov 2021 9:30 GMT"));

  // Calls the `OnEventsFetched` method to update the events in the model.
  // The event list view should be re-rendered automatically with the new event
  // list.
  RefetchEvents(start_of_month, event_list.get());

  // Shows 0 events and shows open in google calendar button after the refresh.
  EXPECT_EQ(1u, content_view()->children().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS),
            GetEmptyLabel());

  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_0", "summary_0", "18 Nov 2021 8:30 GMT", "18 Nov 2021 9:30 GMT"));
  RefetchEvents(start_of_month, event_list.get());

  // Shows 1 event after the refresh.
  EXPECT_EQ(1u, content_view()->children().size());
  EXPECT_EQ(u"summary_0", GetSummary(0)->GetText());
}

}  // namespace ash
