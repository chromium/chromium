// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_up_next_view.h"

#include <memory>
#include <string>
#include <tuple>

#include "ash/shell.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using ::google_apis::calendar::CalendarEvent;
using ::google_apis::calendar::EventList;

std::u16string GetFormattedEventTimeInterval(const CalendarEvent& event) {
  const base::Time& event_start_time = event.start_time().date_time();
  const base::Time& event_end_time = event.end_time().date_time();
  bool use_12_hour_clock =
      Shell::Get()->system_tray_model()->clock()->hour_clock_type() ==
      base::k12HourClock;
  if (use_12_hour_clock) {
    return calendar_utils::FormatTwelveHourClockTimeInterval(event_start_time,
                                                             event_end_time);
  }
  return calendar_utils::FormatTwentyFourHourClockTimeInterval(event_start_time,
                                                               event_end_time);
}

}  // namespace

// TODO(crbug.com/1353495): file-level todo list:
// - update existing `CalendarModel` and `CalendarEventFetch` to support 1-day
//   fetches or consider implementing own simplified model (keep calendar-view
//   team in the loop);
// - add "Loading" / "Nothing for today" UI states;
// - refetch events at 00:00 and decide how to pull new events for current day;
// - correctly display multi-day events (limit to 00:00 and/or 23:59);
// - limit events list height and switch to `views::ScrollView`;
// - remove events from the list on their end times;
// - move fonts/colors/sizes to a config.

GlanceablesUpNextView::GlanceablesUpNextView() {
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(15, 0)));
  SetLayoutManager(std::make_unique<views::BoxLayout>());
  calendar_model_ = Shell::Get()->system_tray_model()->calendar_model();
  calendar_model_->AddObserver(this);
}

GlanceablesUpNextView::~GlanceablesUpNextView() {
  calendar_model_->RemoveObserver(this);
}

void GlanceablesUpNextView::OnEventsFetched(
    const CalendarModel::FetchingStatus status,
    const base::Time start_time,
    const EventList* fetched_events) {
  calendar_model_->RemoveObserver(this);

  const base::Time now = base::Time::Now();
  const base::Time midnight =
      (now + calendar_utils::GetTimeDifference(now)).UTCMidnight();
  const SingleDayEventList& all_todays_events =
      calendar_model_->FindEvents(midnight);

  SingleDayEventList up_next_events;
  for (const auto& event : all_todays_events) {
    if (event.start_time().date_time() >= now ||
        event.end_time().date_time() >= now) {
      up_next_events.push_back(event);
    }
  }

  CreateEventsListView(up_next_events);
}

void GlanceablesUpNextView::CreateEventsListItemView(
    const CalendarEvent& event) {
  auto* item = events_list_view_->AddChildView(std::make_unique<views::View>());
  auto* layout = item->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 10));

  auto* event_title_label = item->AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(event.summary())));
  event_title_label->SetAutoColorReadabilityEnabled(false);
  event_title_label->SetEnabledColor(SK_ColorWHITE);
  event_title_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);

  auto* event_time_label = item->AddChildView(
      std::make_unique<views::Label>(GetFormattedEventTimeInterval(event)));
  event_time_label->SetAutoColorReadabilityEnabled(false);
  event_time_label->SetEnabledColor(SK_ColorWHITE);

  events_list_items_views_.emplace_back(event_title_label, event_time_label);

  layout->SetFlexForView(event_title_label, 1);
  layout->SetFlexForView(event_time_label, 0, true);
}

void GlanceablesUpNextView::CreateEventsListView(
    const SingleDayEventList& events) {
  events_list_view_ = AddChildView(std::make_unique<views::View>());
  events_list_view_->SetPreferredSize(gfx::Size(300, 150));
  auto* events_list_layout =
      events_list_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  events_list_layout->set_between_child_spacing(4);

  for (const auto& event : events)
    CreateEventsListItemView(event);
}

BEGIN_METADATA(GlanceablesUpNextView, views::View)
END_METADATA

}  // namespace ash
