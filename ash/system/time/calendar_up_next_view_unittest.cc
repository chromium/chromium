// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_up_next_view.h"

#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

namespace {

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const base::Time start_time,
    const base::Time end_time,
    bool all_day_event = false) {
  return calendar_test_utils::CreateEvent(
      "id_0", "summary_0", start_time, end_time,
      google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
      google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted,
      all_day_event);
}

}  // namespace

class CalendarUpNextViewTest : public AshTestBase {
 public:
  CalendarUpNextViewTest() = default;
  explicit CalendarUpNextViewTest(
      base::test::TaskEnvironment::TimeSource time_source)
      : AshTestBase(time_source) {}

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<CalendarViewController>();
  }

  void TearDown() override {
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateUpNextView(
      std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events,
      views::Button::PressedCallback callback =
          views::Button::PressedCallback()) {
    if (!widget_)
      widget_ = CreateFramelessTestWidget();

    // Mock events being fetched.
    Shell::Get()->system_tray_model()->calendar_model()->OnEventsFetched(
        calendar_utils::GetStartOfMonthUTC(
            base::subtle::TimeNowIgnoringOverride().LocalMidnight()),
        google_apis::ApiErrorCode::HTTP_SUCCESS,
        calendar_test_utils::CreateMockEventList(std::move(events)).get());

    auto up_next_view =
        std::make_unique<CalendarUpNextView>(controller_.get(), callback);
    up_next_view_ = widget_->SetContentsView(std::move(up_next_view));
    // Set the widget to reflect the CalendarUpNextView size in reality. If we
    // don't then the view will never be scrollable.
    widget_->SetSize(
        gfx::Size(kTrayMenuWidth, up_next_view_->GetPreferredSize().height()));
  }

  const views::View* GetHeaderView() { return up_next_view_->header_view_; }

  const views::Label* GetHeaderLabel() {
    return static_cast<views::Label*>(GetHeaderView()->children()[0]);
  }

  const views::View* GetContentsView() { return up_next_view_->content_view_; }

  const views::ScrollView* GetScrollView() {
    return up_next_view_->scroll_view_;
  }

  const views::View* GetScrollLeftButton() {
    return up_next_view_->left_scroll_button_;
  }

  const views::View* GetScrollRightButton() {
    return up_next_view_->right_scroll_button_;
  }

  const views::View* GetTodaysEventsButton() {
    return up_next_view_->todays_events_button_container_->children()[0];
  }

  virtual void PressScrollLeftButton() {
    PressScrollButton(GetScrollLeftButton());
  }

  virtual void PressScrollRightButton() {
    PressScrollButton(GetScrollRightButton());
  }

  int ScrollPosition() { return GetScrollView()->GetVisibleRect().x(); }

  void ScrollHorizontalPositionTo(int position_in_px) {
    up_next_view_->scroll_view_->ScrollToPosition(
        up_next_view_->scroll_view_->horizontal_scroll_bar(), position_in_px);
  }

  // End the scrolling animation.
  void EndScrollingAnimation() { up_next_view_->scrolling_animation_->End(); }

  CalendarViewController* controller() { return controller_.get(); }

  CalendarUpNextView* up_next_view() { return up_next_view_; }

 private:
  void PressScrollButton(const views::View* button) {
    LeftClickOn(button);
    // End the scrolling animation immediately so tests can assert the results
    // of scrolling. If we don't do this, the test assertions run immediately
    // (and fail) due to animations concurrently running.
    EndScrollingAnimation();
  }

  std::unique_ptr<views::Widget> widget_;
  CalendarUpNextView* up_next_view_;
  std::unique_ptr<CalendarViewController> controller_;
};

TEST_F(CalendarUpNextViewTest, ShouldShowMultipleUpcomingEvents) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add event starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(10);
  auto event_in_ten_mins_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);

  // Add event that's in progress.
  auto event_in_progress_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() -
      base::Minutes(30);
  auto event_in_progress_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(30);

  events.push_back(
      CreateEvent(event_in_ten_mins_start_time, event_in_ten_mins_end_time));
  events.push_back(
      CreateEvent(event_in_progress_start_time, event_in_progress_end_time));

  CreateUpNextView(std::move(events));

  EXPECT_EQ(GetHeaderLabel()->GetText(), u"Up next");
  EXPECT_EQ(GetContentsView()->children().size(), size_t(2));
}

TEST_F(CalendarUpNextViewTest,
       ShouldShowSingleEventTakingUpFullWidthOfParentView) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add single event starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(10);
  auto event_in_ten_mins_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  events.push_back(
      CreateEvent(event_in_ten_mins_start_time, event_in_ten_mins_end_time));

  CreateUpNextView(std::move(events));

  EXPECT_EQ(GetContentsView()->children().size(), size_t(1));
  EXPECT_EQ(GetContentsView()->children()[0]->width(),
            GetScrollView()->width());
}

TEST_F(CalendarUpNextViewTest,
       ShouldScrollLeftAndRightWhenScrollButtonsArePressed) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add multiple events starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(10);
  auto event_in_ten_mins_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  for (int i = 0; i < 5; ++i) {
    events.push_back(
        CreateEvent(event_in_ten_mins_start_time, event_in_ten_mins_end_time));
  }

  CreateUpNextView(std::move(events));

  EXPECT_EQ(GetContentsView()->children().size(), size_t(5));
  EXPECT_EQ(ScrollPosition(), 0);

  // Press scroll right. We should scroll past the first event + margin.
  const int first_event_width =
      GetContentsView()->children()[0]->GetContentsBounds().width() +
      calendar_utils::kUpNextBetweenChildSpacing;
  PressScrollRightButton();
  EXPECT_EQ(ScrollPosition(), first_event_width);

  // Press scroll right again. We should scroll past the second event +
  // margin.
  const int second_event_width =
      GetContentsView()->children()[1]->GetContentsBounds().width() +
      calendar_utils::kUpNextBetweenChildSpacing;
  PressScrollRightButton();
  EXPECT_EQ(ScrollPosition(), first_event_width + second_event_width);

  // Press scroll left. Now we should be back to being past the first event +
  // margin.
  PressScrollLeftButton();
  EXPECT_EQ(ScrollPosition(), first_event_width);

  // Press scroll left again. We should be back at the beginning of the scroll
  // view.
  PressScrollLeftButton();
  EXPECT_EQ(ScrollPosition(), 0);
}

TEST_F(CalendarUpNextViewTest, ShouldHideScrollButtons_WhenOnlyOneEvent) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add single event starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(10);
  auto event_in_ten_mins_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  events.push_back(
      CreateEvent(event_in_ten_mins_start_time, event_in_ten_mins_end_time));

  CreateUpNextView(std::move(events));

  EXPECT_EQ(GetContentsView()->children().size(), size_t(1));
  EXPECT_EQ(ScrollPosition(), 0);

  // With only one event, there won't be any room to scroll in either direction
  // so the buttons should be hidden.
  EXPECT_FALSE(GetScrollLeftButton()->GetVisible());
  EXPECT_FALSE(GetScrollRightButton()->GetVisible());
}

TEST_F(CalendarUpNextViewTest, ShouldShowScrollButtons_WhenMultipleEvents) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add multiple events starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(10);
  auto event_in_ten_mins_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  for (int i = 0; i < 5; ++i) {
    events.push_back(
        CreateEvent(event_in_ten_mins_start_time, event_in_ten_mins_end_time));
  }

  CreateUpNextView(std::move(events));

  EXPECT_EQ(GetContentsView()->children().size(), size_t(5));

  // At the start the scroll left button should be disabled and visible.
  EXPECT_EQ(ScrollPosition(), 0);
  EXPECT_FALSE(GetScrollLeftButton()->GetEnabled());
  EXPECT_TRUE(GetScrollLeftButton()->GetVisible());
  EXPECT_TRUE(GetScrollRightButton()->GetEnabled());
  EXPECT_TRUE(GetScrollRightButton()->GetVisible());

  PressScrollRightButton();

  // After scrolling right a bit, both buttons should be enabled and visible.
  EXPECT_TRUE(GetScrollLeftButton()->GetEnabled());
  EXPECT_TRUE(GetScrollLeftButton()->GetVisible());
  EXPECT_TRUE(GetScrollRightButton()->GetEnabled());
  EXPECT_TRUE(GetScrollRightButton()->GetVisible());

  PressScrollRightButton();
  PressScrollRightButton();
  PressScrollRightButton();

  // After scrolling to the end, the scroll right button should be disabled and
  // visible.
  EXPECT_TRUE(GetScrollLeftButton()->GetEnabled());
  EXPECT_FALSE(GetScrollRightButton()->GetEnabled());
  EXPECT_TRUE(GetScrollRightButton()->GetVisible());
}

// If we have a partially visible event view and the scroll left button is
// pressed, we should scroll to put the whole event into view, aligned to the
// start of the viewport.
//          [---------------] <-- ScrollView viewport
// [-E1-] [---E2---]          <-- Event 2 partially shown in the viewport.
// Press scroll left button.
//          [---------------] <-- ScrollView viewport
//   [-E1-] [---E2---]        <-- Event 2 now fully shown in viewport.
TEST_F(
    CalendarUpNextViewTest,
    ShouldMakeCurrentOrPreviousEventFullyVisibleAndLeftAligned_WhenScrollLeftButtonIsPressed) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);
  calendar_test_utils::ScopedLibcTimeZone scoped_libc_timezone("GMT");
  ASSERT_TRUE(scoped_libc_timezone.is_success());

  // Add multiple events starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(10);
  auto event_in_ten_mins_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  for (int i = 0; i < 5; ++i) {
    events.push_back(
        CreateEvent(event_in_ten_mins_start_time, event_in_ten_mins_end_time));
  }

  CreateUpNextView(std::move(events));

  EXPECT_EQ(GetContentsView()->children().size(), size_t(5));
  EXPECT_EQ(ScrollPosition(), 0);

  // Scroll right so the second event is partially visible on the left of the
  // scrollview.
  ScrollHorizontalPositionTo(200);
  ASSERT_EQ(ScrollPosition(), 200);
  const views::View* second_event = GetContentsView()->children()[1];
  // Assert second view is partially visible.
  EXPECT_TRUE(second_event->GetVisibleBounds().width() <
              second_event->GetContentsBounds().width());

  // Press scroll left. We should scroll so that the second event is aligned to
  // the start of the scroll view and fully visible. This is the equivalent
  // position of being scrolled to the right of the width of the first event.
  const int first_event_width =
      GetContentsView()->children()[0]->GetContentsBounds().width() +
      calendar_utils::kUpNextBetweenChildSpacing;
  PressScrollLeftButton();
  EXPECT_EQ(ScrollPosition(), first_event_width);
}

// If we have a partially visible event and the scroll right button is pressed,
// we should scroll to put the whole event into view, aligned to the start of
// the viewport.
// If we scroll right for a partially visible event view.
//           [---------------]      <-- ScrollView viewport
//           [--E1--]    [--E2--]   <-- Event 2 partially shown in the viewport.
// Press scroll right button.
//           [---------------]      <-- ScrollView viewport
// [--E1--]  [--E2--]               <-- Event 2 now fully shown in the viewport.
TEST_F(
    CalendarUpNextViewTest,
    ShouldMakeNextEventFullyVisibleAndLeftAligned_WhenScrollRightButtonIsPressed) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add multiple events starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(10);
  auto event_in_ten_mins_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  for (int i = 0; i < 5; ++i) {
    events.push_back(
        CreateEvent(event_in_ten_mins_start_time, event_in_ten_mins_end_time));
  }

  CreateUpNextView(std::move(events));

  EXPECT_EQ(GetContentsView()->children().size(), size_t(5));
  EXPECT_EQ(ScrollPosition(), 0);

  ScrollHorizontalPositionTo(100);
  ASSERT_EQ(ScrollPosition(), 100);
  const views::View* first_event = GetContentsView()->children()[0];
  // Assert first view is partially visible.
  EXPECT_TRUE(first_event->GetVisibleBounds().width() <
              first_event->GetContentsBounds().width());

  // Press scroll right. We should scroll past the first event + margin to
  // show the second event, aligned to the start of the scroll view.
  const int first_event_width = first_event->GetContentsBounds().width() +
                                calendar_utils::kUpNextBetweenChildSpacing;
  PressScrollRightButton();
  EXPECT_EQ(ScrollPosition(), first_event_width);
}

TEST_F(CalendarUpNextViewTest,
       ShouldInvokeCallback_WhenTodaysEventButtonPressed) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add event starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(10);
  auto event_in_ten_mins_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  events.push_back(
      CreateEvent(event_in_ten_mins_start_time, event_in_ten_mins_end_time));

  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&called](const ui::Event& event) { called = true; });

  CreateUpNextView(std::move(events), callback);
  EXPECT_FALSE(called);

  LeftClickOn(GetTodaysEventsButton());

  EXPECT_TRUE(called);
}

TEST_F(CalendarUpNextViewTest, ShouldTrackLaunchingFromEventListItem) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add an upcoming event.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(10);
  auto event_in_ten_mins_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  events.push_back(
      CreateEvent(event_in_ten_mins_start_time, event_in_ten_mins_end_time));

  auto histogram_tester = std::make_unique<base::HistogramTester>();
  CreateUpNextView(std::move(events));

  EXPECT_EQ(GetContentsView()->children().size(), size_t(1));

  // Click event inside the scrollview contents.
  LeftClickOn(GetContentsView()->children()[0]);

  histogram_tester->ExpectTotalCount(
      "Ash.Calendar.UpNextView.EventListItem.Pressed", 1);
}

TEST_F(CalendarUpNextViewTest, ShouldTrackEventDisplayedCount) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add 5 upcoming events.
  const int event_count = 5;
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(10);
  auto event_in_ten_mins_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  for (int i = 0; i < event_count; ++i) {
    events.push_back(
        CreateEvent(event_in_ten_mins_start_time, event_in_ten_mins_end_time));
  }

  auto histogram_tester = std::make_unique<base::HistogramTester>();
  CreateUpNextView(std::move(events));

  EXPECT_EQ(GetContentsView()->children().size(), size_t(event_count));

  histogram_tester->ExpectBucketCount(
      "Ash.Calendar.UpNextView.EventDisplayedCount", event_count, 1);
}

class CalendarUpNextViewAnimationTest : public CalendarUpNextViewTest {
 public:
  CalendarUpNextViewAnimationTest()
      : CalendarUpNextViewTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void PressScrollLeftButton() override { LeftClickOn(GetScrollLeftButton()); }

  void PressScrollRightButton() override {
    LeftClickOn(GetScrollRightButton());
  }

  bool IsAnimating() {
    return up_next_view()->scrolling_animation_ &&
           up_next_view()->scrolling_animation_->is_animating();
  }

  const base::TimeDelta kAnimationStartBufferDuration = base::Milliseconds(50);
  const base::TimeDelta kAnimationFinishedDuration = base::Seconds(1);
};

// Flaky: https://crbug.com/1401505
TEST_F(CalendarUpNextViewAnimationTest,
       DISABLED_ShouldAnimateScrollView_WhenScrollButtonsArePressed) {
  // Add multiple events starting in 10 mins.
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time = base::Time::Now() + base::Minutes(10);
  auto event_in_ten_mins_end_time = base::Time::Now() + base::Hours(1);
  for (int i = 0; i < 5; ++i) {
    events.push_back(
        CreateEvent(event_in_ten_mins_start_time, event_in_ten_mins_end_time));
  }

  CreateUpNextView(std::move(events));
  EXPECT_FALSE(IsAnimating());

  PressScrollRightButton();
  task_environment()->FastForwardBy(kAnimationStartBufferDuration);
  EXPECT_TRUE(IsAnimating());

  task_environment()->FastForwardBy(kAnimationFinishedDuration);
  EXPECT_FALSE(IsAnimating());

  PressScrollLeftButton();
  task_environment()->FastForwardBy(kAnimationStartBufferDuration);
  EXPECT_TRUE(IsAnimating());

  task_environment()->FastForwardBy(kAnimationFinishedDuration);
  EXPECT_FALSE(IsAnimating());
}

}  // namespace ash
