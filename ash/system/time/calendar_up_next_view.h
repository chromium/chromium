// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UP_NEXT_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_UP_NEXT_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
}

namespace ash {

// This view displays a scrollable list of `CalendarEventListItemView` for the
// events that a user has coming up, either imminently or that are already in
// progress but not yet finished.
class ASH_EXPORT CalendarUpNextView : public views::View {
 public:
  METADATA_HEADER(CalendarUpNextView);

  explicit CalendarUpNextView(CalendarViewController* calendar_view_controller);
  CalendarUpNextView(const CalendarUpNextView& other) = delete;
  CalendarUpNextView& operator=(const CalendarUpNextView& other) = delete;
  ~CalendarUpNextView() override;

  // views::View
  void Layout() override;
  void OnThemeChanged() override;

 private:
  friend class CalendarUpNextViewTest;
  friend class CalendarUpNextViewAnimationTest;
  friend class CalendarUpNextViewPixelTest;

  // Populates the scroll view with events.
  void UpdateEvents(
      const std::list<google_apis::calendar::CalendarEvent>& events,
      views::BoxLayout* content_layout_manager);

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
  CalendarViewController* calendar_view_controller_;

  // Owned by `CalendarUpNextView`.
  views::View* const header_view_;
  views::Button* left_scroll_button_;
  views::Button* right_scroll_button_;
  views::ScrollView* const scroll_view_;

  // The content of the horizontal `scroll_view`, which carries a list of
  // `CalendarEventListItemView`.
  views::View* const content_view_;

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
