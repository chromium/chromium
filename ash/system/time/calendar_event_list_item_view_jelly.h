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

// This view displays a jelly version of a calendar event entry.
class ASH_EXPORT CalendarEventListItemViewJelly : public ActionableView {
 public:
  METADATA_HEADER(CalendarEventListItemViewJelly);

  CalendarEventListItemViewJelly(
      CalendarViewController* calendar_view_controller,
      SelectedDateParams selected_date_params,
      google_apis::calendar::CalendarEvent event,
      const bool round_top_corners,
      const bool round_bottom_corners,
      // Show the calendar indicator dots which show the event colors. If
      // false this piece of UI is not added to the view hierarchy.
      const bool show_event_list_dot,
      // Used in `Label::SizeToFit()` to fix the width of this view.  If 0, no
      // fixed width is enforced.
      const int fixed_width = 0);
  CalendarEventListItemViewJelly(const CalendarEventListItemViewJelly& other) =
      delete;
  CalendarEventListItemViewJelly& operator=(
      const CalendarEventListItemViewJelly& other) = delete;
  ~CalendarEventListItemViewJelly() override;

  // views::View:
  void OnThemeChanged() override;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  void OnJoinMeetingButtonPressed(const ui::Event& event);

 private:
  friend class CalendarViewEventListViewTest;

  // Unowned.
  CalendarViewController* const calendar_view_controller_;

  const SelectedDateParams selected_date_params_;

  // The URL for the meeting event.
  const GURL event_url_;

  const std::string hangout_link_;

  base::WeakPtrFactory<CalendarEventListItemViewJelly> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_ITEM_VIEW_JELLY_H_
