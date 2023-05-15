// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_VIEW_H_

#include "ash/ash_export.h"
#include "ash/controls/scroll_view_gradient_helper.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/time/calendar_view_controller.h"
#include "base/memory/raw_ptr.h"
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

  // views::View
  void Layout() override;

  // Updates the event list entries.
  void UpdateListItems();

  // Takes a list of `CalendarEvent`'s and generates a parent container
  // containing each `CalendarEvent` as a `CalendarEventListItemViewJelly` view.
  // Returns the parent container.
  std::unique_ptr<views::View> CreateChildEventListView(
      std::list<google_apis::calendar::CalendarEvent> events);

  // Owned by `CalendarView`.
  raw_ptr<CalendarViewController, ExperimentalAsh> calendar_view_controller_;

  // Owned by `CalendarEventListView`.
  const raw_ptr<views::View, ExperimentalAsh> close_button_container_;
  const raw_ptr<views::ScrollView, ExperimentalAsh> scroll_view_;

  // Adds fade in/out gradients to `scroll_view_`.
  std::unique_ptr<ScrollViewGradientHelper> gradient_helper_;

  // The content of the `scroll_view_`, which carries a list of
  // `CalendarEventListItemView`. Owned by `CalendarEventListView`.
  const raw_ptr<views::View, ExperimentalAsh> content_view_;

  // views::View:
  void OnThemeChanged() override;

  base::ScopedObservation<CalendarViewController,
                          CalendarViewController::Observer>
      scoped_calendar_view_controller_observer_{this};

  base::ScopedObservation<CalendarModel, CalendarModel::Observer>
      scoped_calendar_model_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_EVENT_LIST_VIEW_H_
