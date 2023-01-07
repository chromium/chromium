// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/time/calendar_view_controller.h"
#include "base/scoped_observation.h"
#include "ui/views/view.h"

namespace ash {

// This view displays a scrollable list of `CalendarEventListItemView`.
class ASH_EXPORT CalendarEventListView
    : public CalendarModel::Observer,
      public CalendarViewController::Observer,
      public views::View {
 public:
  METADATA_HEADER(CalendarEventListView);

  explicit CalendarEventListView(
      CalendarViewController* calendar_view_controller);
  CalendarEventListView(const CalendarEventListView& other) = delete;
  CalendarEventListView& operator=(const CalendarEventListView& other) = delete;
  ~CalendarEventListView() override;

 private:
  friend class CalendarViewEventListViewTest;
  friend class CalendarViewTest;

  // CalendarViewController::Observer:
  void OnSelectedDateUpdated() override;

  // CalendarModel::Observer:
  void OnEventsFetched(const CalendarModel::FetchingStatus status,
                       const base::Time start_time,
                       const google_apis::calendar::EventList* events) override;

  // Updates the event list entries.
  void UpdateListItems();

  // Owned by `CalendarView`.
  CalendarViewController* calendar_view_controller_;

  // Owned by `CalendarEventListView`.
  views::View* const close_button_container_;
  views::ScrollView* const scroll_view_;

  // The content of the `scroll_view_`, which carries a list of
  // `CalendarEventListItemView`. Owned by `CalendarEventListView`.
  views::View* const content_view_;

  base::ScopedObservation<CalendarViewController,
                          CalendarViewController::Observer>
      scoped_calendar_view_controller_observer_{this};

  base::ScopedObservation<CalendarModel, CalendarModel::Observer>
      scoped_calendar_model_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_VIEW_H_
