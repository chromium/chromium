// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/time/calendar_up_next_view.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

namespace ui {
class Event;
}  // namespace ui

namespace views {

class Label;

}  // namespace views

namespace ash {

class CalendarEventListView;
class CalendarMonthView;
class IconButton;
class CalendarView;

// The header of the calendar view, which shows the current month and year.
class CalendarHeaderView : public views::View {
 public:
  METADATA_HEADER(CalendarHeaderView);

  CalendarHeaderView(const std::u16string& month, const std::u16string& year);
  CalendarHeaderView(const CalendarHeaderView& other) = delete;
  CalendarHeaderView& operator=(const CalendarHeaderView& other) = delete;
  ~CalendarHeaderView() override;

  // Updates the month and year labels.
  void UpdateHeaders(const std::u16string& month, const std::u16string& year);

 private:
  friend class CalendarViewAnimationTest;
  friend class CalendarViewTest;

  // The main header which shows the month name.
  const raw_ptr<views::Label, ExperimentalAsh> header_;

  // The year header which follows the `header_`.
  const raw_ptr<views::Label, ExperimentalAsh> header_year_;
};

// This view displays a scrollable calendar.
class ASH_EXPORT CalendarView : public CalendarModel::Observer,
                                public CalendarViewController::Observer,
                                public GlanceableTrayChildBubble,
                                public views::ViewObserver {
 public:
  METADATA_HEADER(CalendarView);

  // `for_glanceables_container` - Whether the calendar view is shown as a
  // bubble in glanceables container, or a `UnifiedSystemTrayBubble` (which is
  // the case if glanceables feature is not enabled).
  CalendarView(DetailedViewDelegate* delegate, bool for_glanceables_container);
  CalendarView(const CalendarView& other) = delete;
  CalendarView& operator=(const CalendarView& other) = delete;
  ~CalendarView() override;

  // CalendarModel::Observer:
  void OnEventsFetched(const CalendarModel::FetchingStatus status,
                       const base::Time start_time,
                       const google_apis::calendar::EventList* events) override;
  void OnTimeout(base::Time start_of_month) override;

  // CalendarViewController::Observer:
  void OnMonthChanged() override;
  void OpenEventList() override;
  void CloseEventList() override;
  void OnSelectedDateUpdated() override;
  void OnCalendarLoaded() override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewFocused(View* observed_view) override;

  // views::View:
  void OnEvent(ui::Event* event) override;

  // TrayDetailedView:
  void CreateExtraTitleRowButtons() override;
  views::Button* CreateInfoButton(views::Button::PressedCallback callback,
                                  int info_accessible_name_id) override;

  CalendarViewController* calendar_view_controller() {
    return calendar_view_controller_.get();
  }

  CalendarUpNextView* up_next_view() { return up_next_view_; }
  CalendarEventListView* event_list_view() { return event_list_view_; }

  // Sets the bounds of the container of the `up_next_view_` and
  // `event_list_view_` to be flush with the bottom of the scroll view. Only the
  // position will be animated, so give the view its final bounds. The
  // `event_list_view_open` need to be passed in, because under some cases, the
  // `event_list_view_` is still there but we want to use the `up_next_view_`'s
  // bounds.
  void SetCalendarSlidingSurfaceBounds(bool event_list_view_open);

 private:
  // The header of each month view which shows the month's name. If the year of
  // this month is not the same as the current month, the year is also shown in
  // this view.
  class MonthHeaderLabelView;

  // Content view of calendar's scroll view, used for metrics recording.
  // TODO(crbug.com/1297376): Add unit tests for metrics recording.
  class ScrollContentsView : public views::View {
   public:
    explicit ScrollContentsView(CalendarViewController* controller);
    ScrollContentsView(const ScrollContentsView& other) = delete;
    ScrollContentsView& operator=(const ScrollContentsView& other) = delete;
    ~ScrollContentsView() override = default;

    // Update the value of current month based on the controller.
    void OnMonthChanged();

    // views::View:
    void OnEvent(ui::Event* event) override;

    // Called when a stylus touch event is triggered.
    void OnStylusEvent(const ui::TouchEvent& event);

   private:
    // Used as a Shell pre-target handler to notify the owner of stylus events.
    class StylusEventHandler : public ui::EventHandler {
     public:
      explicit StylusEventHandler(ScrollContentsView* content_view);
      StylusEventHandler(const StylusEventHandler&) = delete;
      StylusEventHandler& operator=(const StylusEventHandler&) = delete;
      ~StylusEventHandler() override;

      // ui::EventHandler:
      void OnTouchEvent(ui::TouchEvent* event) override;

     private:
      raw_ptr<ScrollContentsView, ExperimentalAsh> content_view_;
    };

    const raw_ptr<CalendarViewController, DanglingUntriaged | ExperimentalAsh>
        controller_;
    StylusEventHandler stylus_event_handler_;

    // Since we only record metrics once when we scroll through a particular
    // month. This keeps track the current month in display that we have already
    // recorded metrics.
    std::u16string current_month_;
  };

  // The types to create the `MonthHeaderLabelView` which are in corresponding
  // to the 4 months: `previous_month_`, `current_month_`, `next_month_` and
  // `next_next_month_`.
  enum LabelType { PREVIOUS, CURRENT, NEXT, NEXTNEXT };

  friend class CalendarViewTest;
  friend class CalendarViewPixelTest;
  friend class CalendarViewAnimationTest;

  // Assigns month views and labels based on the current date on screen.
  void SetMonthViews();

  // Returns the current month first row position.
  int PositionOfCurrentMonth() const;

  // Returns the today's row position.
  int PositionOfToday() const;

  // Returns the selected date's row position.
  int PositionOfSelectedDate() const;

  // Adds a month label.
  views::View* AddLabelWithId(LabelType type, bool add_at_front = false);

  // Adds a `CalendarMonthView`.
  CalendarMonthView* AddMonth(base::Time month_first_date,
                              bool add_at_front = false);

  // Deletes the current next month and add a new month at the top of the
  // `content_view_`.
  void ScrollUpOneMonth();

  // Deletes the current previous month and adds a new month at the bottom of
  // the `content_view_`.
  void ScrollDownOneMonth();

  // Scrolls up or down one month then auto scrolls to the current month's first
  // row.
  void ScrollOneMonthAndAutoScroll(bool scroll_up);

  // Shows the scrolling animation then scrolls one month then auto scroll to
  // the current month's first row.
  void ScrollOneMonthWithAnimation(bool scroll_up);

  // Sets up the `temp_header_` to prepare for the header animation and returns
  // the moving transform for the header.
  gfx::Transform GetHeaderMovingAndPrepareAnimation(
      bool scroll_up,
      const std::string& animation_name,
      const std::u16string& temp_month,
      const std::u16string& temp_year);

  // Scrolls up/down one row based on `scroll_up`.
  void ScrollOneRowWithAnimation(bool scroll_up);

  // Sets opacity for header and content view (which contains previous, current
  // and next month with their labels),
  void SetHeaderAndContentViewOpacity(float opacity);

  // Enables or disables `should_months_animate_` and `scroll_view_` vertical
  // scroll bar mode.
  void SetShouldMonthsAnimateAndScrollEnabled(bool enabled);

  // Fades out on-screen month, sets date to today by calling `ResetToToday` and
  // fades in updated views after.
  void ResetToTodayWithAnimation();

  // Removes on-screen month and adds today's date month and label views without
  // animation.
  void ResetToToday();

  // Updates the on-screen month map with the current months on screen.
  void UpdateOnScreenMonthMap();

  // Returns whether or not we've finished fetching CalendarEvents.
  bool EventsFetchComplete();

  // Checks if all months in the visible window have finished fetching. If so,
  // stop showing the loading bar.
  void MaybeUpdateLoadingBarVisibility();

  // Fades in current month.
  void FadeInCurrentMonth();

  // Updates the `header_`'s month and year to the current month and year.
  void UpdateHeaders();

  // Resets the `header_`'s opacity and position. Also resets
  // `scrolling_settled_timer_` and `header_animation_restart_timer_`.
  void RestoreHeadersStatus();

  // Resets the the month views' opacity and position. In case the animation is
  // aborted in the middle and the view's are not in the original status.
  void RestoreMonthStatus();

  // Auto scrolls to today. If the view is big enough we scroll to the first row
  // of today's month, otherwise we scroll to the position of today's row.
  void ScrollToToday();

  // If currently focusing on any date cell.
  bool IsDateCellViewFocused();

  // Returns whether `header_`, `current_month_`, `content_view_`, or
  // `event_list_view_` are animating.
  bool IsAnimating();

  // If focusing on `CalendarDateCellView` is interrupted (by scrolling or by
  // today's button), resets the content view's `FocusBehavior` to `ALWAYS`.
  void MaybeResetContentViewFocusBehavior();

  // We only fetch events after we've "settled" on the current on-screen month.
  void OnScrollingSettledTimerFired();

  // Sets `expanded_row_index_` and auto-scrolls the `scroll_view_` when
  // `event_list_view_` is opened. After scrolling, disables the scroll bar.
  void SetExpandedRowThenDisableScroll(int row_index);

  // ScrollView callback.
  void OnContentsScrolled();

  // Callback passed to `up_button_` and `down_button_`, activated on button
  // activation.
  void OnMonthArrowButtonActivated(bool up, const ui::Event& event);

  // Adjusts the Chrome Vox box position for date cells in the scroll view.
  void AdjustDateCellVoxBounds();

  // Performs cleanup on temporary views after the scroll animation is complete,
  // and re-enables the month and header animation.
  void OnScrollMonthAnimationComplete(bool scroll_up);

  // Handles the position and status of `event_list_view_` and other views after
  // the opening event list animation or closing event list animation. Such as
  // restoring the position of them, re-enabling animation and etc.
  void OnOpenEventListAnimationComplete();
  void OnCloseEventListAnimationComplete();

  // Requests the focusing ring to go to the close button of `event_list_view_`.
  void RequestFocusForEventListCloseButton();

  // Animates the month and scrolls back to today and resets the
  // `scrolling_settled_timer_` to update the `on_screen_month_` map after the
  // resetting to today animation.
  void OnResetToTodayAnimationComplete();

  // Enables the month and header animation, restores the header and content
  // opacity.
  void OnResetToTodayFadeInAnimationComplete();

  // Tries to focus the preferred CalendarDateCellView. If `prefer_today` is
  // true, preferred CalendarDateCellView is todays CalendarDateCellView,
  // otherwise preferred view is the selected CalendarDateCellView. If the
  // preferred view is not visible, focus the first visible view.
  void FocusPreferredDateCellViewOrFirstVisible(bool prefer_today);

  // Returns `target_date_cell_view` if it is in the visible window of
  // `scroll_view_` and in `current_month_`. Otherwise returns the first visible
  // focusable date cell on the first fully visible row.
  CalendarDateCellView* GetTargetDateCellViewOrFirstFocusable(
      CalendarDateCellView* target_date_cell_view);

  // Calculates the first fully visible row (which lives in `content_view_`)
  // shown in `scroll_view_`'s visible window.
  int CalculateFirstFullyVisibleRow();

  // Conditionally displays the `up_next_view_`.
  void MaybeShowUpNextView();

  // Removes the `up_next_view_`.
  void RemoveUpNextView();

  // Callback after the animation showing the up next view has ended.
  void OnShowUpNextAnimationEnded();

  // Animates scrolling the Calendar `scroll_view_` by the given offset. Uses
  // layer transforms to mimic scrolling and then sets a final scroll position
  // on the scroll view to give the illusion of animating scrolling.
  void AnimateScrollByOffset(int offset);

  // Post animation callback for `AnimateScrollByOffset()`.
  void OnAnimateScrollByOffsetComplete(int offset);

  // Used by the `CalendarUpNextView` to open the event list for today's date.
  void OpenEventListForTodaysDate();

  enum class ScrollViewState {
    FULL_HEIGHT,
    UP_NEXT_SHOWING,
    EVENT_LIST_SHOWING
  };
  // Used for clipping the calendar scroll view height to the different states
  // that the calendar view can be in.
  void ClipScrollViewHeight(ScrollViewState state_to_change_to);

  // Returns the calculated height of a single visible row.
  int GetSingleVisibleRowHeight();

  // Setters for animation flags.
  void set_should_header_animate(bool should_animate) {
    should_header_animate_ = should_animate;
  }
  void set_should_months_animate(bool should_animate) {
    should_months_animate_ = should_animate;
  }

  std::unique_ptr<CalendarViewController> calendar_view_controller_;

  // Reset `scrolling_settled_timer_`.
  void reset_scrolling_settled_timer() { scrolling_settled_timer_.Reset(); }

  // The content of the `scroll_view_`, which carries months and month labels.
  // Owned by `CalendarView`.
  raw_ptr<ScrollContentsView, ExperimentalAsh> content_view_ = nullptr;

  // The following is owned by `CalendarView`.
  raw_ptr<views::ScrollView, ExperimentalAsh> scroll_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged | ExperimentalAsh> current_label_ =
      nullptr;
  raw_ptr<views::View, DanglingUntriaged | ExperimentalAsh> previous_label_ =
      nullptr;
  raw_ptr<views::View, DanglingUntriaged | ExperimentalAsh> next_label_ =
      nullptr;
  raw_ptr<views::View, DanglingUntriaged | ExperimentalAsh> next_next_label_ =
      nullptr;
  raw_ptr<CalendarMonthView, DanglingUntriaged | ExperimentalAsh>
      previous_month_ = nullptr;
  raw_ptr<CalendarMonthView, DanglingUntriaged | ExperimentalAsh>
      current_month_ = nullptr;
  raw_ptr<CalendarMonthView, DanglingUntriaged | ExperimentalAsh> next_month_ =
      nullptr;
  raw_ptr<CalendarMonthView, DanglingUntriaged | ExperimentalAsh>
      next_next_month_ = nullptr;
  raw_ptr<CalendarHeaderView, ExperimentalAsh> header_ = nullptr;
  // Temporary header, used for animations.
  raw_ptr<CalendarHeaderView, ExperimentalAsh> temp_header_ = nullptr;
  raw_ptr<views::Button, ExperimentalAsh> reset_to_today_button_ = nullptr;
  raw_ptr<views::Button, ExperimentalAsh> settings_button_ = nullptr;
  raw_ptr<IconButton, ExperimentalAsh> managed_button_ = nullptr;
  raw_ptr<IconButton, ExperimentalAsh> up_button_ = nullptr;
  raw_ptr<IconButton, ExperimentalAsh> down_button_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> calendar_sliding_surface_ = nullptr;
  raw_ptr<CalendarEventListView, DanglingUntriaged | ExperimentalAsh>
      event_list_view_ = nullptr;
  // Owned by CalendarView.
  raw_ptr<CalendarUpNextView, DanglingUntriaged | ExperimentalAsh>
      up_next_view_ = nullptr;
  std::map<base::Time, CalendarModel::FetchingStatus> on_screen_month_;
  raw_ptr<CalendarModel, ExperimentalAsh> calendar_model_ =
      Shell::Get()->system_tray_model()->calendar_model();

  // If it `is_resetting_scroll_`, we don't calculate the scroll position and we
  // don't need to check if we need to update the month or not.
  bool is_resetting_scroll_ = false;

  // It's true if the header should animate, but false when it is currently
  // animating, or header changing from mouse scroll (not from the buttons) or
  // cooling down from the last animation.
  bool should_header_animate_ = true;

  // It's true if the month views should animate, but false when it is currently
  // animating, or cooling down from the last animation.
  bool should_months_animate_ = true;

  // This is used to define the animation directions for updating the header and
  // month views.
  bool is_scrolling_up_ = true;

  // Whether the Calendar View is scrolling.
  bool is_calendar_view_scrolling_ = false;

  // If the Calendar View destructor is being called.
  bool is_destroying_ = false;

  // Set to true if the user has scrolled the Calendar at all, either via the
  // scroll view directly or used the month arrow buttons, in the lifetime of
  // the CalendarView.
  bool user_has_scrolled_ = false;

  // Timer that fires when the calendar view is settled on, i.e. finished
  // scrolling to, a currently-visible month
  base::RetainingOneShotTimer scrolling_settled_timer_;

  // Timers that enable the updating month/header animations. When the month
  // keeps getting changed, the animation will be disabled and the cool-down
  // duration is `kAnimationDisablingTimeout` ms to enable the next animation.
  base::RetainingOneShotTimer header_animation_restart_timer_;
  base::RetainingOneShotTimer months_animation_restart_timer_;

  // Timer that checks upcoming events periodically.
  base::RepeatingTimer check_upcoming_events_timer_;

  base::CallbackListSubscription on_contents_scrolled_subscription_;
  base::ScopedObservation<CalendarModel, CalendarModel::Observer>
      scoped_calendar_model_observer_{this};
  base::ScopedObservation<CalendarViewController,
                          CalendarViewController::Observer>
      scoped_calendar_view_controller_observer_{this};
  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      scoped_view_observer_{this};

  base::WeakPtrFactory<CalendarView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_VIEW_H_
