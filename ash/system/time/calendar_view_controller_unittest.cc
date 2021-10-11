// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view_controller.h"

#include <string>
#include <utility>

#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

using CalendarViewControllerUnittest = AshTestBase;

TEST_F(CalendarViewControllerUnittest, UtilFunctions) {
  auto controller = std::make_unique<CalendarViewController>();

  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Aug 2021 10:00 GMT", &date));

  controller->UpdateMonth(date);

  base::Time::Exploded first_day_exploded;
  base::Time first_day = controller->GetOnScreenMonthFirstDay();
  first_day.LocalExplode(&first_day_exploded);
  std::u16string month_name = controller->GetOnScreenMonthName();

  EXPECT_EQ(8, first_day_exploded.month);
  EXPECT_EQ(1, first_day_exploded.day_of_month);
  EXPECT_EQ(2021, first_day_exploded.year);
  EXPECT_EQ(u"August", month_name);

  base::Time::Exploded previous_first_day_exploded;
  base::Time previous_first_day = controller->GetPreviousMonthFirstDay(1);
  previous_first_day.LocalExplode(&previous_first_day_exploded);
  std::u16string previous_month_name = controller->GetPreviousMonthName();

  EXPECT_EQ(7, previous_first_day_exploded.month);
  EXPECT_EQ(1, previous_first_day_exploded.day_of_month);
  EXPECT_EQ(2021, previous_first_day_exploded.year);
  EXPECT_EQ(u"July", previous_month_name);

  base::Time::Exploded next_first_day_exploded;
  base::Time next_first_day = controller->GetNextMonthFirstDay(1);
  next_first_day.LocalExplode(&next_first_day_exploded);
  std::u16string next_month_name = controller->GetNextMonthName();

  EXPECT_EQ(9, next_first_day_exploded.month);
  EXPECT_EQ(1, next_first_day_exploded.day_of_month);
  EXPECT_EQ(2021, next_first_day_exploded.year);
  EXPECT_EQ(u"September", next_month_name);
}

TEST_F(CalendarViewControllerUnittest, CornerCases) {
  auto controller = std::make_unique<CalendarViewController>();

  // Next month of Dec should be Jan of next year.
  base::Time last_month_date;
  ASSERT_TRUE(
      base::Time::FromString("24 Dec 2021 10:00 GMT", &last_month_date));

  controller->UpdateMonth(last_month_date);

  base::Time::Exploded january_first_day_exploded;
  base::Time january_first_day = controller->GetNextMonthFirstDay(1);
  january_first_day.LocalExplode(&january_first_day_exploded);
  std::u16string january_month_name = controller->GetNextMonthName();

  EXPECT_EQ(1, january_first_day_exploded.month);
  EXPECT_EQ(1, january_first_day_exploded.day_of_month);
  EXPECT_EQ(2022, january_first_day_exploded.year);
  EXPECT_EQ(u"January", january_month_name);

  // Previous month of Jan should be Dec of last year
  base::Time first_month_date;
  ASSERT_TRUE(
      base::Time::FromString("24 Jan 2021 10:00 GMT", &first_month_date));

  controller->UpdateMonth(first_month_date);

  base::Time::Exploded dec_first_day_exploded;
  base::Time dec_first_day = controller->GetPreviousMonthFirstDay(1);
  dec_first_day.LocalExplode(&dec_first_day_exploded);
  std::u16string dec_month_name = controller->GetPreviousMonthName();

  EXPECT_EQ(12, dec_first_day_exploded.month);
  EXPECT_EQ(1, dec_first_day_exploded.day_of_month);
  EXPECT_EQ(2020, dec_first_day_exploded.year);
  EXPECT_EQ(u"December", dec_month_name);
}

// Testable version of CalendarViewController, into which we can directly inject
// events that CalendarViewController queries via the Google calendar API.
class MockCalendarViewController : public CalendarViewController {
 public:
  MockCalendarViewController() {}
  MockCalendarViewController(const MockCalendarViewController& other) = delete;
  MockCalendarViewController& operator=(
      const MockCalendarViewController& other) = delete;
  ~MockCalendarViewController() override {}

  // Directly invoke the callback with whatever events have been injected.
  void FetchEvents() override {
    OnCalendarEventsFetched(google_apis::HTTP_SUCCESS,
                            std::move(injected_events_));
  }

  // Directly add events to the calendar "service" from which they'll be
  // fetched.
  void InjectEvents(std::unique_ptr<google_apis::calendar::EventList> events) {
    injected_events_ = std::move(events);
  }

 private:
  std::unique_ptr<google_apis::calendar::EventList> injected_events_;
};

class CalendarViewControllerEventsTest : public AshTestBase {
 public:
  CalendarViewControllerEventsTest() = default;
  CalendarViewControllerEventsTest(const CalendarViewControllerEventsTest&) =
      delete;
  CalendarViewControllerEventsTest& operator=(
      const CalendarViewControllerEventsTest&) = delete;
  ~CalendarViewControllerEventsTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<MockCalendarViewController>();
  }

  void TearDown() override {
    controller_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
      const char* id,
      const char* summary,
      const char* start_time,
      const char* end_time) {
    std::unique_ptr<google_apis::calendar::CalendarEvent> event =
        std::make_unique<google_apis::calendar::CalendarEvent>();
    base::Time start_time_base, end_time_base;
    google_apis::calendar::DateTime start_time_date, end_time_date;
    event->set_id(id);
    event->set_summary(summary);
    DCHECK(base::Time::FromString(start_time, &start_time_base));
    DCHECK(base::Time::FromString(end_time, &end_time_base));
    start_time_date.set_date_time(start_time_base);
    end_time_date.set_date_time(end_time_base);
    event->set_start_time(start_time_date);
    event->set_end_time(end_time_date);
    return event;
  }

  bool IsDayWithEvents(const char* day,
                       CalendarViewController::SingleDayEventList* events) {
    base::Time day_base;

    DCHECK(base::Time::FromString(day, &day_base));

    if (events)
      DCHECK(events->empty());

    return controller_->IsDayWithEvents(day_base, events);
  }

  bool IsEventPresent(const char* event_id,
                      CalendarViewController::SingleDayEventList& events) {
    const auto it =
        std::find_if(events.begin(), events.end(),
                     [event_id](google_apis::calendar::CalendarEvent event) {
                       return event.id() == event_id;
                     });
    return it != events.end();
  }

  std::unique_ptr<MockCalendarViewController> controller_;
};

TEST_F(CalendarViewControllerEventsTest, IsDayWithEvents) {
  const char* kStartTime0 = "23 Oct 2009 11:30 GMT";
  const char* kEndTime0 = "23 Oct 2009 12:30 GMT";
  const char* kId0 = "id_0";
  const char* kSummary0 = "summary_0";
  const char* kStartTime1 = "24 Oct 2009 07:30 GMT";
  const char* kEndTime1 = "25 Oct 2009 08:30 GMT";
  const char* kId1 = "id_1";
  const char* kSummary1 = "summary_1";

  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  CalendarViewController::SingleDayEventList events;

  events.clear();
  EXPECT_FALSE(IsDayWithEvents(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  event_list->InjectItemForTesting(std::move(event));
  controller_->InjectEvents(std::move(event_list));
  controller_->FetchEvents();

  events.clear();
  EXPECT_TRUE(IsDayWithEvents(kStartTime0, &events));
  EXPECT_FALSE(events.empty());

  events.clear();
  EXPECT_FALSE(IsDayWithEvents(kStartTime1, &events));
  EXPECT_TRUE(events.empty());

  // Reset/clear all these, as std::move has invalidated them.
  event_list.reset();
  events.clear();

  event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);

  events.clear();
  EXPECT_FALSE(IsDayWithEvents(kStartTime1, &events));
  EXPECT_TRUE(events.empty());

  event_list->InjectItemForTesting(std::move(event1));
  controller_->InjectEvents(std::move(event_list));
  controller_->FetchEvents();

  events.clear();
  EXPECT_TRUE(IsDayWithEvents(kStartTime0, &events));
  EXPECT_FALSE(events.empty());

  events.clear();
  EXPECT_TRUE(IsDayWithEvents(kStartTime1, &events));
  EXPECT_FALSE(events.empty());
}

TEST_F(CalendarViewControllerEventsTest, SameDayReplaceEvents) {
  const char* kStartTime0 = "23 Oct 2009 11:30 GMT";
  const char* kEndTime0 = "23 Oct 2009 12:30 GMT";
  const char* kId0 = "id_0";
  const char* kSummary0 = "summary_0";
  const char* kStartTime1 = "23 Oct 2009 07:30 GMT";
  const char* kEndTime1 = "23 Oct 2009 08:30 GMT";
  const char* kId1 = "id_1";
  const char* kSummary1 = "summary_1";
  const char* kStartTime2 = "23 Oct 2009 04:30 GMT";
  const char* kEndTime2 = "23 Oct 2009 05:30 GMT";
  const char* kId2 = "id_2";
  const char* kSummary2 = "summary_2";

  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  CalendarViewController::SingleDayEventList events;

  events.clear();
  EXPECT_FALSE(IsDayWithEvents(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  event_list->InjectItemForTesting(std::move(event));
  controller_->InjectEvents(std::move(event_list));
  controller_->FetchEvents();

  events.clear();
  EXPECT_TRUE(IsDayWithEvents(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
  EXPECT_TRUE(IsEventPresent(kId0, events));
  EXPECT_FALSE(IsEventPresent(kId1, events));
  EXPECT_FALSE(IsEventPresent(kId2, events));

  // Reset/clear all these, as std::move has invalidated them.
  event_list.reset();
  events.clear();

  event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event2 =
      CreateEvent(kId2, kSummary2, kStartTime2, kEndTime2);
  event_list->InjectItemForTesting(std::move(event1));
  event_list->InjectItemForTesting(std::move(event2));
  controller_->InjectEvents(std::move(event_list));
  controller_->FetchEvents();

  events.clear();
  EXPECT_TRUE(IsDayWithEvents(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 3);
  EXPECT_TRUE(IsEventPresent(kId0, events));
  EXPECT_TRUE(IsEventPresent(kId1, events));
  EXPECT_TRUE(IsEventPresent(kId2, events));
}

}  // namespace ash
