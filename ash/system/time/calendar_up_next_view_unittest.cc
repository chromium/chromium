// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_up_next_view.h"

#include <utility>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_event_list_item_view.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_requests.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

namespace {

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const base::Time start_time,
    const base::Time end_time,
    bool all_day_event = false,
    const GURL video_conference_url = GURL(),
    const char* summary = "summary") {
  return calendar_test_utils::CreateEvent(
      "id_0", summary, start_time, end_time,
      google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
      google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted,
      all_day_event, video_conference_url);
}

std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>>
CreateUpcomingEvents(int event_count = 1,
                     bool all_day_event = false,
                     const GURL video_conference_url = GURL(),
                     const char* summary = "summary") {
  std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events;
  auto event_in_ten_mins_start_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
      base::Minutes(10);
  auto event_in_ten_mins_end_time =
      base::subtle::TimeNowIgnoringOverride().LocalMidnight() + base::Hours(1);
  for (int i = 0; i < event_count; ++i) {
    events.push_back(CreateEvent(event_in_ten_mins_start_time,
                                 event_in_ten_mins_end_time, all_day_event,
                                 video_conference_url, summary));
  }

  return events;
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
    if (!widget_) {
      widget_ = CreateFramelessTestWidget();
    }

    // Mock events being fetched.
    Shell::Get()->system_tray_model()->calendar_model()->OnEventsFetched(
        calendar_utils::GetStartOfMonthUTC(
            base::subtle::TimeNowIgnoringOverride().LocalMidnight()),
        google_apis::calendar::kPrimaryCalendarId,
        google_apis::ApiErrorCode::HTTP_SUCCESS,
        calendar_test_utils::CreateMockEventList(std::move(events)).get());

    auto up_next_view = std::make_unique<CalendarUpNextView>(
        controller_.get(), std::move(callback));
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

  void PressTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  }

  void PressShiftTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
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
  raw_ptr<CalendarUpNextView> up_next_view_;
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
       ShouldShowSingleEventWithShortTitleTakingUpFullWidthOfParentView) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Create UpNextView with a single upcoming event.
  CreateUpNextView(CreateUpcomingEvents());

  EXPECT_EQ(GetContentsView()->children().size(), size_t(1));
  EXPECT_EQ(GetContentsView()->children()[0]->width(),
            GetScrollView()->width());
}

TEST_F(CalendarUpNextViewTest,
       ShouldShowSingleEventWithLongTitleTakingUpFullWidthOfParentView) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Create UpNextView with a single upcoming event.
  CreateUpNextView(
      CreateUpcomingEvents(1, false, GURL(),
                           "Meeting title with really long long long long long "
                           "long long name that should ellipsis"));

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

  // Add multiple upcoming events.
  const int event_count = 5;
  CreateUpNextView(CreateUpcomingEvents(event_count));

  EXPECT_EQ(GetContentsView()->children().size(), size_t(event_count));
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

TEST_F(CalendarUpNextViewTest,
       ShouldScrollLeftAndRightWhenScrollButtonsArePressed_RTL) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Add multiple upcoming events.
  const int event_count = 5;
  CreateUpNextView(CreateUpcomingEvents(event_count));

  EXPECT_EQ(GetContentsView()->children().size(), size_t(event_count));
  EXPECT_EQ(ScrollPosition(), 0);

  // Sets the UI to be RTL.
  base::i18n::SetRTLForTesting(true);

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

  // Create UpNextView with a single upcoming event.
  CreateUpNextView(CreateUpcomingEvents());

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

  // Add multiple upcoming events.
  const int event_count = 5;
  CreateUpNextView(CreateUpcomingEvents(event_count));
  EXPECT_EQ(GetContentsView()->children().size(), size_t(event_count));

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

  // Add multiple upcoming events.
  const int event_count = 5;
  CreateUpNextView(CreateUpcomingEvents(event_count));
  EXPECT_EQ(GetContentsView()->children().size(), size_t(event_count));
  EXPECT_EQ(ScrollPosition(), 0);

  // Scroll right past the first event and so that the second event is partially
  // visible on the left of the scrollview.
  const int first_event_width =
      GetContentsView()->children()[0]->GetContentsBounds().width() +
      calendar_utils::kUpNextBetweenChildSpacing;
  const int scroll_position_partially_over_second_event =
      first_event_width + 50;
  ScrollHorizontalPositionTo(scroll_position_partially_over_second_event);
  ASSERT_EQ(scroll_position_partially_over_second_event, ScrollPosition());
  const views::View* first_event = GetContentsView()->children()[0];
  const views::View* second_event = GetContentsView()->children()[1];
  // Assert first view is not visible and second view is partially visible.
  ASSERT_EQ(0, first_event->GetVisibleBounds().width());
  EXPECT_LT(second_event->GetVisibleBounds().width(),
            second_event->GetContentsBounds().width());

  // Press scroll left. We should scroll so that the second event is aligned to
  // the start of the scroll view and fully visible. This is the equivalent
  // position of being scrolled to the right of the width of the first event.
  PressScrollLeftButton();
  EXPECT_EQ(first_event_width, ScrollPosition());
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

  // Add multiple upcoming events.
  const int event_count = 5;
  CreateUpNextView(CreateUpcomingEvents(event_count));
  EXPECT_EQ(GetContentsView()->children().size(), size_t(event_count));
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

  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&called](const ui::Event& event) { called = true; });

  // Create UpNextView with a single upcoming event.
  CreateUpNextView(CreateUpcomingEvents(), callback);
  EXPECT_FALSE(called);

  LeftClickOn(GetTodaysEventsButton());

  EXPECT_TRUE(called);
}

TEST_F(CalendarUpNextViewTest, ShouldTrackLaunchingFromEventListItem) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Create UpNextView with a single upcoming event.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  CreateUpNextView(CreateUpcomingEvents());
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
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  const int event_count = 5;
  CreateUpNextView(CreateUpcomingEvents(event_count));
  EXPECT_EQ(GetContentsView()->children().size(), size_t(event_count));

  histogram_tester->ExpectBucketCount(
      "Ash.Calendar.UpNextView.EventDisplayedCount", event_count, 1);
}

TEST_F(CalendarUpNextViewTest,
       ShouldLaunchAndTrackGoogleMeet_WhenJoinMeetingButtonPressed) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  auto histogram_tester = std::make_unique<base::HistogramTester>();
  // Create up next view with upcoming google meet event.
  CreateUpNextView(
      CreateUpcomingEvents(1, false, GURL("https://meet.google.com/abc-123")));
  EXPECT_EQ(GetContentsView()->children().size(), size_t(1));
  EXPECT_EQ(GetSystemTrayClient()->show_video_conference_count(), 0);

  // Click the "Join" meeting button.
  const auto* join_meeting_button =
      GetContentsView()->children()[0]->GetViewByID(kJoinButtonID);
  ASSERT_TRUE(join_meeting_button);
  LeftClickOn(join_meeting_button);

  EXPECT_EQ(GetSystemTrayClient()->show_video_conference_count(), 1);
  histogram_tester->ExpectTotalCount(
      "Ash.Calendar.UpNextView.JoinMeetingButton.Pressed", 1);
}

// Greenlines can be found in b/258648030.
TEST_F(CalendarUpNextViewTest, ShouldFocusViewsInCorrectOrder_WhenPressingTab) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Create up next view with 2 upcoming google meet events.
  CreateUpNextView(
      CreateUpcomingEvents(2, false, GURL("https://meet.google.com/abc-123")));
  EXPECT_EQ(GetContentsView()->children().size(), size_t(2));
  auto* focus_manager = up_next_view()->GetFocusManager();

  // First the event list item view should be focused.
  PressTab();
  auto* first_item = GetContentsView()->children()[0].get();
  ASSERT_TRUE(first_item);
  EXPECT_EQ(first_item, focus_manager->GetFocusedView());
  EXPECT_STREQ("CalendarEventListItemView",
               focus_manager->GetFocusedView()->GetClassName());

  // Next, the "Join" button should be focused.
  PressTab();
  EXPECT_EQ(first_item->GetViewByID(kJoinButtonID),
            focus_manager->GetFocusedView());

  // Next, the second event list item view should be focused.
  PressTab();
  auto* second_item = GetContentsView()->children()[1].get();
  ASSERT_TRUE(second_item);
  EXPECT_EQ(second_item, focus_manager->GetFocusedView());
  EXPECT_STREQ("CalendarEventListItemView",
               focus_manager->GetFocusedView()->GetClassName());

  // Next, the second event list item view "Join" button should be focused.
  PressTab();
  EXPECT_EQ(second_item->GetViewByID(kJoinButtonID),
            focus_manager->GetFocusedView());

  // Finally, the show upcoming events button should be focused.
  PressTab();
  EXPECT_EQ(GetTodaysEventsButton(), focus_manager->GetFocusedView());

  // Going back, the second event list item view "Join" button should be
  // focused.
  PressShiftTab();
  EXPECT_EQ(second_item->GetViewByID(kJoinButtonID),
            focus_manager->GetFocusedView());

  // Going back again, the second event list item view should be focused.
  PressShiftTab();
  EXPECT_EQ(second_item, focus_manager->GetFocusedView());
  EXPECT_STREQ("CalendarEventListItemView",
               focus_manager->GetFocusedView()->GetClassName());
}

// Add unittest for the fix of this bug: b/286596205.
TEST_F(CalendarUpNextViewTest, ShouldPreserveFocusAfterRefreshEvent) {
  // Set time override.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::subtle::TimeNowIgnoringOverride().LocalMidnight(); },
      nullptr, nullptr);

  // Create up next view with 2 upcoming google meet events.
  CreateUpNextView(
      CreateUpcomingEvents(2, false, GURL("https://meet.google.com/abc-123")));
  EXPECT_EQ(GetContentsView()->children().size(), size_t(2));
  auto* focus_manager = up_next_view()->GetFocusManager();

  // First the event list item view should be focused.
  PressTab();
  auto* first_item = GetContentsView()->children()[0].get();
  ASSERT_TRUE(first_item);
  EXPECT_EQ(first_item, focus_manager->GetFocusedView());
  EXPECT_STREQ("CalendarEventListItemView",
               focus_manager->GetFocusedView()->GetClassName());

  up_next_view()->RefreshEvents();

  // After refresh the events, the first event list item view should still be
  // focused.
  EXPECT_EQ(first_item, focus_manager->GetFocusedView());
  EXPECT_STREQ("CalendarEventListItemView",
               focus_manager->GetFocusedView()->GetClassName());
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
  // Add multiple upcoming events.
  const int event_count = 5;
  CreateUpNextView(CreateUpcomingEvents(event_count));
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
