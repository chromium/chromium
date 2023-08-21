// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/calendar_event_list_view.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_view.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash {
namespace {

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* id,
    const base::Time start_time,
    const base::Time end_time,
    const char* summary) {
  return calendar_test_utils::CreateEvent(
      id, summary, start_time, end_time,
      google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
      google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted, false,
      GURL());
}

}  // namespace

class CalendarViewPixelTest : public AshTestBase {
 public:
  CalendarViewPixelTest() = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatures(
        {chromeos::features::kJelly, features::kQsRevamp,
         features::kCalendarJelly},
        {});
    AshTestBase::SetUp();
  }

  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void OpenCalendarView() {
    // Presses the `DateTray` to open the `CalendarView`.
    GetPrimaryShelf()->GetStatusAreaWidget()->date_tray()->OnButtonPressed(
        ui::KeyEvent(ui::EventType::ET_MOUSE_PRESSED, ui::VKEY_UNKNOWN,
                     ui::EF_NONE));
    calendar_view_ = static_cast<CalendarView*>(GetPrimaryUnifiedSystemTray()
                                                    ->bubble()
                                                    ->quick_settings_view()
                                                    ->GetDetailedViewForTest());
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
        google_apis::ApiErrorCode::HTTP_SUCCESS,
        calendar_test_utils::CreateMockEventList(std::move(events)).get());
  }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }

 private:
  raw_ptr<CalendarView, DanglingUntriaged | ExperimentalAsh> calendar_view_ =
      nullptr;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
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
      /*revision_number=*/2, GetCalendarView()));
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
      "ellipsis"));
  InsertEvents(std::move(events));

  OpenCalendarView();
  OpenEventListView();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "event_list_view",
      /*revision_number=*/4, GetEventListView()));
}

}  // namespace ash
