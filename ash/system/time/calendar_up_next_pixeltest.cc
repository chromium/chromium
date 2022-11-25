// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_up_next_view.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash {
namespace {

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const base::Time start_time,
    const base::Time end_time,
    bool all_day_event = false) {
  return calendar_test_utils::CreateEvent(
      "id_0", "Event with long name that should ellipsis", start_time, end_time,
      google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
      google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted,
      all_day_event);
}

}  // namespace

class CalendarUpNextViewPixelTest : public AshTestBase {
 public:
  CalendarUpNextViewPixelTest() = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    controller_ = std::make_unique<CalendarViewController>();
    widget_ = CreateFramelessTestWidget();
  }

  void TearDown() override {
    widget_.reset();
    controller_.reset();

    AshTestBase::TearDown();
  }

  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void CreateCalendarUpNextView(
      std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events) {
    // Mock events fetch
    Shell::Get()->system_tray_model()->calendar_model()->OnEventsFetched(
        calendar_utils::GetStartOfMonthUTC(
            base::subtle::TimeNowIgnoringOverride().LocalMidnight()),
        google_apis::ApiErrorCode::HTTP_SUCCESS,
        calendar_test_utils::CreateMockEventList(std::move(events)).get());

    up_next_view_ = widget_->SetContentsView(
        std::make_unique<CalendarUpNextView>(controller_.get()));
    widget_->SetSize(
        gfx::Size(kTrayMenuWidth, up_next_view_->GetPreferredSize().height()));
  }

  views::Widget* Widget() { return widget_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
  CalendarUpNextView* up_next_view_ = nullptr;
  std::unique_ptr<CalendarViewController> controller_;
};

TEST_F(
    CalendarUpNextViewPixelTest,
    GivenASingleUpcomingEvent_WhenUpNextViewIsCreated_ThenShouldDisplaySingleUpcomingEvent) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add single event starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto start_time = base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                    base::Minutes(10);
  auto end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  events.push_back(CreateEvent(start_time, end_time));

  CreateCalendarUpNextView(std::move(events));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "calendar_up_next_single_upcoming_event.rev_0", Widget()));
}

TEST_F(
    CalendarUpNextViewPixelTest,
    GivenThreeUpcomingEvents_WhenUpNextViewIsCreated_ThenShouldDisplayMultipleEventsInScrollView) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add 3 events starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto start_time = base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                    base::Minutes(10);
  auto end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  events.push_back(CreateEvent(start_time, end_time));
  events.push_back(CreateEvent(start_time, end_time));
  events.push_back(CreateEvent(start_time, end_time));

  CreateCalendarUpNextView(std::move(events));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "calendar_up_next_multiple_upcoming_events.rev_0", Widget()));
}

}  // namespace ash
