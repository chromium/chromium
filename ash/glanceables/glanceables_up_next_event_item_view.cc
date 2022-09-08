// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_up_next_event_item_view.h"

#include <memory>
#include <string>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::google_apis::calendar::CalendarEvent;

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

GlanceablesUpNextEventItemView::GlanceablesUpNextEventItemView(
    CalendarEvent event)
    : event_(event) {
  SetAccessibleName(base::UTF8ToUTF16(event_.summary()));
  SetCallback(base::BindRepeating(&GlanceablesUpNextEventItemView::OpenEvent,
                                  base::Unretained(this)));
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 10));

  event_title_label_ = AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(event_.summary())));
  event_title_label_->SetAutoColorReadabilityEnabled(false);
  event_title_label_->SetEnabledColor(SK_ColorWHITE);
  event_title_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);

  event_time_label_ = AddChildView(
      std::make_unique<views::Label>(GetFormattedEventTimeInterval(event_)));
  event_time_label_->SetAutoColorReadabilityEnabled(false);
  event_time_label_->SetEnabledColor(SK_ColorWHITE);

  layout->SetFlexForView(event_title_label_, 1);
  layout->SetFlexForView(event_time_label_, 0, true);
}

void GlanceablesUpNextEventItemView::OpenEvent() const {
  bool opened_pwa = false;
  GURL finalized_event_url;
  Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
      GURL(event_.html_link()), base::Time::Now(), opened_pwa,
      finalized_event_url);
}

}  // namespace ash
