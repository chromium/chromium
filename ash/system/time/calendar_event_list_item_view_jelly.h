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

namespace views {
class Label;
}  // namespace views

namespace ash {

class CalendarViewController;

// This view displays a jelly version of a calendar event entry.
class ASH_EXPORT CalendarEventListItemViewJelly : public ActionableView {
 public:
  METADATA_HEADER(CalendarEventListItemViewJelly);

  CalendarEventListItemViewJelly(
      CalendarViewController* calendar_view_controller,
      google_apis::calendar::CalendarEvent event);
  CalendarEventListItemViewJelly(const CalendarEventListItemViewJelly& other) =
      delete;
  CalendarEventListItemViewJelly& operator=(
      const CalendarEventListItemViewJelly& other) = delete;
  ~CalendarEventListItemViewJelly() override;

  // views::View:
  void OnThemeChanged() override;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

 private:
  friend class CalendarViewEventListViewTest;

  // Unowned.
  CalendarViewController* const calendar_view_controller_;

  // Owned by the views hierarchy.
  // The summary (title) of the meeting event.
  views::Label* const summary_;

  // Owned by the views hierarchy.
  // The start time and end time of a meeting event.
  views::Label* const time_range_;

  // The URL for the meeting event.
  const GURL event_url_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_ITEM_VIEW_JELLY_H_
