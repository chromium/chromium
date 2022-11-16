// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_up_next_view.h"

#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* start_time,
    const char* end_time,
    bool all_day_event = false) {
  return calendar_test_utils::CreateEvent(
      "id_0", "summary_0", start_time, end_time,
      google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
      google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted,
      all_day_event);
}

std::unique_ptr<google_apis::calendar::EventList> CreateMockEventList(
    std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>>& events) {
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("Greenwich Mean Time");

  for (auto& event : events)
    event_list->InjectItemForTesting(std::move(event));

  return event_list;
}

}  // namespace

class CalendarUpNextViewTest : public AshTestBase {
 public:
  CalendarUpNextViewTest() = default;
  CalendarUpNextViewTest(const CalendarUpNextViewTest&) = delete;
  CalendarUpNextViewTest& operator=(const CalendarUpNextViewTest&) = delete;
  ~CalendarUpNextViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<CalendarViewController>();
  }

  void TearDown() override {
    up_next_view_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateUpNextView(
      base::Time date,
      std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>>&
          events) {
    up_next_view_.reset();
    Shell::Get()->system_tray_model()->calendar_model()->OnEventsFetched(
        calendar_utils::GetStartOfMonthUTC(date),
        google_apis::ApiErrorCode::HTTP_SUCCESS,
        CreateMockEventList(events).get());
    up_next_view_ = std::make_unique<CalendarUpNextView>(controller_.get());
  }

  const views::Label* GetHeaderLabel() {
    return static_cast<views::Label*>(
        up_next_view_->header_view_->children()[0]);
  }

  const views::View* GetContentsView() {
    return static_cast<views::View*>(up_next_view_->content_view_);
  }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }

  CalendarViewController* controller() { return controller_.get(); }

  CalendarUpNextView* up_next_view() { return up_next_view_.get(); }

 private:
  std::unique_ptr<CalendarUpNextView> up_next_view_;
  std::unique_ptr<CalendarViewController> controller_;
  base::test::ScopedFeatureList features_;
  static base::Time fake_time_;
};

base::Time CalendarUpNextViewTest::fake_time_;

TEST_F(CalendarUpNextViewTest,
       GivenUpcomingEvents_WhenUpNextViewIsCreated_ThenShowEvents) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 09:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarUpNextViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  // Event starts in 10 mins.
  const char* event_in_ten_mins_start_time_string = "22 Nov 2021 09:10 GMT";
  const char* event_in_ten_mins_end_time_string = "22 Nov 2021 10:00 GMT";
  // Event in progress.
  const char* event_in_progress_start_time_string = "22 Nov 2021 08:30 GMT";
  const char* event_in_progress_end_time_string = "22 Nov 2021 09:30 GMT";

  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  events.emplace_back(CreateEvent(event_in_ten_mins_start_time_string,
                                  event_in_ten_mins_end_time_string));
  events.emplace_back(CreateEvent(event_in_progress_start_time_string,
                                  event_in_progress_end_time_string));

  CreateUpNextView(date, events);

  EXPECT_EQ(GetHeaderLabel()->GetText(), u"Up next");
  EXPECT_EQ(GetContentsView()->children().size(), size_t(2));
}

}  // namespace ash
