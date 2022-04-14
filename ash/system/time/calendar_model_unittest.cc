// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_model.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/shell.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash {

namespace {

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
const char* kStartTime13 = "24 Oct 2009 07:30 GMT";
const char* kEndTime13 = "25 Oct 2009 08:30 GMT";
const char* kId13 = "id_13";
const char* kSummary13 = "summary_13";

// For when we need more events than is reasonable to hard-code from the above.
// Returns a list of `num_events` events, one per month, ordered chromologically
// from oldest to newest.
[[maybe_unused]] std::unique_ptr<google_apis::calendar::EventList>
GetOrderedEventList(int num_events) {
  const char* kStartTime = "01 Oct 2009 00:00 GMT";
  const char* kIdBase = "id_";
  const char* kSummaryBase = "summary_";

  base::Time start_time_base;
  bool result = base::Time::FromString(kStartTime, &start_time_base);
  DCHECK(result);

  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");

  for (int i = 0; i < num_events; ++i) {
    base::Time start_time = start_time_base;
    for (int j = 0; j < i; ++j) {
      start_time = calendar_utils::GetStartOfNextMonthUTC(start_time);
    }
    base::Time end_time = start_time + base::Hours(1);
    std::string id = kIdBase;
    id.append(base::NumberToString(i));
    std::string summary = kSummaryBase;
    summary.append(base::NumberToString(i));
    std::unique_ptr<google_apis::calendar::CalendarEvent> event =
        ash::calendar_test_utils::CreateEvent(id.c_str(), summary.c_str(),
                                              start_time, end_time);
    event_list->InjectItemForTesting(std::move(event));
  }

  return event_list;
}

}  // namespace

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
  current_date =
      calendar_test_utils::GetTimeFromString("23 Oct 2009 11:30 GMT");
  start_of_month =
      calendar_test_utils::GetTimeFromString("01 Oct 2009 00:00 GMT");
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
  base::Time start_of_previous_month =
      calendar_test_utils::GetTimeFromString("01 Sep 2009 00:00 GMT");
  base::Time start_of_next_month =
      calendar_test_utils::GetTimeFromString("01 Nov 2009 00:00 GMT");
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  EXPECT_EQ(3UL, months.size());
  EXPECT_TRUE(months.find(start_of_month) != months.end());
  EXPECT_TRUE(months.find(start_of_previous_month) != months.end());
  EXPECT_TRUE(months.find(start_of_next_month) != months.end());

  // 2 months out.
  base::Time start_of_previous_month_2 =
      calendar_test_utils::GetTimeFromString("01 Aug 2009 00:00 GMT");
  base::Time start_of_next_month_2 =
      calendar_test_utils::GetTimeFromString("01 Dec 2009 00:00 GMT");
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 2);
  EXPECT_EQ(5UL, months.size());
  EXPECT_TRUE(months.find(start_of_month) != months.end());
  EXPECT_TRUE(months.find(start_of_previous_month) != months.end());
  EXPECT_TRUE(months.find(start_of_next_month) != months.end());
  EXPECT_TRUE(months.find(start_of_previous_month_2) != months.end());
  EXPECT_TRUE(months.find(start_of_next_month_2) != months.end());

  // 3 months out, which takes us into the next year.
  base::Time start_of_previous_month_3 =
      calendar_test_utils::GetTimeFromString("01 Jul 2009 00:00 GMT");
  base::Time start_of_next_month_3 =
      calendar_test_utils::GetTimeFromString("01 Jan 2010 00:00 GMT");
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
    // Early return if events (not even an empty list) have been injected.
    if (!injected_events_.get())
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
    event_months_.emplace(start_of_month, SingleMonthEventMap());

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
    time_overrides_.reset();
    calendar_model_.reset();

    AshTestBase::TearDown();
  }

  int EventsNumberOfDay(const char* day, SingleDayEventList* events) {
    base::Time day_base = calendar_test_utils::GetTimeFromString(day);

    if (events)
      events->clear();

    return calendar_model_->EventsNumberOfDay(day_base, events);
  }

  int EventsNumberOfDayInternal(const char* day,
                                SingleDayEventList* events) const {
    base::Time day_base = calendar_test_utils::GetTimeFromString(day);

    if (events)
      events->clear();

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

  bool EventsPresentAtIndex(const google_apis::calendar::EventList* event_list,
                            int index) {
    DCHECK(event_list);
    DCHECK_GE(index, 0);
    DCHECK_LT(index, static_cast<int>(event_list->items().size()));
    const base::Time& date =
        event_list->items()[index]->start_time().date_time();
    return calendar_model_->EventsNumberOfDayInternal(date, nullptr) > 0;
  }

  bool EventsPresentInRange(const google_apis::calendar::EventList* event_list,
                            int start_index,
                            int end_index) {
    DCHECK(event_list);
    DCHECK_GE(start_index, 0);
    DCHECK_GT(end_index, start_index);

    for (int i = start_index; i < end_index; ++i) {
      if (!EventsPresentAtIndex(event_list, i))
        return false;
    }

    return true;
  }

  bool NoEventsPresentInRange(
      const google_apis::calendar::EventList* event_list,
      int start_index,
      int end_index,
      std::set<base::Time>* non_prunable_months = nullptr) {
    DCHECK(event_list);
    DCHECK_GE(start_index, 0);
    DCHECK_GT(end_index, start_index);

    for (int i = start_index; i < end_index; ++i) {
      if (non_prunable_months) {
        const base::Time& date =
            event_list->items()[i]->start_time().date_time();
        const base::Time& start_of_month =
            calendar_utils::GetFirstDayOfMonth(date).UTCMidnight();
        if (base::Contains(*non_prunable_months, start_of_month))
          continue;
      }

      if (EventsPresentAtIndex(event_list, i))
        return false;
    }

    return true;
  }

  // Convenient representation of the ranges used in our sliding-window tests.
  // Being within a range means >= start && < end.
  struct SlidingWindowRanges {
    SlidingWindowRanges(int index, int total_size) {
      // "Preceding" range always starts at 0, and only ends meaningfully (and
      // we only bother testing it) if anything's been pruned, i.e. the only
      // reason the end index would be > 0.
      preceding_start = 0;
      preceding_end =
          std::max(0, index + calendar_utils::kNumSurroundingMonthsCached -
                          calendar_utils::kMaxNumPrunableMonths);

      // "Active" range is where we have cached months. Starts at 0 or the
      // bottom of a full cache, whichever is larger.
      active_start =
          std::max(0, index + calendar_utils::kNumSurroundingMonthsCached -
                          calendar_utils::kMaxNumPrunableMonths);
      active_end = std::min(index + calendar_utils::kNumSurroundingMonthsCached,
                            total_size);

      // "Following" range. The smaller of the very end of the cache and our
      // current position + surrounding.
      following_start = std::min(
          index + calendar_utils::kNumSurroundingMonthsCached + 1, total_size);
      following_end = total_size;
    }

    int preceding_start;
    int preceding_end;
    int active_start;
    int active_end;
    int following_start;
    int following_end;
  };

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

  void SetFakeNowFromTime(const base::Time& date) {
    CalendarModelTest::SetFakeNow(date);
    time_overrides_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        &CalendarModelTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
        /*thread_ticks_override=*/nullptr);
  }

  void SetFakeNowFromStr(const char* date_str) {
    base::Time current_date;
    bool result = base::Time::FromString(date_str, &current_date);
    DCHECK(result);
    SetFakeNowFromTime(current_date);
  }

  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }
  static base::Time FakeTimeNow() { return fake_time_; }
  static base::Time fake_time_;
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_overrides_;

  std::unique_ptr<TestableCalendarModel> calendar_model_;
};

base::Time CalendarModelTest::fake_time_;

TEST_F(CalendarModelTest, DayWithEvents_OneDay) {
  const char* kStartTime = "23 Oct 2009 11:30 GMT";
  const char* kEndTime = "23 Oct 2009 12:30 GMT";
  const char* kId = "id_0";
  const char* kSummary = "summary_0";

  // Current date is just `kStartTime`.
  SetFakeNowFromStr(kStartTime);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  std::set<base::Time> months =
      calendar_utils::GetSurroundingMonthsUTC(base::Time::Now(), 1);

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId, kSummary, kStartTime, kEndTime);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0`.
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  calendar_model_->InjectEvents(std::move(event_list));

  // Now fetch the events, which will get all events from the current month, as
  // well as next/prev months.
  calendar_model_->FetchEvents(months);

  // Now we have an event on kStartTime0.
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
}

TEST_F(CalendarModelTest, DayWithEvents_TwoDays) {
  // Current date is just `kStartTime0`.
  SetFakeNowFromStr(kStartTime0);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  std::set<base::Time> months =
      calendar_utils::GetSurroundingMonthsUTC(base::Time::Now(), 1);

  // Get ready to inject two events.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event13 =
      calendar_test_utils::CreateEvent(kId13, kSummary13, kStartTime13,
                                       kEndTime13);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0` or
  // `kStartTime1`.
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime13, &events));
  EXPECT_TRUE(events.empty());

  // Inject both events.
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event13));
  calendar_model_->InjectEvents(std::move(event_list));
  calendar_model_->FetchEvents(months);

  // Now both days should have events.
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime13, &events));
  EXPECT_FALSE(events.empty());
}

TEST_F(CalendarModelTest, ChangeTimeDifference) {
  // Current date is just `kStartTime0`.
  SetFakeNowFromStr(kStartTime0);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  std::set<base::Time> months =
      calendar_utils::GetSurroundingMonthsUTC(base::Time::Now(), 1);

  // Get ready to inject two events.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event13 =
      calendar_test_utils::CreateEvent(kId13, kSummary13, kStartTime13,
                                       kEndTime13);
  SingleDayEventList events;

  // Inject both events.
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event13));
  calendar_model_->InjectEvents(std::move(event_list));
  calendar_model_->FetchEvents(months);

  // Based on the tesing timezone "America/Los_Angeles" these 2 events are
  // distributed into 2 days. Each day has one event.
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime13, &events));

  // Adjusts the time with -10 hours.
  // `kStartTime0` "23 Oct 2009 11:30" -> "23 Oct 2009 1:30".
  // `kStartTime1` "24 Oct 2009 07:30" -> "23 Oct 2009 21:30"
  // Both events should be on the 23rd.
  calendar_model_->RedistributeEvents(/*time_difference_minutes=*/-10 * 60);
  EXPECT_EQ(2, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime13, &events));

  // Adjusts the time with +15 hours.
  // `kStartTime0` "23 Oct 2009 11:30" -> "24 Oct 2009 2:30".
  // `kStartTime1` "24 Oct 2009 07:30" -> "24 Oct 2009 22:30"
  // Both events should be on the 24rd.
  calendar_model_->RedistributeEvents(/*time_difference_minutes=*/15 * 60);
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_EQ(2, EventsNumberOfDay(kStartTime13, &events));
}

TEST_F(CalendarModelTest, EventsDifferentMonths) {
  // Current date is just `kStartTime1`.
  SetFakeNowFromStr(kStartTime1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  std::set<base::Time> months =
      calendar_utils::GetSurroundingMonthsUTC(base::Time::Now(), 1);

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
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime1, &events));
  EXPECT_TRUE(events.empty());
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
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
  EXPECT_TRUE(IsEventPresent(kId0, events));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime1, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
  EXPECT_TRUE(IsEventPresent(kId1, events));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime2, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);
  EXPECT_TRUE(IsEventPresent(kId2, events));
}

// Test for pruning of events, where a sliding window passes through a list of
// chronologically-ordered months. As the window is moved one month at a time,
// from beginning to end, we verify the presence of cached events in an "active"
// range that sits between a "prefix" range and a "suffix" range. We also verify
// the absence of cached events in the "prefix" and "suffix" ranges.
TEST_F(CalendarModelTest, PruneEvents_SlidingWindow) {
  constexpr int kNumAdditionalMonths = 5;
  constexpr int kNumEvents =
      calendar_utils::kMaxNumPrunableMonths + kNumAdditionalMonths;
  DCHECK_GT(kNumEvents, 0);
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      GetOrderedEventList(kNumEvents);

  // Current time is the start time of the first event in the list.
  const google_apis::calendar::CalendarEvent* first_event =
      event_list->items()[0].get();
  const base::Time& start_time = first_event->start_time().date_time();
  SetFakeNowFromTime(start_time);

  // Basic setup.
  SingleDayEventList events;
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(
      base::Time::Now(), calendar_utils::kNumSurroundingMonthsCached);

  // Inject events.
  calendar_model_->InjectEvents(std::move(event_list));

  // Injecting the list transferred ownership of the first list we got, so get
  // another one.
  event_list = GetOrderedEventList(kNumEvents);

  // Loop where we advance our window, fetch, and then verify the
  // presence/absence of events where we expect.
  for (int i = 0; i < kNumEvents; ++i) {
    // Advance our set of visible months.
    const google_apis::calendar::CalendarEvent* on_screen_event =
        event_list->items()[i].get();
    const base::Time& on_screen_month =
        on_screen_event->start_time().date_time();
    months = calendar_utils::GetSurroundingMonthsUTC(
        on_screen_month, calendar_utils::kNumSurroundingMonthsCached);

    // Fetch events.
    calendar_model_->FetchEvents(months);

    // Construct the testable ranges.
    SlidingWindowRanges ranges(i, kNumEvents);

    // Verify that the ranges contain or don't contain what we expect.
    EXPECT_EQ(ranges.preceding_start, 0);
    if (ranges.preceding_end != 0) {
      EXPECT_TRUE(NoEventsPresentInRange(
          event_list.get(), ranges.preceding_start, ranges.preceding_end));
    }
    EXPECT_TRUE(EventsPresentInRange(event_list.get(), ranges.active_start,
                                     ranges.active_end));
    if (ranges.following_start != kNumEvents &&
        ranges.following_end != kNumEvents) {
      EXPECT_TRUE(NoEventsPresentInRange(
          event_list.get(), ranges.following_start, ranges.following_end));
    }
  }
}

// Test for pruning of events, where a sliding window passes through a list of
// chronologically-ordered months, some of which are non-prunable. As the window
// is moved one month at a time, from beginning to end, we verify the presence
// of cached events in an "active" range that sits between a "prefix" range and
// a "suffix" range. We also verify the absence of cached events in the "prefix"
// and "suffix" ranges.
TEST_F(CalendarModelTest, PruneEvents_SlidingWindowWithNonPrunableMonths) {
  constexpr int kNumNonPrunableMonths =
      2 * calendar_utils::kNumSurroundingMonthsCached + 1;
  constexpr int kNumAdditionalMonths = 10;
  constexpr int kNumEvents =
      calendar_utils::kMaxNumPrunableMonths + kNumAdditionalMonths;
  DCHECK_GE(kNumEvents, 5);
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      GetOrderedEventList(kNumEvents);

  // Current time is the start time of the first event in the list.
  const google_apis::calendar::CalendarEvent* first_event =
      event_list->items()[0].get();
  const base::Time& start_time = first_event->start_time().date_time();
  SetFakeNowFromTime(start_time);

  // Basic setup.
  SingleDayEventList events;
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(
      base::Time::Now(), calendar_utils::kNumSurroundingMonthsCached);

  // Designate the first `kNumNonPrunableMonths` months as non-prunable. Getting
  // the months surrounding index 2 gets us the first 5.
  std::set<base::Time> non_prunable_months =
      calendar_utils::GetSurroundingMonthsUTC(
          event_list->items()[2]->start_time().date_time(),
          calendar_utils::kNumSurroundingMonthsCached);
  DCHECK_EQ(static_cast<int>(non_prunable_months.size()),
            kNumNonPrunableMonths);
  calendar_model_->AddNonPrunableMonths(non_prunable_months);

  // Inject events.
  calendar_model_->InjectEvents(std::move(event_list));

  // Fetch the mon-prunable events.
  calendar_model_->FetchEvents(non_prunable_months);

  // Injecting the list transferred ownership of the first list we got, so get
  // another one.
  event_list = GetOrderedEventList(kNumEvents);

  // Loop where we advance our window, fetch, and then verify the
  // presence/absence of events where we expect.
  for (int i = 0; i < kNumEvents; ++i) {
    // Advance our set of visible months.
    const google_apis::calendar::CalendarEvent* on_screen_event =
        event_list->items()[i].get();
    const base::Time& on_screen_month =
        on_screen_event->start_time().date_time();
    months = calendar_utils::GetSurroundingMonthsUTC(
        on_screen_month, calendar_utils::kNumSurroundingMonthsCached);

    // Fetch events.
    calendar_model_->FetchEvents(months);

    // Construct the testable ranges.
    SlidingWindowRanges ranges(i, kNumEvents);

    // Verify that the ranges contain or don't contain what we expect.
    if (ranges.preceding_start != 0 && ranges.preceding_end != 0) {
      EXPECT_TRUE(NoEventsPresentInRange(
          event_list.get(), ranges.preceding_start, ranges.preceding_end));
    }
    EXPECT_TRUE(EventsPresentInRange(event_list.get(), ranges.active_start,
                                     ranges.active_end));
    if (ranges.following_start != kNumEvents &&
        ranges.following_end != kNumEvents) {
      EXPECT_TRUE(
          NoEventsPresentInRange(event_list.get(), ranges.following_start,
                                 ranges.following_end, &non_prunable_months));
    }

    // Verify that our non-prunable months didn't get pruned, i.e. are still
    // present.
    EXPECT_TRUE(
        EventsPresentInRange(event_list.get(), 0, kNumNonPrunableMonths));
  }
}

TEST_F(CalendarModelTest, RecordFetchResultHistogram_Success) {
  base::HistogramTester histogram_tester;

  // Current date is just `kStartTime0`.
  SetFakeNowFromStr(kStartTime0);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  std::set<base::Time> months =
      calendar_utils::GetSurroundingMonthsUTC(base::Time::Now(), 1);

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0`.
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  calendar_model_->InjectEvents(std::move(event_list));

  // Now fetch the events, which will get all events from the current month,
  // as well as next/prev months.
  calendar_model_->FetchEvents(months);

  // We should have recorded "success" for all three fetches (current, prev,
  // and next months).
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::HTTP_SUCCESS,
                                     /*expected_count=*/3);
}

TEST_F(CalendarModelTest, RecordFetchResultHistogram_Failure) {
  base::HistogramTester histogram_tester;

  // Current date is just `kStartTime0`.
  base::Time current_date;
  bool result = base::Time::FromString(kStartTime0, &current_date);
  DCHECK(result);
  SetFakeNowFromTime(current_date);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  std::set<base::Time> months =
      calendar_utils::GetSurroundingMonthsUTC(base::Time::Now(), 1);

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0`.
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  calendar_model_->InjectEvents(std::move(event_list));

  // Set up return error codes.
  calendar_model_->SetFetchErrors(current_date, google_apis::HTTP_UNAUTHORIZED,
                                  google_apis::NO_CONNECTION,
                                  google_apis::PARSE_ERROR);

  // Now fetch the events, which will get all events from the current month,
  // as well as next/prev months.
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
  // Current date is just `kStartTime0`.
  SetFakeNowFromStr(kStartTime0);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  std::set<base::Time> months =
      calendar_utils::GetSurroundingMonthsUTC(base::Time::Now(), 1);

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0`.
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  calendar_model_->InjectEvents(std::move(event_list));

  // Now fetch the events, which will get all events from the current month,
  // as well as next/prev months.
  calendar_model_->FetchEvents(months);

  // Now we have an event on kStartTime0.
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);

  // Lets pretend the user locked the screen, which should clear all cached
  // events.
  SessionInfo session_info;
  session_info.state = session_manager::SessionState::LOCKED;
  SessionController::Get()->SetSessionInfo(session_info);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
}

TEST_F(CalendarModelTest, ActiveUserChange) {
  // Set up two users, user1 is the active user.
  UpdateSession(1u, "user1@test.com");
  UpdateSession(2u, "user2@test.com");
  std::vector<uint32_t> order = {1u, 2u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();

  // Current date is just `kStartTime0`.
  SetFakeNowFromStr(kStartTime0);
  calendar_model_ = std::make_unique<TestableCalendarModel>();
  std::set<base::Time> months =
      calendar_utils::GetSurroundingMonthsUTC(base::Time::Now(), 1);

  // Set up list of events to inject.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  SingleDayEventList events;

  // Haven't injected anything yet, so no events on `kStartTime0`.
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  // Inject events (pretend the user just added them).
  event_list->InjectItemForTesting(std::move(event));
  calendar_model_->InjectEvents(std::move(event_list));

  // Now fetch the events, which will get all events from the current month,
  // as well as next/prev months.
  calendar_model_->FetchEvents(months);

  // Now we have an event on kStartTime0.
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);

  // Make user2 the active user, and we should clear the cached events.
  order = {2u, 1u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
}

TEST_F(CalendarModelTest, ClearEvents) {
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

  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("Greenwich Mean Time");
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event1));
  event_list->InjectItemForTesting(std::move(event2));
  event_list->InjectItemForTesting(std::move(event3));
  event_list->InjectItemForTesting(std::move(event4));
  event_list->InjectItemForTesting(std::move(event5));

  // Current time is `kStartTime1`, which means the `kStartTime0` is the
  // previous month and `kStartTime2` is the next month.
  SetFakeNowFromStr(kStartTime1);

  // Construct CalendarModel.
  base::Time now = base::Time::Now();
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(now, 1);
  calendar_model_ = std::make_unique<TestableCalendarModel>();

  // Non-prunable months are today's date and the two surrounding months.
  calendar_model_->AddNonPrunableMonths(months);

  // Inject all events.
  calendar_model_->InjectEvents(std::move(event_list));

  // Events from no months should now be present.
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime0, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime1, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime2, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime3, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime4, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime5, &events));

  // Fetch events from today's date and two surrounding months, i.e. the
  // non-prunable months.
  calendar_model_->FetchEvents(months);

  // Events from non-prunable months should be present, but not the other
  // months.
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime0, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime1, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime2, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime3, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime4, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime5, &events));

  // Move forward to `kStartTime4`.
  base::Time current_date = calendar_test_utils::GetTimeFromString(kStartTime4);
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);

  // Fetch events for `kStartTime4` and the two surrounding months.
  calendar_model_->FetchEvents(months);

  // Events from all months should now be present.
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime0, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime1, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime2, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime3, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime4, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime5, &events));

  // Clear out all non-prunable months.
  calendar_model_->ClearAllPrunableEvents();

  // Events from all non-prunable months should be present, but others not
  // present.
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime0, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime1, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime2, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime3, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime4, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime5, &events));

  // Now clear out all events.
  calendar_model_->ClearAllCachedEvents();

  // Events from all months prunable and non-prunable should not be present.
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime0, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime1, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime2, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime3, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime4, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime5, &events));
}

// A mock `CalendarClient`. This mock client's `GetEventList` waits for a short
// duration to mock the fetching process.
class CalendarClientTestImpl : public CalendarClient {
 public:
  CalendarClientTestImpl() = default;
  CalendarClientTestImpl(const CalendarClientTestImpl& other) = delete;
  CalendarClientTestImpl& operator=(const CalendarClientTestImpl& other) =
      delete;
  ~CalendarClientTestImpl() override = default;

  // CalendarClient:
  base::OnceClosure GetEventList(
      google_apis::calendar::CalendarEventListCallback callback,
      const base::Time& start_time,
      const base::Time& end_time) override {
    // Give it a little bit of time to mock the api calling.
    base::PlatformThread::Sleep(base::Seconds(1));
    std::move(callback).Run(google_apis::HTTP_SUCCESS, nullptr);
    return base::DoNothing();
  }
};

class CalendarModelFunctionTest : public AshTestBase {
 public:
  CalendarModelFunctionTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  CalendarModelFunctionTest(const CalendarModelFunctionTest& other) = delete;
  CalendarModelFunctionTest& operator=(const CalendarModelFunctionTest& other) =
      delete;
  ~CalendarModelFunctionTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Register a mock `CalendarClient` to the `CalendarController`.
    const std::string email = "user1@email.com";
    AccountId account_id = AccountId::FromUserEmail(email);
    Shell::Get()->calendar_controller()->SetActiveUserAccountIdForTesting(
        account_id);
    calendar_model_ = std::make_unique<CalendarModel>();
    calendar_client_ = std::make_unique<CalendarClientTestImpl>();
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id, calendar_client_.get());
  }

  void TearDown() override {
    calendar_model_.reset();

    AshTestBase::TearDown();
  }

  void InsertEvents(const google_apis::calendar::EventList* events) {
    calendar_model_->InsertEventsForTesting(events);
  }

  void FetchEvents(base::Time fetching_date) {
    calendar_model_->MaybeFetchMonth(fetching_date);
  }

  CalendarModel* calendar_model() { return calendar_model_.get(); }

 private:
  std::unique_ptr<CalendarModel> calendar_model_;
  std::unique_ptr<CalendarClientTestImpl> calendar_client_;
};

TEST_F(CalendarModelFunctionTest, FindFetchingStaus) {
  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      calendar_test_utils::CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event2 =
      calendar_test_utils::CreateEvent(kId2, kSummary2, kStartTime2, kEndTime2);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event3 =
      calendar_test_utils::CreateEvent(kId3, kSummary3, kStartTime3, kEndTime3);

  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("Greenwich Mean Time");
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event1));
  event_list->InjectItemForTesting(std::move(event2));
  event_list->InjectItemForTesting(std::move(event3));

  // Injects the above events.
  InsertEvents(event_list.get());

  // Starts fetching a date that is not in the cache.
  base::Time fetching_date =
      calendar_test_utils::GetTimeFromString("1 Apr 2022 00:00 GMT");
  FetchEvents(fetching_date);

  // The request for `fetching_date` is just sent out.
  EXPECT_EQ(CalendarModel::kFetching,
            calendar_model()->FindFetchingStaus(
                calendar_utils::GetStartOfMonthUTC(fetching_date)));

  // The request for kStartTime 0,1,2,3 are already finished (since the results
  // are injected).
  EXPECT_EQ(
      CalendarModel::kSuccess,
      calendar_model()->FindFetchingStaus(calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString(kStartTime0))));
  EXPECT_EQ(
      CalendarModel::kSuccess,
      calendar_model()->FindFetchingStaus(calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString(kStartTime1))));
  EXPECT_EQ(
      CalendarModel::kSuccess,
      calendar_model()->FindFetchingStaus(calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString(kStartTime2))));
  EXPECT_EQ(
      CalendarModel::kSuccess,
      calendar_model()->FindFetchingStaus(calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString(kStartTime3))));

  // The result of `kStartTime4` has never been injected. And the request has
  // never been sent either.
  EXPECT_EQ(
      CalendarModel::kNever,
      calendar_model()->FindFetchingStaus(calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString(kStartTime4))));

  // Wait until the response is back. The sleep duration may be in `base::Time`
  // or `base::TimeTicks`, depending on platform in the platform threads. So
  // using a relatively longer waiting duration to make sure the platform thread
  // sleeping ends.
  task_environment()->FastForwardBy(base::Minutes(10));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model()->FindFetchingStaus(
                calendar_utils::GetStartOfMonthUTC(fetching_date)));
}

}  // namespace ash
