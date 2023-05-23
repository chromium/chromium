// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_view.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_event_list_item_view.h"
#include "ash/system/time/calendar_event_list_item_view_jelly.h"
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

class CalendarViewEventListViewTest : public AshTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  CalendarViewEventListViewTest() = default;
  CalendarViewEventListViewTest(const CalendarViewEventListViewTest&) = delete;
  CalendarViewEventListViewTest& operator=(
      const CalendarViewEventListViewTest&) = delete;
  ~CalendarViewEventListViewTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(features::kCalendarJelly,
                                              IsCalendarJellyEnabled());
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

  // The way we send metrics is slightly different for Jelly so we need to
  // ensure this value is set to true in the controller.
  void SetEventListIsShowingForMetrics() {
    controller_->is_event_list_showing_ = true;
  }

  CalendarEventListView* event_list_view() { return event_list_view_.get(); }
  views::View* content_view() { return event_list_view_->content_view_; }
  CalendarViewController* controller() { return controller_.get(); }

  views::View* GetSameDayEventsContainer() {
    views::View* container =
        content_view()->GetViewByID(kEventListSameDayEventsContainer);
    CHECK(container);

    return container;
  }

  views::Label* GetSummary(int child_index) {
    return features::IsCalendarJellyEnabled()
               ? static_cast<views::Label*>(
                     static_cast<CalendarEventListItemViewJelly*>(
                         GetSameDayEventsContainer()->children()[child_index])
                         ->GetViewByID(kSummaryLabelID))
               : static_cast<views::Label*>(
                     static_cast<CalendarEventListItemView*>(
                         content_view()->children()[child_index])
                         ->summary_);
  }

  std::u16string GetEmptyLabel() {
    return static_cast<views::LabelButton*>(
               content_view()->children()[0]->children()[0])
        ->GetText();
  }

  ActionableView* GetActionableView(int child_index) {
    return features::IsCalendarJellyEnabled()
               ? static_cast<ActionableView*>(
                     GetSameDayEventsContainer()->children()[child_index])
               : static_cast<ActionableView*>(
                     content_view()->children()[child_index]);
  }

  size_t GetContentViewSize() {
    return features::IsCalendarJellyEnabled()
               ? GetSameDayEventsContainer()->children().size()
               : content_view()->children().size();
  }

  size_t GetEmptyContentViewSize() { return content_view()->children().size(); }

  bool IsCalendarJellyEnabled() { return GetParam(); }

 private:
  std::unique_ptr<CalendarEventListView> event_list_view_;
  std::unique_ptr<CalendarViewController> controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, CalendarViewEventListViewTest, testing::Bool());

TEST_P(CalendarViewEventListViewTest, ShowEvents) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));

  CreateEventListView(date - base::Days(1));

  // No events on 17 Nov 2021, so we see the empty list default.
  EXPECT_EQ(1u, GetEmptyContentViewSize());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS),
            GetEmptyLabel());

  SetSelectedDate(date);

  // 3 events on 18 Nov 2021. And they should be sorted by the start time.
  EXPECT_EQ(3u, GetContentViewSize());
  EXPECT_EQ(u"summary_1", GetSummary(0)->GetText());
  EXPECT_EQ(u"summary_0", GetSummary(1)->GetText());
  EXPECT_EQ(u"summary_2", GetSummary(2)->GetText());

  SetSelectedDate(date + base::Days(1));

  // 1 event on 19 Nov 2021. For no title mettings, shows "No title" as the
  // meeting summary.
  EXPECT_EQ(1u, GetContentViewSize());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_TITLE),
            GetSummary(0)->GetText());

  SetSelectedDate(date + base::Days(2));

  // 0 event on 20 Nov 2021.
  EXPECT_EQ(1u, GetEmptyContentViewSize());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS),
            GetEmptyLabel());

  SetSelectedDate(date + base::Days(3));

  // 2 events on 21 Nov 2021.
  EXPECT_EQ(2u, GetContentViewSize());
  EXPECT_EQ(u"summary_4", GetSummary(0)->GetText());
  EXPECT_EQ(u"summary_5", GetSummary(1)->GetText());
}

TEST_P(CalendarViewEventListViewTest, LaunchEmptyList) {
  base::HistogramTester histogram_tester;
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  CreateEventListView(date - base::Days(1));

  // No events, so we see the empty list by default.
  EXPECT_EQ(1u, GetEmptyContentViewSize());
  views::Button* empty_list_button =
      static_cast<views::Button*>(content_view()->children()[0]->children()[0]);
  empty_list_button->AcceleratorPressed(
      ui::Accelerator(ui::KeyboardCode::VKEY_SPACE, /*modifiers=*/0));

  histogram_tester.ExpectTotalCount(
      "Ash.Calendar.UserJourneyTime.EventLaunched", 1);
  EXPECT_EQ(histogram_tester.GetTotalSum(
                "Ash.Calendar.EventListView.EventDisplayedCount"),
            0);
}

TEST_P(CalendarViewEventListViewTest, LaunchItem) {
  base::HistogramTester histogram_tester;
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  CreateEventListView(date);
  if (features::IsCalendarJellyEnabled()) {
    SetEventListIsShowingForMetrics();
  }
  EXPECT_EQ(3u, GetContentViewSize());

  // Launch the first item.
  ActionableView* first_item = GetActionableView(0);
  first_item->AcceleratorPressed(
      ui::Accelerator(ui::KeyboardCode::VKEY_SPACE, /*modifiers=*/0));

  histogram_tester.ExpectTotalCount(
      "Ash.Calendar.UserJourneyTime.EventLaunched", 1);
  histogram_tester.ExpectTotalCount("Ash.Calendar.EventListItem.Activated", 1);
  EXPECT_EQ(histogram_tester.GetTotalSum(
                features::IsCalendarJellyEnabled()
                    ? "Ash.Calendar.EventListViewJelly.EventDisplayedCount"
                    : "Ash.Calendar.EventListView.EventDisplayedCount"),
            3);
}

TEST_P(CalendarViewEventListViewTest, RefreshEvents) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  CreateEventListView(date);

  SetSelectedDate(date);

  // With the initial event list there should be 3 events on the 18th.
  EXPECT_EQ(3u, GetContentViewSize());

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
  EXPECT_EQ(1u, GetEmptyContentViewSize());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS),
            GetEmptyLabel());

  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_0", "summary_0", "18 Nov 2021 8:30 GMT", "18 Nov 2021 9:30 GMT"));
  RefetchEvents(start_of_month, event_list.get());

  // Shows 1 event after the refresh.
  EXPECT_EQ(1u, GetContentViewSize());
  EXPECT_EQ(u"summary_0", GetSummary(0)->GetText());
}

}  // namespace ash
