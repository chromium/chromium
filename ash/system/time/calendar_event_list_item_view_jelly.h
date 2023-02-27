// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_ITEM_VIEW_JELLY_H_
#define ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_ITEM_VIEW_JELLY_H_

#include "ash/ash_export.h"
#include "ash/system/tray/actionable_view.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "url/gurl.h"

namespace ui {
class Event;
}  // namespace ui

namespace gfx {
class RoundedCornersF;
}

namespace ash {

// Label ID's.
constexpr int kSummaryLabelID = 100;
constexpr int kTimeLabelID = 101;
constexpr int kEventListItemDotID = 102;
constexpr int kJoinButtonID = 103;

class CalendarViewController;

struct SelectedDateParams {
  base::Time selected_date;
  base::Time selected_date_midnight;
  base::Time selected_date_midnight_utc;
};

struct UIParams {
  bool round_top_corners = false;
  bool round_bottom_corners = false;
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

// This view displays a jelly version of a calendar event entry.
class ASH_EXPORT CalendarEventListItemViewJelly : public ActionableView {
 public:
  METADATA_HEADER(CalendarEventListItemViewJelly);

  CalendarEventListItemViewJelly(
      CalendarViewController* calendar_view_controller,
      SelectedDateParams selected_date_params,
      google_apis::calendar::CalendarEvent event,
      UIParams ui_params,
      EventListItemIndex event_list_item_index);
  CalendarEventListItemViewJelly(const CalendarEventListItemViewJelly& other) =
      delete;
  CalendarEventListItemViewJelly& operator=(
      const CalendarEventListItemViewJelly& other) = delete;
  ~CalendarEventListItemViewJelly() override;

  // views::View:
  void OnThemeChanged() override;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  // Sets up a custom highlight path for when the
  // `CalendarEventListItemViewJelly` view is focused. Conditionally follows the
  // same corner rounding as the view.
  void SetUpFocusHighlight(const gfx::RoundedCornersF& item_corner_radius);

  void OnJoinMeetingButtonPressed(const ui::Event& event);

 private:
  friend class CalendarViewEventListViewTest;

  // Unowned.
  CalendarViewController* const calendar_view_controller_;

  const SelectedDateParams selected_date_params_;

  // The URL for the meeting event.
  const GURL event_url_;

  const GURL video_conference_url_;

  base::WeakPtrFactory<CalendarEventListItemViewJelly> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_ITEM_VIEW_JELLY_H_
