// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/calendar/calendar_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/calendar_event_list_view.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_view.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "google_apis/calendar/calendar_api_requests.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash {
namespace {

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* id,
    const base::Time start_time,
    const base::Time end_time,
    const char* summary,
    const GURL video_conference_url = GURL()) {
  return calendar_test_utils::CreateEvent(
      id, summary, start_time, end_time,
      google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
      google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted, false,
      video_conference_url);
}

}  // namespace

class CalendarViewPixelTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    Shell::Get()->calendar_controller()->SetActiveUserAccountIdForTesting(
        account_id_);
    Shell::Get()->calendar_controller()->RegisterClientForUser(account_id_,
                                                               &client_);
  }

  void TearDown() override {
    Shell::Get()->calendar_controller()->RegisterClientForUser(account_id_,
                                                               nullptr);

    AshTestBase::TearDown();
  }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void OpenCalendarView() {
    // Presses the `DateTray` to open the `CalendarView`.
    GetPrimaryShelf()->GetStatusAreaWidget()->date_tray()->OnButtonPressed(
        ui::KeyEvent(ui::EventType::kMousePressed, ui::VKEY_UNKNOWN,
                     ui::EF_NONE));
    calendar_view_ = GetPrimaryUnifiedSystemTray()
                         ->bubble()
                         ->quick_settings_view()
                         ->GetDetailedViewForTest<CalendarView>();
  }

  CalendarView* GetCalendarView() { return calendar_view_; }

  void OpenEventListView() { calendar_view_->OpenEventListForTodaysDate(); }

  CalendarEventListView* GetEventListView() {
    return calendar_view_->event_list_view_;
  }

  void InsertEvents(
      std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events) {
    // Mock events fetch
    Shell::Get()->system_tray_model()->calendar_model()->OnEventsFetched(
        calendar_utils::GetStartOfMonthUTC(
            base::subtle::TimeNowIgnoringOverride().LocalMidnight()),
        google_apis::calendar::kPrimaryCalendarId,
        google_apis::ApiErrorCode::HTTP_SUCCESS,
        calendar_test_utils::CreateMockEventList(std::move(events)).get());
  }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }

 private:
  const AccountId account_id_ = AccountId::FromUserEmail("user1@email.com");
  calendar_test_utils::CalendarClientTestImpl client_;
  raw_ptr<CalendarView, DanglingUntriaged> calendar_view_ = nullptr;
  static base::Time fake_time_;
};

base::Time CalendarViewPixelTest::fake_time_;

TEST_F(CalendarViewPixelTest, Basics) {
  // Sets time override.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("14 Jun 2023 10:00 GMT", &date));
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewPixelTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  OpenCalendarView();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "calendar_view",
      /*revision_number=*/10, GetCalendarView()));
}

// Tests that the scroll view scrolls up when there are not at least 2 weeks
// visible below todays view (without up-next view).
TEST_F(CalendarViewPixelTest, Basics_ShowMoreFutureDates) {
  // Sets time override.
  base::Time date;

  // Sets today's date to be a later day in a month so that it will scroll up to
  // show at least two more rows of the current month.
  ASSERT_TRUE(base::Time::FromString("30 Jun 2023 10:00 GMT", &date));
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewPixelTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  OpenCalendarView();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "calendar_view_more_future_dates",
      /*revision_number=*/1, GetCalendarView()));
}

TEST_F(CalendarViewPixelTest, EventList) {
  // Sets time override.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("14 Jun 2023 10:00 GMT", &date));
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewPixelTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  // Adds events.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto start_time1 = date;
  auto end_time1 = date + base::Hours(1);
  auto start_time2 = date + base::Hours(2);
  auto end_time2 = date + base::Hours(3);
  events.push_back(CreateEvent("id_0", start_time1, end_time1, "Meeting 1"));
  events.push_back(CreateEvent(
      "id_1", start_time2, end_time2,
      "Event with a very very very very very very very long name that should "
      "ellipsis",
      GURL("https://meet.google.com/abc-123")));
  InsertEvents(std::move(events));

  OpenCalendarView();
  OpenEventListView();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "event_list_view",
      /*revision_number=*/11, GetEventListView()));
}

}  // namespace ash
