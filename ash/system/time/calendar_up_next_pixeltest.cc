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
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "google_apis/calendar/calendar_api_requests.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash {
namespace {

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const base::Time start_time,
    const base::Time end_time,
    const char* summary =
        "Event with a very very very very very very very long name that should "
        "ellipsis",
    bool all_day_event = false,
    const GURL video_conference_url = GURL()) {
  return calendar_test_utils::CreateEvent(
      "id_0", summary, start_time, end_time,
      google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
      google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted,
      all_day_event, video_conference_url);
}

}  // namespace

class CalendarUpNextViewPixelTest : public AshTestBase {
 public:
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
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void CreateCalendarUpNextView(
      std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events) {
    // Mock events fetch
    Shell::Get()->system_tray_model()->calendar_model()->OnEventsFetched(
        calendar_utils::GetStartOfMonthUTC(
            base::subtle::TimeNowIgnoringOverride().LocalMidnight()),
        google_apis::calendar::kPrimaryCalendarId,
        google_apis::ApiErrorCode::HTTP_SUCCESS,
        calendar_test_utils::CreateMockEventList(std::move(events)).get());

    up_next_view_ =
        widget_->SetContentsView(std::make_unique<CalendarUpNextView>(
            controller_.get(), views::Button::PressedCallback()));
    widget_->SetSize(
        gfx::Size(kTrayMenuWidth, up_next_view_->GetPreferredSize().height()));
  }

  views::Widget* Widget() { return widget_.get(); }

  const views::View* GetHeaderView() { return up_next_view_->header_view_; }

  const views::View* GetHeaderButtonContainerView() {
    return GetHeaderView()->children()[1];
  }

  const views::View* GetScrollRightButton() {
    return GetHeaderButtonContainerView()->children()[1];
  }

  void PressScrollRightButton() { PressScrollButton(GetScrollRightButton()); }

  // End the scrolling animation.
  void EndScrollingAnimation() { up_next_view_->scrolling_animation_->End(); }

 private:
  void PressScrollButton(const views::View* button) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
    event_generator->ClickLeftButton();
    // End the scrolling animation immediately so the pixel test images aren't
    // in an animating state. If we don't do this, the test images are taken
    // almost immediately and are incorrect.
    EndScrollingAnimation();
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<CalendarUpNextView, DanglingUntriaged> up_next_view_ = nullptr;
  std::unique_ptr<CalendarViewController> controller_;
};

TEST_F(CalendarUpNextViewPixelTest,
       ShouldShowSingleEventTakingUpFullWidthOfParentView) {
  // Set time and timezone override.
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");
  calendar_test_utils::ScopedLibcTimeZone scoped_libc_timezone(
      "America/Los_Angeles");
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
      "calendar_up_next_single_upcoming_event",
      /*revision_number=*/9, Widget()));
}

TEST_F(CalendarUpNextViewPixelTest,
       ShouldShowMultipleEventsInHorizontalScrollView) {
  // Set time and timezone override.
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");
  calendar_test_utils::ScopedLibcTimeZone scoped_libc_timezone(
      "America/Los_Angeles");
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
      "calendar_up_next_multiple_upcoming_events",
      /*revision_number=*/9, Widget()));
}

TEST_F(
    CalendarUpNextViewPixelTest,
    ShouldMakeSecondEventFullyVisibleAndLeftAligned_WhenScrollRightButtonIsPressed) {
  // Set time and timezone override.
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");
  calendar_test_utils::ScopedLibcTimeZone scoped_libc_timezone(
      "America/Los_Angeles");
  ASSERT_TRUE(scoped_libc_timezone.is_success());
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);
  auto now = base::subtle::TimeNowIgnoringOverride().LocalMidnight();

  // Add 3 events starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto start_time = now + base::Minutes(10);
  auto end_time = now + base::Hours(1);
  events.push_back(CreateEvent(start_time, end_time, "First event"));
  events.push_back(CreateEvent(start_time, end_time, "Second event"));
  events.push_back(CreateEvent(start_time, end_time, "Third event"));
  events.push_back(CreateEvent(start_time, end_time, "Fourth event"));

  CreateCalendarUpNextView(std::move(events));

  PressScrollRightButton();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "calendar_up_next_multiple_upcoming_events_press_scroll_right_button",
      /*revision_number=*/8, Widget()));
}

TEST_F(CalendarUpNextViewPixelTest, ShouldShowJoinMeetingButton) {
  // Set time and timezone override.
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");
  calendar_test_utils::ScopedLibcTimeZone scoped_libc_timezone(
      "America/Los_Angeles");
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add an upcoming event with a video_conference_url.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto start_time = base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                    base::Minutes(10);
  auto end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  events.push_back(CreateEvent(start_time, end_time, "First event", false,
                               GURL("https://meet.google.com/abc-123")));

  CreateCalendarUpNextView(std::move(events));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "calendar_up_next_join_button",
      /*revision_number=*/9, Widget()));
}

}  // namespace ash
