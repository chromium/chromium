// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_UP_NEXT_VIEW_H_
#define ASH_GLANCEABLES_GLANCEABLES_UP_NEXT_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/system/time/calendar_model.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace base {
class Time;
}  // namespace base

namespace google_apis::calendar {
class EventList;
}  // namespace google_apis::calendar

namespace views {
class Label;
}  // namespace views

namespace ash {

class CalendarModel;
class GlanceablesUpNextEventItemView;

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

 private:
  friend class GlanceablesTest;

  SingleDayEventList GetUpNextEvents();
  void CreateEventsListView(const SingleDayEventList& events);
  void AddNoEventsLabel();

  raw_ptr<CalendarModel, ExperimentalAsh> calendar_model_ = nullptr;
  std::vector<GlanceablesUpNextEventItemView*> event_item_views_;
  raw_ptr<views::Label, ExperimentalAsh> no_events_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_UP_NEXT_VIEW_H_
