// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_ITEM_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/actionable_view.h"
#include "base/memory/raw_ptr.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "url/gurl.h"

namespace ui {

class Event;

}  // namespace ui

namespace views {

class Label;

}  // namespace views

namespace ash {

class CalendarViewController;

// This view displays a calendar event entry.
class ASH_EXPORT CalendarEventListItemView : public ActionableView {
 public:
  METADATA_HEADER(CalendarEventListItemView);

  CalendarEventListItemView(CalendarViewController* calendar_view_controller,
                            google_apis::calendar::CalendarEvent event);
  CalendarEventListItemView(const CalendarEventListItemView& other) = delete;
  CalendarEventListItemView& operator=(const CalendarEventListItemView& other) =
      delete;
  ~CalendarEventListItemView() override;

  // views::View:
  void OnThemeChanged() override;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

 private:
  friend class CalendarViewEventListViewTest;

  // Unowned.
  const raw_ptr<CalendarViewController, ExperimentalAsh>
      calendar_view_controller_;

  // The summary (title) of the meeting event.
  const raw_ptr<views::Label, ExperimentalAsh> summary_;

  // The start time and end time of a meeting event.
  const raw_ptr<views::Label, ExperimentalAsh> time_range_;

  // The URL for the meeting event.
  const GURL event_url_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_ITEM_VIEW_H_
