// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_up_next_view.h"

#include <memory>
#include <string>
#include <tuple>

#include "ash/glanceables/glanceables_up_next_event_item_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

namespace ash {

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
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(15, 0)));
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

  if (!up_next_events.empty())
    CreateEventsListView(up_next_events);
  else
    AddNoEventsLabel();
}

void GlanceablesUpNextView::CreateEventsListView(
    const SingleDayEventList& events) {
  auto* events_list_view = AddChildView(std::make_unique<views::View>());
  events_list_view->SetPreferredSize(gfx::Size(300, 150));
  auto* events_list_layout =
      events_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  events_list_layout->set_between_child_spacing(4);

  for (const auto& event : events) {
    event_item_views_.push_back(events_list_view->AddChildView(
        std::make_unique<GlanceablesUpNextEventItemView>(event)));
  }
}

void GlanceablesUpNextView::AddNoEventsLabel() {
  no_events_label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_GLANCEABLES_UP_NEXT_NO_EVENTS)));
  no_events_label_->SetAutoColorReadabilityEnabled(false);
  no_events_label_->SetEnabledColor(SK_ColorWHITE);
  no_events_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
}

BEGIN_METADATA(GlanceablesUpNextView, views::View)
END_METADATA

}  // namespace ash
