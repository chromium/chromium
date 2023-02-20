// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_item_view_jelly.h"

#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* start_time,
    const char* end_time,
    bool all_day_event = false,
    std::string hangout_link = "") {
  return calendar_test_utils::CreateEvent(
      "id_0", "summary_0", start_time, end_time,
      google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
      google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted,
      all_day_event, hangout_link);
}

}  // namespace

class CalendarViewEventListItemViewJellyTest : public AshTestBase {
 public:
  CalendarViewEventListItemViewJellyTest() = default;
  CalendarViewEventListItemViewJellyTest(
      const CalendarViewEventListItemViewJellyTest&) = delete;
  CalendarViewEventListItemViewJellyTest& operator=(
      const CalendarViewEventListItemViewJellyTest&) = delete;
  ~CalendarViewEventListItemViewJellyTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<CalendarViewController>();
  }

  void TearDown() override {
    event_list_item_view_jelly_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateEventListItemView(base::Time date,
                               google_apis::calendar::CalendarEvent* event,
                               bool round_top_corners = false,
                               bool round_bottom_corners = false,
                               bool show_event_list_dot = true,
                               int fixed_width = 0) {
    event_list_item_view_jelly_.reset();
    controller_->UpdateMonth(date);
    controller_->selected_date_ = date;
    event_list_item_view_jelly_ =
        std::make_unique<CalendarEventListItemViewJelly>(
            controller_.get(),
            SelectedDateParams{controller_->selected_date().value(),
                               controller_->selected_date_midnight(),
                               controller_->selected_date_midnight_utc()},
            *event, round_top_corners, round_bottom_corners,
            show_event_list_dot, fixed_width);
  }

  void SetSelectedDateInController(base::Time date) {
    controller_->selected_date_ = date;
    controller_->ShowEventListView(/*selected_calendar_date_cell_view=*/nullptr,
                                   date,
                                   /*row_index=*/0);
  }

  const views::Label* GetSummaryLabel() {
    return static_cast<views::Label*>(
        event_list_item_view_jelly_->GetViewByID(kSummaryLabelID));
  }

  const views::Label* GetTimeLabel() {
    return static_cast<views::Label*>(
        event_list_item_view_jelly_->GetViewByID(kTimeLabelID));
  }

  const views::View* GetEventListItemDot() {
    return static_cast<views::View*>(
        event_list_item_view_jelly_->GetViewByID(kEventListItemDotID));
  }

  const views::Button* GetJoinButton() {
    return static_cast<views::Button*>(
        event_list_item_view_jelly_->GetViewByID(kJoinButtonID));
  }

  CalendarViewController* controller() { return controller_.get(); }

  CalendarEventListItemViewJelly* event_list_item_view() {
    return event_list_item_view_jelly_.get();
  }

 private:
  std::unique_ptr<CalendarEventListItemViewJelly> event_list_item_view_jelly_;
  std::unique_ptr<CalendarViewController> controller_;
  base::test::ScopedFeatureList features_;
};

TEST_F(CalendarViewEventListItemViewJellyTest,
       ShouldShowCorrectLabels_GivenAOneHourEvent) {
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT+2");
  calendar_test_utils::ScopedLibcTimeZone scoped_libc_timezone("GMT+2");
  ASSERT_TRUE(scoped_libc_timezone.is_success());
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 00:00 UTC", &date));
  SetSelectedDateInController(date);
  const char* start_time_string = "22 Nov 2021 07:00 GMT";
  const char* end_time_string = "22 Nov 2021 08:00 GMT";
  const auto event = CreateEvent(start_time_string, end_time_string);

  CreateEventListItemView(date, event.get());

  const views::Label* summary_label = GetSummaryLabel();
  const views::Label* time_label = GetTimeLabel();
  EXPECT_EQ(u"summary_0", summary_label->GetText());
  EXPECT_EQ(u"9:00\u2009\x2013\u200910:00\u202fAM", time_label->GetText());
  EXPECT_EQ(u"summary_0, 9:00\u2009\x2013\u200910:00\u202fAM",
            summary_label->GetTooltipText());
  EXPECT_EQ(u"summary_0, 9:00\u2009\x2013\u200910:00\u202fAM",
            time_label->GetTooltipText());
  EXPECT_EQ(
      u"9:00\u202fAM to\n        10:00\u202fAM,\n        GMT+02:00,\n        "
      u"summary_0. Select for more details in Google Calendar.",
      event_list_item_view()->GetAccessibleName());
}

TEST_F(CalendarViewEventListItemViewJellyTest, TopRoundedCorners) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 00:00 UTC", &date));
  SetSelectedDateInController(date);
  const char* start_time_string = "22 Nov 2021 09:00 GMT";
  const char* end_time_string = "22 Nov 2021 10:00 GMT";
  const auto event = CreateEvent(start_time_string, end_time_string);

  CreateEventListItemView(date, event.get(), true);

  const ui::Layer* background_layer =
      event_list_item_view()->GetLayersInOrder().back();
  EXPECT_EQ(gfx::RoundedCornersF(16, 16, 0, 0),
            background_layer->rounded_corner_radii());
}

TEST_F(CalendarViewEventListItemViewJellyTest, BottomRoundedCorners) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 00:00 UTC", &date));
  SetSelectedDateInController(date);
  const char* start_time_string = "22 Nov 2021 09:00 GMT";
  const char* end_time_string = "22 Nov 2021 10:00 GMT";
  const auto event = CreateEvent(start_time_string, end_time_string);

  CreateEventListItemView(date, event.get(), /*round_top_corners=*/false,
                          /*round_bottom_corners=*/true);

  const ui::Layer* background_layer =
      event_list_item_view()->GetLayersInOrder().back();
  EXPECT_EQ(gfx::RoundedCornersF(0, 0, 16, 16),
            background_layer->rounded_corner_radii());
}

TEST_F(CalendarViewEventListItemViewJellyTest, AllRoundedCorners) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 00:00 UTC", &date));
  SetSelectedDateInController(date);
  const char* start_time_string = "22 Nov 2021 09:00 GMT";
  const char* end_time_string = "22 Nov 2021 10:00 GMT";
  const auto event = CreateEvent(start_time_string, end_time_string);

  CreateEventListItemView(date, event.get(), /*round_top_corners=*/true,
                          /*round_bottom_corners=*/true);

  const ui::Layer* background_layer =
      event_list_item_view()->GetLayersInOrder().back();
  EXPECT_EQ(gfx::RoundedCornersF(16, 16, 16, 16),
            background_layer->rounded_corner_radii());
}

TEST_F(CalendarViewEventListItemViewJellyTest, FixedLabelWidth) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 00:00 UTC", &date));
  SetSelectedDateInController(date);
  const char* start_time_string = "22 Nov 2021 09:00 GMT";
  const char* end_time_string = "22 Nov 2021 10:00 GMT";
  const auto event = CreateEvent(start_time_string, end_time_string);

  // If we don't set `fixed_width`, it will default to 0 (which the
  // `views::Label::SizeToFit()` method will ignore).
  CreateEventListItemView(date, event.get(), /*round_top_corners=*/true,
                          /*round_bottom_corners=*/true);

  // The label should have it's preferred size as we didn't set a `fixed_width`.
  EXPECT_TRUE(GetSummaryLabel()->width() > 0);

  // Set a fixed width. The label should be this size exactly.
  const auto fixed_width = 150;
  CreateEventListItemView(date, event.get(),
                          /*round_top_corners=*/true,
                          /*round_bottom_corners=*/true,
                          /*show_event_list_dot=*/true,
                          /*fixed_width=*/fixed_width);

  EXPECT_EQ(fixed_width, GetSummaryLabel()->width());
}

TEST_F(CalendarViewEventListItemViewJellyTest,
       ShouldShowAndHideEventListItemDot) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 00:00 UTC", &date));
  SetSelectedDateInController(date);
  const char* start_time_string = "22 Nov 2021 09:00 GMT";
  const char* end_time_string = "22 Nov 2021 10:00 GMT";
  const auto event = CreateEvent(start_time_string, end_time_string);

  // Set `show_event_list_dot` to false.
  CreateEventListItemView(date, event.get(), /*round_top_corners=*/true,
                          /*round_bottom_corners=*/true,
                          /*show_event_list_dot=*/false);

  // Event list dot should not exist.
  EXPECT_FALSE(GetEventListItemDot());

  // Set `show_event_list_dot` to true.
  CreateEventListItemView(date, event.get(), /*round_top_corners=*/true,
                          /*round_bottom_corners=*/true,
                          /*show_event_list_dot=*/true);

  // Event list dot should exist.
  EXPECT_TRUE(GetEventListItemDot());
}

TEST_F(CalendarViewEventListItemViewJellyTest,
       ShouldShowJoinMeetingButton_WhenGoogleMeetLinkExists) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 00:00 UTC", &date));
  SetSelectedDateInController(date);
  const char* start_time_string = "22 Nov 2021 09:00 GMT";
  const char* end_time_string = "22 Nov 2021 10:00 GMT";
  const auto event = CreateEvent(start_time_string, end_time_string, false,
                                 "https://meet.google.com/my-meeting");

  CreateEventListItemView(date, event.get(), /*round_top_corners=*/true,
                          /*round_bottom_corners=*/true,
                          /*show_event_list_dot=*/false);

  EXPECT_TRUE(GetJoinButton());
}

TEST_F(CalendarViewEventListItemViewJellyTest,
       ShouldHideJoinMeetingButton_WhenGoogleMeetLinkDoesNotExist) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 00:00 UTC", &date));
  SetSelectedDateInController(date);
  const char* start_time_string = "22 Nov 2021 09:00 GMT";
  const char* end_time_string = "22 Nov 2021 10:00 GMT";
  const auto event = CreateEvent(start_time_string, end_time_string);

  CreateEventListItemView(date, event.get(), /*round_top_corners=*/true,
                          /*round_bottom_corners=*/true,
                          /*show_event_list_dot=*/false);

  EXPECT_FALSE(GetJoinButton());
}

}  // namespace ash
