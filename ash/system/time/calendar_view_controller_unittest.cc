// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view_controller.h"

#include <string>
#include <utility>

#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

using CalendarViewControllerUnittest = AshTestBase;

TEST_F(CalendarViewControllerUnittest, UtilFunctions) {
  auto controller = std::make_unique<CalendarViewController>();

  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Aug 2021 10:00 GMT", &date));

  controller->UpdateMonth(date);

  base::Time::Exploded first_day_exploded;
  base::Time first_day = controller->GetOnScreenMonthFirstDayLocal();
  first_day.LocalExplode(&first_day_exploded);
  std::u16string month_name = controller->GetOnScreenMonthName();

  EXPECT_EQ(8, first_day_exploded.month);
  EXPECT_EQ(1, first_day_exploded.day_of_month);
  EXPECT_EQ(2021, first_day_exploded.year);
  EXPECT_EQ(u"August", month_name);

  base::Time::Exploded previous_first_day_exploded;
  base::Time previous_first_day = controller->GetPreviousMonthFirstDayLocal(1);
  previous_first_day.LocalExplode(&previous_first_day_exploded);
  std::u16string previous_month_name = controller->GetPreviousMonthName();

  EXPECT_EQ(7, previous_first_day_exploded.month);
  EXPECT_EQ(1, previous_first_day_exploded.day_of_month);
  EXPECT_EQ(2021, previous_first_day_exploded.year);
  EXPECT_EQ(u"July", previous_month_name);

  base::Time::Exploded next_first_day_exploded;
  base::Time next_first_day = controller->GetNextMonthFirstDayLocal(1);
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
  base::Time january_first_day = controller->GetNextMonthFirstDayLocal(1);
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
  base::Time dec_first_day = controller->GetPreviousMonthFirstDayLocal(1);
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
  MockCalendarViewController() = default;
  MockCalendarViewController(const MockCalendarViewController& other) = delete;
  MockCalendarViewController& operator=(
      const MockCalendarViewController& other) = delete;
  ~MockCalendarViewController() override = default;

  std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
      const char* id,
      const char* summary,
      base::Time start_time,
      base::Time end_time) {
    std::unique_ptr<google_apis::calendar::CalendarEvent> event =
        std::make_unique<google_apis::calendar::CalendarEvent>();
    google_apis::calendar::DateTime start_time_date, end_time_date;
    event->set_id(id);
    event->set_summary(summary);
    google_apis::calendar::DateTime start_date, end_date;
    start_date.set_date_time(start_time);
    end_date.set_date_time(end_time);
    event->set_start_time(start_date);
    event->set_end_time(start_date);
    return event;
  }

  void MaybeFetchMonth(base::Time start_of_month) override {
    if (IsMonthAlreadyFetched(start_of_month))
      return;

    // Month has officially been fetched.
    MarkMonthAsFetched(start_of_month);

    std::unique_ptr<google_apis::calendar::EventList> fetched_events =
        std::make_unique<google_apis::calendar::EventList>();
    base::Time::Exploded exp_month_start;
    start_of_month.UTCExplode(&exp_month_start);
    for (auto& event : injected_events_->items()) {
      base::Time::Exploded exp_event_start;
      event->start_time().date_time().UTCExplode(&exp_event_start);
      if ((exp_month_start.month == exp_event_start.month) &&
          (exp_month_start.year == exp_event_start.year)) {
        std::unique_ptr<google_apis::calendar::CalendarEvent> single_event =
            CreateEvent(event->id().c_str(), event->summary().c_str(),
                        event->start_time().date_time(),
                        event->end_time().date_time());
        fetched_events->InjectItemForTesting(std::move(single_event));
      }
    }

    // Add an empty month to `event_months_` to indicate this is a month we've
    // fetched.
    SingleMonthEventMap empty_month;
    event_months_.emplace(start_of_month, empty_month);

    // Receive the results of the fetch.  Check whether we've set an error code
    // for start_of_month.
    OnCalendarEventsFetched(GetFetchErrorCode(start_of_month),
                            std::move(fetched_events));
  }

  // Directly add events to the calendar "service" from which they'll be
  // fetched.
  void InjectEvents(std::unique_ptr<google_apis::calendar::EventList> events) {
    injected_events_ = std::move(events);
  }

  // For testing of event-fetching error cases.  Specify the error codes we want
  // fetches of the previous, current, and next months' events to return.
  void SetFetchErrors(base::Time current_date,
                      google_apis::ApiErrorCode prev,
                      google_apis::ApiErrorCode current,
                      google_apis::ApiErrorCode next) {
    ResetFetchErrors();
    fetch_errors_.emplace(
        calendar_utils::GetStartOfPreviousMonthUTC(current_date), prev);
    fetch_errors_.emplace(calendar_utils::GetStartOfMonthUTC(current_date),
                          current);
    fetch_errors_.emplace(calendar_utils::GetStartOfNextMonthUTC(current_date),
                          next);
  }
  void ResetFetchErrors() { fetch_errors_.clear(); }
  google_apis::ApiErrorCode GetFetchErrorCode(base::Time date) {
    google_apis::ApiErrorCode error = google_apis::HTTP_SUCCESS;
    auto it = fetch_errors_.find(date);
    if (it != fetch_errors_.end())
      error = it->second;
    return error;
  }

 private:
  std::unique_ptr<google_apis::calendar::EventList> injected_events_;
  std::map<base::Time, google_apis::ApiErrorCode> fetch_errors_;
};

class CalendarViewControllerEventsTest : public AshTestBase {
 public:
  CalendarViewControllerEventsTest() = default;
  CalendarViewControllerEventsTest(const CalendarViewControllerEventsTest&) =
      delete;
  CalendarViewControllerEventsTest& operator=(
      const CalendarViewControllerEventsTest&) = delete;
  ~CalendarViewControllerEventsTest() override = default;

  void TearDown() override {
    controller_.reset();
    AshTestBase::TearDown();
  }

  int EventsNumberOfDay(const char* day, SingleDayEventList* events) {
    base::Time day_base;

    bool result = base::Time::FromString(day, &day_base);
    DCHECK(result);

    if (events)
      DCHECK(events->empty());

    return controller_->EventsNumberOfDay(day_base, events);
  }

  int EventsNumberOfDayInternal(const char* day,
                                SingleDayEventList* events) const {
    base::Time day_base;

    bool result = base::Time::FromString(day, &day_base);
    DCHECK(result);

    if (events)
      DCHECK(events->empty());

    return controller_->EventsNumberOfDayInternal(day_base, events);
  }

  bool IsEventPresent(const char* event_id, SingleDayEventList& events) {
    const auto it =
        std::find_if(events.begin(), events.end(),
                     [event_id](google_apis::calendar::CalendarEvent event) {
                       return event.id() == event_id;
                     });
    return it != events.end();
  }

  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }
  static base::Time FakeTimeNow() { return fake_time_; }

  std::unique_ptr<MockCalendarViewController> controller_;
  static base::Time fake_time_;
};

base::Time CalendarViewControllerEventsTest::fake_time_;

TEST_F(CalendarViewControllerEventsTest, DayWithEvents_OneDay) {
  const char* kStartTime = "23 Oct 2009 11:30 GMT";
  const char* kEndTime = "23 Oct 2009 12:30 GMT";
  const char* kId = "id_0";
  const char* kSummary = "summary_0";

  // Current date is just kStartTime.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewControllerEventsTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  controller_ = std::make_unique<MockCalendarViewController>();

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId, kSummary, kStartTime, kEndTime);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on kStartTime0.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  controller_->InjectEvents(std::move(event_list));

  // Now fetch the events, which will get all events from the current month, as
  // well as next/prev months.
  controller_->FetchEvents();

  // Now we have an event on kStartTime0.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
}

TEST_F(CalendarViewControllerEventsTest, DayWithEvents_TwoDays) {
  const char* kStartTime0 = "23 Oct 2009 11:30 GMT";
  const char* kEndTime0 = "23 Oct 2009 12:30 GMT";
  const char* kId0 = "id_0";
  const char* kSummary0 = "summary_0";
  const char* kStartTime1 = "24 Oct 2009 07:30 GMT";
  const char* kEndTime1 = "25 Oct 2009 08:30 GMT";
  const char* kId1 = "id_1";
  const char* kSummary1 = "summary_1";

  // Current date is just kStartTime0.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime0, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewControllerEventsTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  controller_ = std::make_unique<MockCalendarViewController>();

  // Get ready to inject two events.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      calendar_test_utils::CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on kStartTime0 or kStartTime1.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime1, &events));
  EXPECT_TRUE(events.empty());

  // Inject both events.
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event1));
  controller_->InjectEvents(std::move(event_list));
  controller_->FetchEvents();

  // Now both days should have events.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime1, &events));
  EXPECT_FALSE(events.empty());
}

TEST_F(CalendarViewControllerEventsTest, OnlyFetchOnce) {
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

  // Current date is just kStartTime0.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime0, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewControllerEventsTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  controller_ = std::make_unique<MockCalendarViewController>();

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  SingleDayEventList events;

  // No events at kStartTime0.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  // Inject one event, pretend the user just added it somewhere else.
  event_list->InjectItemForTesting(std::move(event));
  controller_->InjectEvents(std::move(event_list));

  // Fetch events, pretend the user just brought up the CrOS calendar.
  controller_->FetchEvents();

  // Confirm we have only event 0 and NOT events 1 or 2.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
  EXPECT_TRUE(IsEventPresent(kId0, events));
  EXPECT_FALSE(IsEventPresent(kId1, events));
  EXPECT_FALSE(IsEventPresent(kId2, events));

  // Reset/clear all these, as std::move has invalidated them.
  event_list.reset();
  events.clear();

  // Inject two more events.
  event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      calendar_test_utils::CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event2 =
      calendar_test_utils::CreateEvent(kId2, kSummary2, kStartTime2, kEndTime2);
  event_list->InjectItemForTesting(std::move(event1));
  event_list->InjectItemForTesting(std::move(event2));
  controller_->InjectEvents(std::move(event_list));
  controller_->FetchEvents();

  // Verify that we still see the first event but neither of the new events,
  // because as far as the controller is concerned we've already fetched this
  // month.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
  EXPECT_TRUE(IsEventPresent(kId0, events));
  EXPECT_FALSE(IsEventPresent(kId1, events));
  EXPECT_FALSE(IsEventPresent(kId2, events));
}

TEST_F(CalendarViewControllerEventsTest, EventsDifferentMonths) {
  const char* kStartTime0 = "23 Oct 2009 11:30 GMT";
  const char* kEndTime0 = "23 Oct 2009 12:30 GMT";
  const char* kId0 = "id_0";
  const char* kSummary0 = "summary_0";
  const char* kStartTime1 = "23 Nov 2009 07:30 GMT";
  const char* kEndTime1 = "23 Nov 2009 08:30 GMT";
  const char* kId1 = "id_1";
  const char* kSummary1 = "summary_1";
  const char* kStartTime2 = "23 Dec 2009 04:30 GMT";
  const char* kEndTime2 = "23 Dec 2009 05:30 GMT";
  const char* kId2 = "id_2";
  const char* kSummary2 = "summary_2";

  // Current date is just kStartTime1.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime1, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewControllerEventsTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  controller_ = std::make_unique<MockCalendarViewController>();

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      calendar_test_utils::CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event2 =
      calendar_test_utils::CreateEvent(kId2, kSummary2, kStartTime2, kEndTime2);
  SingleDayEventList events;

  // No events on any day.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime1, &events));
  EXPECT_TRUE(events.empty());
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime2, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (user added them to their calendar).
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event1));
  event_list->InjectItemForTesting(std::move(event2));
  controller_->InjectEvents(std::move(event_list));

  // Fetch events (user just opened CrOS calendar).
  controller_->FetchEvents();

  // Confirm we have all three events.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
  EXPECT_TRUE(IsEventPresent(kId0, events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime1, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
  EXPECT_TRUE(IsEventPresent(kId1, events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime2, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
  EXPECT_TRUE(IsEventPresent(kId2, events));
}

// crbug:1256500 has been filed to track the effort of adding more test coverage
// of the pruning cases if needed.
TEST_F(CalendarViewControllerEventsTest, PruneEvents) {
  const char* kStartTime0 = "23 Oct 2009 11:30 GMT";
  const char* kEndTime0 = "23 Oct 2009 12:30 GMT";
  const char* kId0 = "id_0";
  const char* kSummary0 = "summary_0";
  const char* kStartTime1 = "23 Nov 2009 07:30 GMT";
  const char* kEndTime1 = "23 Nov 2009 08:30 GMT";
  const char* kId1 = "id_1";
  const char* kSummary1 = "summary_1";
  const char* kStartTime2 = "23 Dec 2009 11:30 GMT";
  const char* kEndTime2 = "23 Dec 2009 12:30 GMT";
  const char* kId2 = "id_2";
  const char* kSummary2 = "summary_2";
  const char* kStartTime3 = "23 Jan 2010 11:30 GMT";
  const char* kEndTime3 = "23 Jan 2010 12:30 GMT";
  const char* kId3 = "id_3";
  const char* kSummary3 = "summary_3";
  const char* kStartTime4 = "23 Feb 2010 11:30 GMT";
  const char* kEndTime4 = "23 Feb 2010 12:30 GMT";
  const char* kId4 = "id_4";
  const char* kSummary4 = "summary_4";
  const char* kStartTime5 = "23 Mar 2010 11:30 GMT";
  const char* kEndTime5 = "23 Mar 2010 12:30 GMT";
  const char* kId5 = "id_5";
  const char* kSummary5 = "summary_5";
  const char* kStartTime6 = "23 Apr 2010 11:30 GMT";
  const char* kEndTime6 = "23 Apr 2010 12:30 GMT";
  const char* kId6 = "id_6";
  const char* kSummary6 = "summary_6";
  const char* kStartTime7 = "23 May 2010 11:30 GMT";
  const char* kEndTime7 = "23 May 2010 12:30 GMT";
  const char* kId7 = "id_7";
  const char* kSummary7 = "summary_7";
  const char* kStartTime8 = "23 Jun 2010 11:30 GMT";
  const char* kEndTime8 = "23 Jun 2010 12:30 GMT";
  const char* kId8 = "id_8";
  const char* kSummary8 = "summary_8";

  // Current time is kStartTime1, which means event in the previous month is
  // kStartTime0 and kStartTime2 is in the next month.  IMPORTANT: because these
  // three months are the "now" current/prev/next month when the calendar was
  // opened, they will NOT be pruned.
  // Current date is just kStartTime1.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime1, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewControllerEventsTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  controller_ = std::make_unique<MockCalendarViewController>();

  // Get our event list ready.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");

  // A series of events, one in each successive month.
  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      calendar_test_utils::CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event2 =
      calendar_test_utils::CreateEvent(kId2, kSummary2, kStartTime2, kEndTime2);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event3 =
      calendar_test_utils::CreateEvent(kId3, kSummary3, kStartTime3, kEndTime3);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event4 =
      calendar_test_utils::CreateEvent(kId4, kSummary4, kStartTime4, kEndTime4);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event5 =
      calendar_test_utils::CreateEvent(kId5, kSummary5, kStartTime5, kEndTime5);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event6 =
      calendar_test_utils::CreateEvent(kId6, kSummary6, kStartTime6, kEndTime6);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event7 =
      calendar_test_utils::CreateEvent(kId7, kSummary7, kStartTime7, kEndTime7);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event8 =
      calendar_test_utils::CreateEvent(kId8, kSummary8, kStartTime8, kEndTime8);
  SingleDayEventList events;

  // Inject all events, i.e. pretend the user added all these at some point.
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event1));
  event_list->InjectItemForTesting(std::move(event2));
  event_list->InjectItemForTesting(std::move(event3));
  event_list->InjectItemForTesting(std::move(event4));
  event_list->InjectItemForTesting(std::move(event5));
  event_list->InjectItemForTesting(std::move(event6));
  event_list->InjectItemForTesting(std::move(event7));
  event_list->InjectItemForTesting(std::move(event8));
  controller_->InjectEvents(std::move(event_list));

  // Fetch events, as if the user just opened the CrOS calendar with kStartTime1
  // as the currently on-screen month.  This means events from kStartTime0
  // (prev), kStartTime1 (current), and kStartTime2 (next) will be fetched.
  controller_->FetchEvents();

  // Events 0, 1, and 2 should be cached, but not 3.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime0, &events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime1, &events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime2, &events));
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime3, &events));

  // Advance us to kStartTime2 and fetch again.
  result = base::Time::FromString(kStartTime2, &current_date);
  DCHECK(result);
  controller_->UpdateMonth(current_date);
  controller_->FetchEvents();

  // Now kStartTime3 should be cached.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime3, &events));

  // Keep advancing us one month at a time, right up to the point where we need
  // to prune.
  result = base::Time::FromString(kStartTime3, &current_date);
  DCHECK(result);
  controller_->UpdateMonth(current_date);
  controller_->FetchEvents();
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime4, &events));

  result = base::Time::FromString(kStartTime4, &current_date);
  DCHECK(result);
  controller_->UpdateMonth(current_date);
  controller_->FetchEvents();
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime5, &events));

  // Now we're about to add a 7th month to the cache, so we're going to need to
  // prune the least-recently-used prunable month, which is kStartTime3.  So,
  // kStartTime3 should show up as a day with events before we advance, but not
  // after, which means we pruned as expected.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime3, &events));
  result = base::Time::FromString(kStartTime5, &current_date);
  DCHECK(result);
  controller_->UpdateMonth(current_date);
  controller_->FetchEvents();
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime6, &events));
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime3, &events));

  // Verify that our non-prunable months are still present.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime0, &events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime1, &events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime2, &events));

  // If we advance again, kStartTime4 should be pruned.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime4, &events));
  result = base::Time::FromString(kStartTime6, &current_date);
  DCHECK(result);
  controller_->UpdateMonth(current_date);
  controller_->FetchEvents();
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime7, &events));
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime4, &events));

  // Verify that our non-prunable months are still present.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime0, &events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime1, &events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime2, &events));

  // If we advance again, kStartTime5 should be pruned.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime5, &events));
  result = base::Time::FromString(kStartTime7, &current_date);
  DCHECK(result);
  controller_->UpdateMonth(current_date);
  controller_->FetchEvents();
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime8, &events));
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime5, &events));

  // Verify that our non-prunable months are still present.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime0, &events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime1, &events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime2, &events));
}

TEST_F(CalendarViewControllerEventsTest, RecordFetchResultHistogram_Success) {
  const char* kStartTime = "23 Oct 2009 11:30 GMT";
  const char* kEndTime = "23 Oct 2009 12:30 GMT";
  const char* kId = "id_0";
  const char* kSummary = "summary_0";
  base::HistogramTester histogram_tester;

  // Current date is just kStartTime.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewControllerEventsTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  controller_ = std::make_unique<MockCalendarViewController>();

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId, kSummary, kStartTime, kEndTime);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on kStartTime0.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  controller_->InjectEvents(std::move(event_list));

  // Now fetch the events, which will get all events from the current month, as
  // well as next/prev months.
  controller_->FetchEvents();

  // We should have recorded "success" for all three fetches (current, prev, and
  // next months).
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::HTTP_SUCCESS,
                                     /*expected_count=*/3);
}

TEST_F(CalendarViewControllerEventsTest, RecordFetchResultHistogram_Failure) {
  const char* kStartTime = "23 Oct 2009 11:30 GMT";
  const char* kEndTime = "23 Oct 2009 12:30 GMT";
  const char* kId = "id_0";
  const char* kSummary = "summary_0";
  base::HistogramTester histogram_tester;

  // Current date is just kStartTime.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewControllerEventsTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  controller_ = std::make_unique<MockCalendarViewController>();

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId, kSummary, kStartTime, kEndTime);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on kStartTime0.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  controller_->InjectEvents(std::move(event_list));

  // Set up return error codes.
  controller_->SetFetchErrors(current_date, google_apis::HTTP_UNAUTHORIZED,
                              google_apis::NO_CONNECTION,
                              google_apis::PARSE_ERROR);

  // Now fetch the events, which will get all events from the current month, as
  // well as next/prev months.
  controller_->FetchEvents();

  // We should have recorded "success" for no fetches, and one each for the
  // errors we specified.
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::HTTP_SUCCESS,
                                     /*expected_count=*/0);
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::HTTP_UNAUTHORIZED,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::NO_CONNECTION,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::PARSE_ERROR,
                                     /*expected_count=*/1);
}

}  // namespace ash
