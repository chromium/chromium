// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_ITEM_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "url/gurl.h"

namespace ui {
class Event;
}  // namespace ui

namespace gfx {
class RoundedCornersF;
}

namespace ash {

// View ID's.
constexpr int kSummaryLabelID = 100;
constexpr int kTimeLabelID = 101;
constexpr int kEventListItemDotID = 102;
constexpr int kJoinButtonID = 103;
constexpr int kEventListMultiDayEventsContainer = 104;
constexpr int kEventListSameDayEventsContainer = 105;

class CalendarViewController;

struct SelectedDateParams {
  base::Time selected_date;
  base::Time selected_date_midnight;
  base::Time selected_date_midnight_utc;
};

struct UIParams {
  bool round_top_corners = false;
  bool round_bottom_corners = false;
  // If this view is for `CalendarUpNextView`. `CalendarUpNextView` event list
  // item has a different focus ring rounded corner radius.
  bool is_up_next_event_list_item = false;
  // Show the calendar indicator dots which show the event colors. If
  // false this piece of UI is not added to the view hierarchy.
  bool show_event_list_dot = false;
  // Used in `Label::SizeToFit()` to fix the width of this view.  If 0, no
  // fixed width is enforced.
  int fixed_width = 0;
};

// The index of the event in the event list. Used for the accessibility
// description to show "Event n of n".
struct EventListItemIndex {
  int item_index;
  int total_count_of_events;
};

// This view displays a calendar event entry.
class ASH_EXPORT CalendarEventListItemView : public views::Button {
  METADATA_HEADER(CalendarEventListItemView, views::Button)

 public:
  CalendarEventListItemView(CalendarViewController* calendar_view_controller,
                            SelectedDateParams selected_date_params,
                            google_apis::calendar::CalendarEvent event,
                            UIParams ui_params,
                            EventListItemIndex event_list_item_index);
  CalendarEventListItemView(const CalendarEventListItemView& other) = delete;
  CalendarEventListItemView& operator=(const CalendarEventListItemView& other) =
      delete;
  ~CalendarEventListItemView() override;

  // views::View:
  void OnThemeChanged() override;

  void PerformAction(const ui::Event& event);

  // Sets up a custom highlight path for when the
  // `CalendarEventListItemView` view is focused. Conditionally follows the
  // same corner rounding as the view.
  void SetUpFocusHighlight(const gfx::RoundedCornersF& item_corner_radius);

  void OnJoinMeetingButtonPressed(const ui::Event& event);

  bool is_current_or_next_single_day_event() const {
    return is_current_or_next_single_day_event_;
  }

 private:
  friend class CalendarViewEventListViewTest;

  // Unowned.
  const raw_ptr<CalendarViewController, DanglingUntriaged>
      calendar_view_controller_;

  const SelectedDateParams selected_date_params_;

  // The URL for the meeting event.
  const GURL event_url_;

  const GURL video_conference_url_;

  // Whether this item which is not an all-day or multi-day event is the current
  // or next event. Used for auto scroll in the `CalendarEventListView`.
  bool is_current_or_next_single_day_event_ = false;

  // Whether this event has ended by now.
  bool is_past_event_ = false;

  base::WeakPtrFactory<CalendarEventListItemView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_ITEM_VIEW_H_
