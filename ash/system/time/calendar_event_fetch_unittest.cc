// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_fetch.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/shell.h"
#include "ash/system/power/peripheral_battery_listener.h"
#include "ash/system/time/calendar_event_fetch_types.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/account_id/account_id.h"
#include "google_apis/common/api_error_codes.h"

namespace ash {

// Subclass of CalendarClient where we control the results of a call to
// GetEventList(), which is what's being called downstream when a
// CalendarEventFetch is instantiated.
class TestCalendarClient : public CalendarClient {
 public:
  TestCalendarClient() = default;
  TestCalendarClient(const TestCalendarClient& other) = delete;
  TestCalendarClient& operator=(const TestCalendarClient& other) = delete;
  ~TestCalendarClient() override = default;

  bool IsDisabledByAdmin() const override { return false; }

  base::OnceClosure GetCalendarList(
      google_apis::calendar::CalendarListCallback callback) override {
    // This method should not be used during the event fetch unit test.
    return base::DoNothing();
  }

  base::OnceClosure GetEventList(
      google_apis::calendar::CalendarEventListCallback callback,
      const base::Time start_time,
      const base::Time end_time) override {
    // Store these off.
    start_time_ = start_time;
    callback_ = std::move(callback);

    // By delaying the response, we make the unit tests behave a little more
    // like a event fetch in production.  Use set_response_delay() to use a
    // value that's different from the default.
    StartResponseDelayTimeout();
    return base::BindOnce(&TestCalendarClient::CancelCallback,
                          weak_factory_.GetWeakPtr());
  }

  base::OnceClosure GetEventList(
      google_apis::calendar::CalendarEventListCallback callback,
      const base::Time start_time,
      const base::Time end_time,
      const std::string& calendar_id,
      const std::string& calendar_color_id) override {
    // Store these off.
    start_time_ = start_time;
    callback_ = std::move(callback);
    calendar_id_ = calendar_id;

    // By delaying the response, we make the unit tests behave a little more
    // like a event fetch in production.  Use set_response_delay() to use a
    // value that's different from the default.
    StartResponseDelayTimeout();
    return base::BindOnce(&TestCalendarClient::CancelCallback,
                          weak_factory_.GetWeakPtr());
  }

  void CancelCallback() { set_api_error_code(google_apis::CANCELLED); }

  void set_event_list(
      std::unique_ptr<google_apis::calendar::EventList> event_list) {
    event_list_ = std::move(event_list);
  }
  void set_api_error_code(google_apis::ApiErrorCode api_error_code) {
    api_error_code_ = api_error_code;
  }
  void set_response_delay(const base::TimeDelta& delay) {
    response_delay_ = delay;
  }
  const base::TimeDelta get_response_delay() { return response_delay_; }

 private:
  void StartResponseDelayTimeout() {
    fetch_response_timeout_.Start(
        FROM_HERE, response_delay_,
        base::BindRepeating(&TestCalendarClient::OnResponseDelayTimeout,
                            base::Unretained(this)));
  }

  void OnResponseDelayTimeout() {
    // We send back an event list, possibly empty, with every response.
    auto requested_event_list =
        std::make_unique<google_apis::calendar::EventList>();
    requested_event_list->set_time_zone("Greenwich Mean Time");

    // If we're set to return an error.
    if (api_error_code_ != google_apis::HTTP_SUCCESS) {
      std::move(callback_).Run(api_error_code_,
                               std::move(requested_event_list));
      return;
    }

    // If we have some events, send back any that start in the month we
    // requested.
    if (event_list_) {
      for (auto& event : event_list_->items()) {
        if (calendar_test_utils::IsTheSameMonth(event->start_time().date_time(),
                                                start_time_)) {
          requested_event_list->InjectItemForTesting(
              calendar_test_utils::CreateEvent(event->id().c_str(),
                                               event->summary().c_str(),
                                               event->start_time().date_time(),
                                               event->end_time().date_time()));
        }
      }
    }

    std::move(callback_).Run(api_error_code_, std::move(requested_event_list));
  }

  google_apis::calendar::CalendarEventListCallback callback_;
  base::Time start_time_;
  std::optional<std::string> calendar_id_;
  std::unique_ptr<google_apis::calendar::EventList> event_list_;
  google_apis::ApiErrorCode api_error_code_ = google_apis::HTTP_SUCCESS;
  base::RetainingOneShotTimer fetch_response_timeout_;
  base::TimeDelta response_delay_ = base::Milliseconds(100);

  base::WeakPtrFactory<TestCalendarClient> weak_factory_{this};
};

class CalendarEventFetchTest : public NoSessionAshTestBase {
 public:
  CalendarEventFetchTest()
      : NoSessionAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  CalendarEventFetchTest(const CalendarEventFetchTest& other) = delete;
  CalendarEventFetchTest& operator=(const CalendarEventFetchTest& other) =
      delete;
  ~CalendarEventFetchTest() override = default;

  void RegisterClient() {
    DCHECK(Shell::HasInstance());
    Shell::Get()->calendar_controller()->SetActiveUserAccountIdForTesting(
        GetDefaultUserId());
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        GetDefaultUserId(),
        /*client=*/&client_);
  }

  // Actual callback invoked when an event fetch is complete.
  void OnEventsFetched(base::Time start_of_month,
                       std::string calendar_id,
                       google_apis::ApiErrorCode error,
                       const google_apis::calendar::EventList* events) {
    calendar_id_ = calendar_id;
    api_error_code_ = error;
    events_fetched_count_ = 0;
    if (events)
      events_fetched_count_ = events->items().size();
  }

  // Callback invoked when an event fetch failed with an internal error.
  void OnEventFetchFailedInternalError(
      base::Time start_of_month,
      std::string calendar_id,
      CalendarEventFetchInternalErrorCode error) {
    calendar_id_ = calendar_id;
    internal_error_code_ = error;
  }

  base::Time GetStartOfMonthFromString(const char* str) {
    base::Time start_of_month;
    bool result = base::Time::FromString(str, &start_of_month);
    DCHECK(result);
    return start_of_month.UTCMidnight();
  }

  std::unique_ptr<CalendarEventFetch> PerformFetch(
      const base::Time start_of_month) {
    std::unique_ptr<CalendarEventFetch> fetch =
        std::make_unique<CalendarEventFetch>(
            start_of_month,
            base::BindRepeating(&CalendarEventFetchTest::OnEventsFetched,
                                base::Unretained(this)),
            base::BindRepeating(
                &CalendarEventFetchTest::OnEventFetchFailedInternalError,
                base::Unretained(this)),
            task_environment()->GetMockTickClock());
    return fetch;
  }

  std::unique_ptr<CalendarEventFetch> PerformFetchByCalendarId(
      const base::Time start_of_month,
      const std::string calendar_id,
      const std::string calendar_color_id) {
    std::unique_ptr<CalendarEventFetch> fetch =
        std::make_unique<CalendarEventFetch>(
            start_of_month,
            base::BindRepeating(&CalendarEventFetchTest::OnEventsFetched,
                                base::Unretained(this)),
            base::BindRepeating(
                &CalendarEventFetchTest::OnEventFetchFailedInternalError,
                base::Unretained(this)),
            task_environment()->GetMockTickClock(), calendar_id,
            calendar_color_id);
    return fetch;
  }

  std::optional<std::string> get_calendar_id() { return calendar_id_; }
  std::optional<int> events_fetched_count() { return events_fetched_count_; }
  std::optional<google_apis::ApiErrorCode> api_error_code() {
    return api_error_code_;
  }
  std::optional<CalendarEventFetchInternalErrorCode> internal_error_code() {
    return internal_error_code_;
  }

 protected:
  TestCalendarClient client_;

 private:
  const AccountId GetDefaultUserId() {
    return AccountId::FromUserEmail("user0@tray");
  }

  std::optional<std::string> calendar_id_;
  std::optional<int> events_fetched_count_;
  std::optional<google_apis::ApiErrorCode> api_error_code_;
  std::optional<CalendarEventFetchInternalErrorCode> internal_error_code_;

  base::WeakPtrFactory<CalendarEventFetchTest> weak_factory_{this};
};

TEST_F(CalendarEventFetchTest, NoEvents) {
  // Register our TestCalendarClient with the default user.
  RegisterClient();

  // Month whose events we want to fetch.
  base::Time start_of_month =
      GetStartOfMonthFromString("23 Oct 2009 11:30 GMT");

  // Perform the fetch.
  std::unique_ptr<CalendarEventFetch> fetch = PerformFetch(start_of_month);

  // Advance time to when the fetch is complete. `fetch` can no longer be used
  // after this.
  task_environment()->FastForwardBy(client_.get_response_delay());

  // No events were set in the client, so fetch should return no results.
  std::optional<int> count = events_fetched_count();
  EXPECT_TRUE(count.has_value() && count.value() == 0);

  // API error is HTTP_SUCCESS.
  std::optional<google_apis::ApiErrorCode> return_error_code = api_error_code();
  EXPECT_TRUE(return_error_code.has_value() &&
              return_error_code == google_apis::HTTP_SUCCESS);

  // No internal error.
  EXPECT_FALSE(internal_error_code().has_value());
}

TEST_F(CalendarEventFetchTest, HaveEvents) {
  // Register our TestCalendarClient with the default user.
  RegisterClient();

  // Inject two events, in different months.
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("Greenwich Mean Time");
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_0", "summary_0", "23 Oct 2009 11:30 GMT", "23 Oct 2009 12:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_1", "summary_1", "18 Nov 2021 8:15 GMT", "18 Nov 2021 11:30 GMT"));
  client_.set_event_list(std::move(event_list));

  // Month whose events we want to fetch, for which no events have been
  // injected.
  base::Time start_of_month =
      GetStartOfMonthFromString("27 Sep 1971 11:30 GMT");

  // Perform the fetch.
  std::unique_ptr<CalendarEventFetch> fetch = PerformFetch(start_of_month);

  // Advance time to when the fetch is complete. `fetch` can no longer be used
  // after this.
  task_environment()->FastForwardBy(client_.get_response_delay());

  // No events for this month in the client, so fetch should return no results.
  std::optional<int> count = events_fetched_count();
  EXPECT_TRUE(count.has_value() && count.value() == 0);

  // No internal error.
  EXPECT_FALSE(internal_error_code().has_value());

  // Now use a month that has events
  start_of_month = GetStartOfMonthFromString("23 Oct 2009 11:30 GMT");

  // Perform the fetch.
  std::unique_ptr<CalendarEventFetch> fetch2 = PerformFetch(start_of_month);

  // Advance time to when the fetch is complete. `fetch2` can no longer be
  // used after this.
  task_environment()->FastForwardBy(client_.get_response_delay());

  // There is one event for this month in the client, so fetch should return one
  // event.
  count = events_fetched_count();
  EXPECT_TRUE(count.has_value() && count.value() == 1);

  // API error is HTTP_SUCCESS.
  std::optional<google_apis::ApiErrorCode> return_error_code = api_error_code();
  EXPECT_TRUE(return_error_code.has_value() &&
              return_error_code == google_apis::HTTP_SUCCESS);

  // No internal error.
  EXPECT_FALSE(internal_error_code().has_value());
}

TEST_F(CalendarEventFetchTest, FetchEventsForNonPrimaryCalendar) {
  RegisterClient();

  // The month for which we want to fetch events.
  base::Time start_of_month = GetStartOfMonthFromString("01 Oct 2023 8:00 GMT");

  // The ID and color ID of the calendar we want to fetch events for.
  std::string calendar_id = "google.com_zu5cft6test@group.calendar.google.com";
  std::string calendar_color_id = "14";

  // Inject some events.
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("Greenwich Mean Time");
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_0", "summary_0", "02 Oct 2023 17:00 GMT", "02 Oct 2023 18:00 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_1", "summary_1", "05 Oct 2023 19:00 GMT", "05 Oct 2023 20:00 GMT"));
  client_.set_event_list(std::move(event_list));

  std::unique_ptr<CalendarEventFetch> fetch =
      PerformFetchByCalendarId(start_of_month, calendar_id, calendar_color_id);

  // Advance time to when the fetch is complete. `fetch` can no longer be used
  // after this.
  task_environment()->FastForwardBy(client_.get_response_delay());

  // There are two events for this month in the client, so fetch should return
  // two events.
  std::optional<int> count = events_fetched_count();
  EXPECT_TRUE(count.has_value() && count.value() == 2);

  // API error is HTTP_SUCCESS.
  std::optional<google_apis::ApiErrorCode> return_error_code = api_error_code();
  EXPECT_TRUE(return_error_code.has_value() &&
              return_error_code == google_apis::HTTP_SUCCESS);

  // No internal error.
  EXPECT_FALSE(internal_error_code().has_value());

  // The calendar ID assigned on fetch completion should equal the calendar ID
  // passed during the creation of the fetch.
  std::optional<std::string> fetch_calendar_id = get_calendar_id();
  EXPECT_TRUE(fetch_calendar_id.has_value() &&
              fetch_calendar_id == calendar_id);
}

TEST_F(CalendarEventFetchTest, ApiFailure) {
  // Specifically set up the fetch to fail with an API error.
  const google_apis::ApiErrorCode error_code = google_apis::HTTP_NOT_FOUND;
  client_.set_api_error_code(error_code);

  // Register our TestCalendarClient with the default user.
  RegisterClient();

  // Month whose events we want to fetch.
  base::Time start_of_month =
      GetStartOfMonthFromString("23 Oct 2009 11:30 GMT");

  // Perform the fetch.
  std::unique_ptr<CalendarEventFetch> fetch = PerformFetch(start_of_month);

  // Advance time to when the fetch is complete. `fetch` can no longer be used
  // after this.
  task_environment()->FastForwardBy(client_.get_response_delay());

  // No events were set in the client, so fetch should return no results.
  std::optional<int> count = events_fetched_count();
  EXPECT_TRUE(count.has_value() && count.value() == 0);

  // API error is what we set.
  std::optional<google_apis::ApiErrorCode> return_error_code = api_error_code();
  EXPECT_TRUE(return_error_code.has_value() && return_error_code == error_code);

  // No internal error.
  EXPECT_FALSE(internal_error_code().has_value());
}

TEST_F(CalendarEventFetchTest, Timeout) {
  base::HistogramTester histogram_tester;

  // No metrics recorded yet.
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Timeout", true,
                                     /*expected_count=*/0);

  // Specifically delay the response until after CalendarEventFetch declares a
  // timeout.
  client_.set_response_delay(calendar_utils::kCalendarDataFetchTimeout +
                             base::Milliseconds(100));

  // Register our TestCalendarClient with the default user.
  RegisterClient();

  // Month whose events we want to fetch.
  base::Time start_of_month =
      GetStartOfMonthFromString("23 Oct 2009 11:30 GMT");

  // No internal error code reported.
  std::optional<CalendarEventFetchInternalErrorCode> internal_error =
      internal_error_code();
  EXPECT_FALSE(internal_error.has_value());

  // Perform the fetch.
  std::unique_ptr<CalendarEventFetch> fetch = PerformFetch(start_of_month);

  // Advance time to when the fetch times out. `fetch` can no longer be used
  // after this.
  task_environment()->FastForwardBy(calendar_utils::kCalendarDataFetchTimeout);

  // Events should be completely nonexistent.
  EXPECT_FALSE(events_fetched_count().has_value());

  // API error should be completely nonexistent.
  std::optional<google_apis::ApiErrorCode> return_error_code = api_error_code();
  EXPECT_FALSE(return_error_code.has_value());

  // Internal error code reported is kTimeout.
  internal_error = internal_error_code();
  EXPECT_TRUE(internal_error.has_value() &&
              internal_error == CalendarEventFetchInternalErrorCode::kTimeout);

  // Metrics now recorded.
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Timeout", true,
                                     /*expected_count=*/1);
}

TEST_F(CalendarEventFetchTest, Cancel) {
  // Register our TestCalendarClient with the default user.
  RegisterClient();

  // Month whose events we want to fetch.
  base::Time start_of_month =
      GetStartOfMonthFromString("23 Oct 2009 11:30 GMT");

  // Perform the fetch.
  std::unique_ptr<CalendarEventFetch> fetch = PerformFetch(start_of_month);

  // Cancel the request.
  fetch->Cancel();

  // Advance time to when the fetch is complete. `fetch` can no longer be used
  // after this.
  task_environment()->FastForwardBy(client_.get_response_delay());

  // API error is CANCELLED.
  std::optional<google_apis::ApiErrorCode> return_error_code = api_error_code();
  EXPECT_TRUE(return_error_code.has_value() &&
              return_error_code == google_apis::CANCELLED);
}

}  // namespace ash
