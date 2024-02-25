// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UP_NEXT_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_UP_NEXT_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/time/calendar_view_controller.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

// This view displays a scrollable list of `CalendarEventListItemView` for the
// events that a user has coming up, either imminently or that are already in
// progress but not yet finished.
class ASH_EXPORT CalendarUpNextView : public views::View {
  METADATA_HEADER(CalendarUpNextView, views::View)

 public:
  CalendarUpNextView(CalendarViewController* calendar_view_controller,
                     views::Button::PressedCallback callback);
  CalendarUpNextView(const CalendarUpNextView& other) = delete;
  CalendarUpNextView& operator=(const CalendarUpNextView& other) = delete;
  ~CalendarUpNextView() override;

  // Called by a timer in the calendar whilst the up next view is open to
  // refresh any ongoing events.
  void RefreshEvents();

  // Returns the `SkPath` for the background of the `CalendarUpNextView`.
  SkPath GetClipPath() const;

  // views::View
  void Layout(PassKey) override;

 private:
  friend class CalendarUpNextViewAnimationTest;
  friend class CalendarUpNextViewPixelTest;
  friend class CalendarUpNextViewTest;
  friend class CalendarViewTest;

  // Populates the scroll view with events.
  void UpdateEvents(
      const std::list<google_apis::calendar::CalendarEvent>& events);

  // Callbacks for scroll buttons.
  void OnScrollLeftButtonPressed(const ui::Event& event);
  void OnScrollRightButtonPressed(const ui::Event& event);

  // Toggles enabled / disabled states of the scroll buttons.
  void ToggleScrollButtonState();

  // Scrolls the scroll view by the given offset.
  void ScrollViewByOffset(int offset);

  // Takes two coordinates and animates the `content_view_` to move between
  // them. Gives the effect of animating the horizontal `scroll_view_` smoothly
  // moving upon the `left_scroll_button_` and `right_scroll_button_` presses.
  void AnimateScrollToShowXCoordinate(const int start_edge,
                                      const int target_edge);

  // Owned by `CalendarView`.
  raw_ptr<CalendarViewController, DanglingUntriaged> calendar_view_controller_;

  // Owned by `CalendarUpNextView`.
  const raw_ptr<views::View> todays_events_button_container_;
  const raw_ptr<views::View> header_view_;
  raw_ptr<views::Button> left_scroll_button_;
  raw_ptr<views::Button> right_scroll_button_;
  const raw_ptr<views::ScrollView> scroll_view_;

  // The current events displayed in calendar up next. Serves as a cache to diff
  // against when refreshing events.
  SingleDayEventList displayed_events_;

  // The content of the horizontal `scroll_view`, which carries a list of
  // `CalendarEventListItemView`.
  const raw_ptr<views::View> content_view_;

  // Helper class for animating the `scroll_view_` when a scroll button is
  // pressed.
  std::unique_ptr<gfx::LinearAnimation> scrolling_animation_;

  // Bounds animator used in the `scrolling_animation_` class.
  views::BoundsAnimator bounds_animator_;

  // Callback subscriptions.
  base::CallbackListSubscription on_contents_scrolled_subscription_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_UP_NEXT_VIEW_H_
