// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_model.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"

namespace ash {

class CalendarModelUtilsTest : public AshTestBase {
 public:
  CalendarModelUtilsTest() = default;
  CalendarModelUtilsTest(const CalendarModelUtilsTest& other) = delete;
  CalendarModelUtilsTest& operator=(const CalendarModelUtilsTest& other) =
      delete;
  ~CalendarModelUtilsTest() override = default;

  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }
  static base::Time FakeTimeNow() { return fake_time_; }

  static base::Time fake_time_;
};

base::Time CalendarModelUtilsTest::fake_time_;

TEST_F(CalendarModelUtilsTest, SurroundingMonths) {
  std::set<base::Time> months;

  // Set current date.
  base::Time current_date, start_of_month;
  bool result = base::Time::FromString("23 Oct 2009 11:30 GMT", &current_date);
  DCHECK(result);
  result = base::Time::FromString("01 Oct 2009 00:00 GMT", &start_of_month);
  DCHECK(result);
  CalendarModelUtilsTest::SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelUtilsTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  // 0 months out.
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 0);
  EXPECT_EQ(1UL, months.size());
  EXPECT_TRUE(months.find(start_of_month) != months.end());

  // 1 month out.
  base::Time start_of_previous_month;
  result =
      base::Time::FromString("01 Sep 2009 00:00 GMT", &start_of_previous_month);
  DCHECK(result);
  base::Time start_of_next_month;
  result =
      base::Time::FromString("01 Nov 2009 00:00 GMT", &start_of_next_month);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  EXPECT_EQ(3UL, months.size());
  EXPECT_TRUE(months.find(start_of_month) != months.end());
  EXPECT_TRUE(months.find(start_of_previous_month) != months.end());
  EXPECT_TRUE(months.find(start_of_next_month) != months.end());

  // 2 months out.
  base::Time start_of_previous_month_2;
  result = base::Time::FromString("01 Aug 2009 00:00 GMT",
                                  &start_of_previous_month_2);
  DCHECK(result);
  base::Time start_of_next_month_2;
  result =
      base::Time::FromString("01 Dec 2009 00:00 GMT", &start_of_next_month_2);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 2);
  EXPECT_EQ(5UL, months.size());
  EXPECT_TRUE(months.find(start_of_month) != months.end());
  EXPECT_TRUE(months.find(start_of_previous_month) != months.end());
  EXPECT_TRUE(months.find(start_of_next_month) != months.end());
  EXPECT_TRUE(months.find(start_of_previous_month_2) != months.end());
  EXPECT_TRUE(months.find(start_of_next_month_2) != months.end());

  // 3 months out, which takes us into the next year.
  base::Time start_of_previous_month_3;
  result = base::Time::FromString("01 Jul 2009 00:00 GMT",
                                  &start_of_previous_month_3);
  DCHECK(result);
  base::Time start_of_next_month_3;
  result =
      base::Time::FromString("01 Jan 2010 00:00 GMT", &start_of_next_month_3);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 3);
  EXPECT_EQ(7UL, months.size());
  EXPECT_TRUE(months.find(start_of_month) != months.end());
  EXPECT_TRUE(months.find(start_of_previous_month) != months.end());
  EXPECT_TRUE(months.find(start_of_next_month) != months.end());
  EXPECT_TRUE(months.find(start_of_previous_month_2) != months.end());
  EXPECT_TRUE(months.find(start_of_next_month_2) != months.end());
  EXPECT_TRUE(months.find(start_of_previous_month_3) != months.end());
  EXPECT_TRUE(months.find(start_of_next_month_3) != months.end());
}

// Testable version of CalendarModel, into which we can directly inject
// events that CalendarModel queries via the Google calendar API.
class TestableCalendarModel : public CalendarModel {
 public:
  TestableCalendarModel() = default;
  TestableCalendarModel(const TestableCalendarModel& other) = delete;
  TestableCalendarModel& operator=(const TestableCalendarModel& other) = delete;
  ~TestableCalendarModel() override = default;

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

 protected:
  void MaybeFetchMonth(base::Time start_of_month) override {
    // Early return if the month has already been fetched or no events (not even
    // an empty list) have been injected.
    if (IsMonthAlreadyFetched(start_of_month) || !injected_events_.get())
      return;

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
    OnEventsFetched(start_of_month, GetFetchErrorCode(start_of_month),
                    fetched_events.get());
  }

 private:
  std::unique_ptr<google_apis::calendar::EventList> injected_events_;
  std::map<base::Time, google_apis::ApiErrorCode> fetch_errors_;
};

class CalendarModelTest : public AshTestBase {
 public:
  CalendarModelTest() = default;
  CalendarModelTest(const CalendarModelTest& other) = delete;
  CalendarModelTest& operator=(const CalendarModelTest& other) = delete;
  ~CalendarModelTest() override = default;

  void TearDown() override {
    calendar_model_.reset();

    AshTestBase::TearDown();
  }

  int EventsNumberOfDay(const char* day, SingleDayEventList* events) {
    base::Time day_base;

    bool result = base::Time::FromString(day, &day_base);
    DCHECK(result);

    if (events)
      DCHECK(events->empty());

    return calendar_model_->EventsNumberOfDay(day_base, events);
  }

  int EventsNumberOfDayInternal(const char* day,
                                SingleDayEventList* events) const {
    base::Time day_base;

    bool result = base::Time::FromString(day, &day_base);
    DCHECK(result);

    if (events)
      DCHECK(events->empty());

    return calendar_model_->EventsNumberOfDayInternal(day_base, events);
  }

  bool IsEventPresent(const char* event_id, SingleDayEventList& events) {
    const auto it =
        std::find_if(events.begin(), events.end(),
                     [event_id](google_apis::calendar::CalendarEvent event) {
                       return event.id() == event_id;
                     });
    return it != events.end();
  }

  void UpdateSession(uint32_t session_id, const std::string& email) {
    UserSession session;
    session.session_id = session_id;
    session.user_info.type = user_manager::USER_TYPE_REGULAR;
    session.user_info.account_id = AccountId::FromUserEmail(email);
    session.user_info.display_name = email;
    session.user_info.display_email = email;
    session.user_info.is_new_profile = false;

    SessionController::Get()->UpdateUserSession(session);
  }

  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }
  static base::Time FakeTimeNow() { return fake_time_; }
  static base::Time fake_time_;

  std::unique_ptr<TestableCalendarModel> calendar_model_;
};

base::Time CalendarModelTest::fake_time_;

TEST_F(CalendarModelTest, Instantiate) {
  // Set current date.
  base::Time current_date;
  bool result = base::Time::FromString("23 Oct 2009 11:30 GMT", &current_date);
  DCHECK(result);
  CalendarModelTest::SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  calendar_model_->FetchEvents(months);

  // An event fetcher was instantiated, and there are no events for today (or
  // any day).
  EXPECT_NE(calendar_model_.get(), nullptr);
  EXPECT_EQ(calendar_model_->EventsNumberOfDay(base::Time::Now(), nullptr), 0);
}

TEST_F(CalendarModelTest, DayWithEvents_OneDay) {
  const char* kStartTime = "23 Oct 2009 11:30 GMT";
  const char* kEndTime = "23 Oct 2009 12:30 GMT";
  const char* kId = "id_0";
  const char* kSummary = "summary_0";

  // Current date is just `kStartTime`.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  calendar_model_->FetchEvents(months);

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId, kSummary, kStartTime, kEndTime);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0`.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  calendar_model_->InjectEvents(std::move(event_list));

  // Now fetch the events, which will get all events from the current month, as
  // well as next/prev months.
  calendar_model_->FetchEvents(months);

  // Now we have an event on kStartTime0.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
}

TEST_F(CalendarModelTest, DayWithEvents_TwoDays) {
  const char* kStartTime0 = "23 Oct 2009 11:30 GMT";
  const char* kEndTime0 = "23 Oct 2009 12:30 GMT";
  const char* kId0 = "id_0";
  const char* kSummary0 = "summary_0";
  const char* kStartTime1 = "24 Oct 2009 07:30 GMT";
  const char* kEndTime1 = "25 Oct 2009 08:30 GMT";
  const char* kId1 = "id_1";
  const char* kSummary1 = "summary_1";

  // Current date is just `kStartTime0`.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime0, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  calendar_model_->FetchEvents(months);

  // Get ready to inject two events.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      calendar_test_utils::CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0` or
  // `kStartTime1`.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime1, &events));
  EXPECT_TRUE(events.empty());

  // Inject both events.
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event1));
  calendar_model_->InjectEvents(std::move(event_list));
  calendar_model_->FetchEvents(months);

  // Now both days should have events.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime1, &events));
  EXPECT_FALSE(events.empty());
}

TEST_F(CalendarModelTest, ChangeTimeDifference) {
  const char* kStartTime0 = "23 Oct 2009 11:30 GMT";
  const char* kEndTime0 = "23 Oct 2009 12:30 GMT";
  const char* kId0 = "id_0";
  const char* kSummary0 = "summary_0";
  const char* kStartTime1 = "24 Oct 2009 07:30 GMT";
  const char* kEndTime1 = "25 Oct 2009 08:30 GMT";
  const char* kId1 = "id_1";
  const char* kSummary1 = "summary_1";

  // Current date is just `kStartTime0`.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime0, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  calendar_model_->FetchEvents(months);

  // Get ready to inject two events.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      calendar_test_utils::CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);
  SingleDayEventList events;

  // Inject both events.
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event1));
  calendar_model_->InjectEvents(std::move(event_list));
  calendar_model_->FetchEvents(months);

  // Based on the tesing timezone "America/Los_Angeles" these 2 events are
  // distributed into 2 days. Each day has one event.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));

  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime1, &events));

  // Adjusts the time with -10 hours.
  // `kStartTime0` "23 Oct 2009 11:30" -> "23 Oct 2009 1:30".
  // `kStartTime1` "24 Oct 2009 07:30" -> "23 Oct 2009 21:30"
  // Both events should be on the 23rd.
  calendar_model_->RedistributeEvents(/*time_difference_minutes=*/-10 * 60);
  events.clear();
  EXPECT_EQ(2, EventsNumberOfDay(kStartTime0, &events));

  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime1, &events));

  // Adjusts the time with +15 hours.
  // `kStartTime0` "23 Oct 2009 11:30" -> "24 Oct 2009 2:30".
  // `kStartTime1` "24 Oct 2009 07:30" -> "24 Oct 2009 22:30"
  // Both events should be on the 24rd.
  calendar_model_->RedistributeEvents(/*time_difference_minutes=*/15 * 60);
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));

  events.clear();
  EXPECT_EQ(2, EventsNumberOfDay(kStartTime1, &events));
}

TEST_F(CalendarModelTest, OnlyFetchOnce) {
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

  // Current date is just `kStartTime0`.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime0, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  calendar_model_->FetchEvents(months);

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  SingleDayEventList events;

  // No events at `kStartTime0`.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  // Inject one event, pretend the user just added it somewhere else.
  event_list->InjectItemForTesting(std::move(event));
  calendar_model_->InjectEvents(std::move(event_list));

  // Fetch events, pretend the user just brought up the CrOS calendar.
  calendar_model_->FetchEvents(months);

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
  calendar_model_->InjectEvents(std::move(event_list));
  calendar_model_->FetchEvents(months);

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

TEST_F(CalendarModelTest, EventsDifferentMonths) {
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

  // Current date is just `kStartTime1`.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime1, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  calendar_model_->FetchEvents(months);

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
  calendar_model_->InjectEvents(std::move(event_list));

  // Fetch events (user just opened CrOS calendar).
  calendar_model_->FetchEvents(months);

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
TEST_F(CalendarModelTest, PruneEvents) {
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
  const char* kStartTime9 = "23 Jul 2010 11:30 GMT";
  const char* kEndTime9 = "23 Jul 2010 12:30 GMT";
  const char* kId9 = "id_9";
  const char* kSummary9 = "summary_9";
  const char* kStartTime10 = "23 Aug 2010 11:30 GMT";
  const char* kEndTime10 = "23 Aug 2010 12:30 GMT";
  const char* kId10 = "id_10";
  const char* kSummary10 = "summary_10";
  const char* kStartTime11 = "23 Sep 2010 11:30 GMT";
  const char* kEndTime11 = "23 Sep 2010 12:30 GMT";
  const char* kId11 = "id_11";
  const char* kSummary11 = "summary_11";
  const char* kStartTime12 = "23 Oct 2010 11:30 GMT";
  const char* kEndTime12 = "23 Oct 2010 12:30 GMT";
  const char* kId12 = "id_12";
  const char* kSummary12 = "summary_12";

  // Current time is `kStartTime1`, which means event in the previous month is
  // kStartTime0 and `kStartTime2` is in the next month.  IMPORTANT: because
  // these three months are the "now" current/prev/next month when the calendar
  // was opened, they will NOT be pruned. Current date is just `kStartTime1`.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime1, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  calendar_model_->FetchEvents(months);

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
  std::unique_ptr<google_apis::calendar::CalendarEvent> event9 =
      calendar_test_utils::CreateEvent(kId9, kSummary9, kStartTime9, kEndTime9);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event10 =
      calendar_test_utils::CreateEvent(kId10, kSummary10, kStartTime10,
                                       kEndTime10);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event11 =
      calendar_test_utils::CreateEvent(kId11, kSummary11, kStartTime11,
                                       kEndTime11);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event12 =
      calendar_test_utils::CreateEvent(kId12, kSummary12, kStartTime12,
                                       kEndTime12);
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
  event_list->InjectItemForTesting(std::move(event9));
  event_list->InjectItemForTesting(std::move(event10));
  event_list->InjectItemForTesting(std::move(event11));
  event_list->InjectItemForTesting(std::move(event12));
  calendar_model_->InjectEvents(std::move(event_list));

  // Fetch events, as if the user just opened the CrOS calendar with
  // `kStartTime1` as the currently on-screen month.  This means events from
  // `kStartTime0` (prev), `kStartTime1` (current), and `kStartTime2` (next)
  // will be fetched.
  calendar_model_->FetchEvents(months);

  // Events 0, 1, and 2 should be cached, but not 3.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime0, &events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime1, &events));
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime2, &events));
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime3, &events));

  // Advance us to `kStartTime2` and fetch again.
  result = base::Time::FromString(kStartTime2, &current_date);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  calendar_model_->FetchEvents(months);

  // Now `kStartTime3` should be cached.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime3, &events));

  // Keep advancing us one month at a time, right up to the point where we need
  // to prune.
  result = base::Time::FromString(kStartTime3, &current_date);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  calendar_model_->FetchEvents(months);
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime4, &events));

  result = base::Time::FromString(kStartTime4, &current_date);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  calendar_model_->FetchEvents(months);
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime5, &events));

  result = base::Time::FromString(kStartTime5, &current_date);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  calendar_model_->FetchEvents(months);
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime6, &events));

  result = base::Time::FromString(kStartTime6, &current_date);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  calendar_model_->FetchEvents(months);
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime7, &events));

  result = base::Time::FromString(kStartTime7, &current_date);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  calendar_model_->FetchEvents(months);
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime8, &events));

  // Now we're about to add a 10th month to the cache, so we're going to need to
  // prune the least-recently-used prunable month, which is `kStartTime0`.  So,
  // `kStartTime0` should show up as a day with events before we advance, but
  // not after, which means we pruned as expected.

  // If we advance again, `kStartTime0` should be pruned.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime0, &events));
  result = base::Time::FromString(kStartTime8, &current_date);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  calendar_model_->FetchEvents(months);
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime9, &events));
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime0, &events));

  // If we advance again, `kStartTime1` should be pruned.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime1, &events));
  result = base::Time::FromString(kStartTime9, &current_date);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  calendar_model_->FetchEvents(months);
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime10, &events));
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime1, &events));

  // If we advance again, `kStartTime2` should be pruned.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime2, &events));
  result = base::Time::FromString(kStartTime10, &current_date);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  calendar_model_->FetchEvents(months);
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime11, &events));
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime2, &events));

  // If we advance again, `kStartTime3` should be pruned.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime3, &events));
  result = base::Time::FromString(kStartTime11, &current_date);
  DCHECK(result);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  calendar_model_->FetchEvents(months);
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime12, &events));
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime3, &events));
}

TEST_F(CalendarModelTest, RecordFetchResultHistogram_Success) {
  const char* kStartTime = "23 Oct 2009 11:30 GMT";
  const char* kEndTime = "23 Oct 2009 12:30 GMT";
  const char* kId = "id_0";
  const char* kSummary = "summary_0";
  base::HistogramTester histogram_tester;

  // Current date is just `kStartTime`.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  calendar_model_->FetchEvents(months);

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId, kSummary, kStartTime, kEndTime);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0`.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  calendar_model_->InjectEvents(std::move(event_list));

  // Now fetch the events, which will get all events from the current month, as
  // well as next/prev months.
  calendar_model_->FetchEvents(months);

  // We should have recorded "success" for all three fetches (current, prev, and
  // next months).
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::HTTP_SUCCESS,
                                     /*expected_count=*/3);
}

TEST_F(CalendarModelTest, RecordFetchResultHistogram_Failure) {
  const char* kStartTime = "23 Oct 2009 11:30 GMT";
  const char* kEndTime = "23 Oct 2009 12:30 GMT";
  const char* kId = "id_0";
  const char* kSummary = "summary_0";
  base::HistogramTester histogram_tester;

  // Current date is just `kStartTime`.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  calendar_model_->FetchEvents(months);

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId, kSummary, kStartTime, kEndTime);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0`.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  calendar_model_->InjectEvents(std::move(event_list));

  // Set up return error codes.
  calendar_model_->SetFetchErrors(current_date, google_apis::HTTP_UNAUTHORIZED,
                                  google_apis::NO_CONNECTION,
                                  google_apis::PARSE_ERROR);

  // Now fetch the events, which will get all events from the current month, as
  // well as next/prev months.
  calendar_model_->FetchEvents(months);

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

TEST_F(CalendarModelTest, SessionStateChange) {
  const char* kStartTime = "23 Oct 2009 11:30 GMT";
  const char* kEndTime = "23 Oct 2009 12:30 GMT";
  const char* kId = "id_0";
  const char* kSummary = "summary_0";

  // Current date is just `kStartTime`.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  calendar_model_->FetchEvents(months);

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId, kSummary, kStartTime, kEndTime);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0`.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  calendar_model_->InjectEvents(std::move(event_list));

  // Now fetch the events, which will get all events from the current month, as
  // well as next/prev months.
  calendar_model_->FetchEvents(months);

  // Now we have an event on kStartTime0.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);

  // Lets pretend the user locked the screen, which should clear all cached
  // events.
  SessionInfo session_info;
  session_info.state = session_manager::SessionState::LOCKED;
  SessionController::Get()->SetSessionInfo(session_info);
  base::RunLoop().RunUntilIdle();
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime, &events));
  EXPECT_TRUE(events.empty());
}

TEST_F(CalendarModelTest, ActiveUserChange) {
  const char* kStartTime = "23 Oct 2009 11:30 GMT";
  const char* kEndTime = "23 Oct 2009 12:30 GMT";
  const char* kId = "id_0";
  const char* kSummary = "summary_0";

  // Set up two users, user1 is the active user.
  UpdateSession(1u, "user1@test.com");
  UpdateSession(2u, "user2@test.com");
  std::vector<uint32_t> order = {1u, 2u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();

  // Current date is just `kStartTime`.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime, &current_date);
  DCHECK(result);
  SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  calendar_model_->FetchEvents(months);

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId, kSummary, kStartTime, kEndTime);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0`.
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  calendar_model_->InjectEvents(std::move(event_list));

  // Now fetch the events, which will get all events from the current month, as
  // well as next/prev months.
  calendar_model_->FetchEvents(months);

  // Now we have an event on kStartTime0.
  events.clear();
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);

  // Make user2 the active user, and we should clear the cached events.
  order = {2u, 1u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();
  events.clear();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime, &events));
  EXPECT_TRUE(events.empty());
}

}  // namespace ash
