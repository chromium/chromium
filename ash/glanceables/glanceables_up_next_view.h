// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_UP_NEXT_VIEW_H_
#define ASH_GLANCEABLES_GLANCEABLES_UP_NEXT_VIEW_H_

#include <tuple>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/time/calendar_model.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace google_apis::calendar {
class EventList;
}  // namespace google_apis::calendar

namespace views {
class Label;
}  // namespace views

namespace ash {

class CalendarModel;

// "Up next" section with today's calendar events.
class ASH_EXPORT GlanceablesUpNextView : public views::View,
                                         public CalendarModel::Observer {
 public:
  METADATA_HEADER(GlanceablesUpNextView);

  GlanceablesUpNextView();
  GlanceablesUpNextView(const GlanceablesUpNextView&) = delete;
  GlanceablesUpNextView& operator=(const GlanceablesUpNextView&) = delete;
  ~GlanceablesUpNextView() override;

  // CalendarModel::Observer:
  void OnEventsFetched(
      const CalendarModel::FetchingStatus status,
      const base::Time start_time,
      const google_apis::calendar::EventList* fetched_events) override;

  std::vector<std::tuple<views::Label*, views::Label*>>
  events_list_items_views_for_test() {
    return events_list_items_views_;
  }

 private:
  void CreateEventsListItemView(
      const google_apis::calendar::CalendarEvent& event);
  void CreateEventsListView(const SingleDayEventList& events);

  CalendarModel* calendar_model_ = nullptr;
  views::View* events_list_view_ = nullptr;
  std::vector<std::tuple<views::Label*, views::Label*>>
      events_list_items_views_;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_UP_NEXT_VIEW_H_
