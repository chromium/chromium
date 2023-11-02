// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_up_next_view.h"

#include <memory>
#include <string>

#include "ash/glanceables/glanceables_up_next_event_item_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using ::google_apis::calendar::CalendarEvent;

bool IsAllDayEvent(const CalendarEvent& event) {
  const auto exploded_start_time =
      calendar_utils::GetExplodedUTC(event.start_time().date_time());
  const auto exploded_end_time =
      calendar_utils::GetExplodedUTC(event.end_time().date_time());

  if (event.start_time().date_time() < event.end_time().date_time() &&
      exploded_start_time.hour == exploded_end_time.hour &&
      exploded_start_time.minute == exploded_end_time.minute &&
      exploded_start_time.second == exploded_end_time.second &&
      exploded_start_time.millisecond == exploded_end_time.millisecond) {
    return true;
  }
  return false;
}

bool ShouldShowEvent(const CalendarEvent& event) {
  const auto now = base::Time::Now();
  const auto& start_time = event.start_time().date_time();
  const auto& end_time = event.end_time().date_time();

  // Skip finished events.
  if (end_time < now)
    return false;

  // Skip ongoing events 1.5 hours after the start time.
  if (start_time <= now && now - start_time >= base::Hours(1.5))
    return false;

  // Skip all-day events.
  if (IsAllDayEvent(event))
    return false;

  return true;
}

bool EventComparator(const CalendarEvent& a, const CalendarEvent& b) {
  const auto& a_start_time = a.start_time().date_time();
  const auto& a_end_time = a.end_time().date_time();
  const auto& b_start_time = b.start_time().date_time();
  const auto& b_end_time = b.end_time().date_time();

  // If `a` and `b` have the same start and end time, then they are ordered
  // alphabetically.
  if (a_start_time == b_start_time && a_end_time == b_end_time)
    return a.summary() < b.summary();

  // If `a` and `b` have the same start time, then they are ordered based on
  // duration (longer events ordered first).
  if (a_start_time == b_start_time)
    return a_end_time - a_start_time > b_end_time - b_start_time;

  // Default ordering by start time.
  return a_start_time < b_start_time;
}

}  // namespace

// TODO(crbug.com/1353495): file-level todo list:
// - update existing `CalendarModel` and `CalendarEventFetch` to support 1-day
//   fetches or consider implementing own simplified model (keep calendar-view
//   team in the loop);
// - add "Loading" UI state;
// - refetch events at 00:00 and decide how to pull new events for current day;
// - correctly display multi-day events (limit to 00:00 and/or 23:59);
// - limit events list height and switch to `views::ScrollView`;
// - remove events from the list on their end times;
// - move fonts/colors/sizes to a config.

GlanceablesUpNextView::GlanceablesUpNextView() {
  SetLayoutManager(std::make_unique<views::FlexLayout>());

  calendar_model_ = Shell::Get()->system_tray_model()->calendar_model();
  calendar_model_->AddObserver(this);
}

GlanceablesUpNextView::~GlanceablesUpNextView() {
  calendar_model_->RemoveObserver(this);
}

void GlanceablesUpNextView::OnEventsFetched(
    const CalendarModel::FetchingStatus status,
    const base::Time start_time,
    const google_apis::calendar::EventList* fetched_events) {
  calendar_model_->RemoveObserver(this);

  const SingleDayEventList up_next_events = GetUpNextEvents();
  if (!up_next_events.empty())
    CreateEventsListView(up_next_events);
  else
    AddNoEventsLabel();
}

SingleDayEventList GlanceablesUpNextView::GetUpNextEvents() {
  const base::Time now = base::Time::Now();
  const base::Time midnight =
      (now + calendar_utils::GetTimeDifference(now)).UTCMidnight();
  const SingleDayEventList& all_todays_events =
      calendar_model_->FindEvents(midnight);

  SingleDayEventList up_next_events;
  for (const auto& event : all_todays_events) {
    if (ShouldShowEvent(event))
      up_next_events.push_back(event);
  }
  up_next_events.sort(&EventComparator);
  return up_next_events;
}

void GlanceablesUpNextView::CreateEventsListView(
    const SingleDayEventList& events) {
  auto* events_list_view = AddChildView(std::make_unique<views::View>());
  events_list_view->SetPreferredSize(gfx::Size(300, 150));
  auto* events_list_layout =
      events_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  events_list_layout->set_between_child_spacing(12);

  for (const auto& event : events) {
    event_item_views_.push_back(events_list_view->AddChildView(
        std::make_unique<GlanceablesUpNextEventItemView>(event)));
  }
}

void GlanceablesUpNextView::AddNoEventsLabel() {
  no_events_label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_GLANCEABLES_UP_NEXT_NO_EVENTS)));
  no_events_label_->SetAutoColorReadabilityEnabled(false);
  no_events_label_->SetEnabledColor(gfx::kGoogleGrey200);
  no_events_label_->SetFontList(gfx::FontList({"Google Sans"},
                                              gfx::Font::FontStyle::NORMAL, 14,
                                              gfx::Font::Weight::MEDIUM));
  no_events_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
}

BEGIN_METADATA(GlanceablesUpNextView, views::View)
END_METADATA

}  // namespace ash
