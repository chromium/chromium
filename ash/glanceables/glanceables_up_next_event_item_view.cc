// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_up_next_event_item_view.h"

#include <memory>
#include <string>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::google_apis::calendar::CalendarEvent;

std::u16string GetEventTimeLabelText(const CalendarEvent& event) {
  const base::Time& event_start_time = event.start_time().date_time();
  bool use_12_hour_clock =
      Shell::Get()->system_tray_model()->clock()->hour_clock_type() ==
      base::k12HourClock;
  if (use_12_hour_clock)
    return calendar_utils::GetTwelveHourClockTime(event_start_time);
  return calendar_utils::GetTwentyFourHourClockTime(event_start_time);
}

}  // namespace

GlanceablesUpNextEventItemView::GlanceablesUpNextEventItemView(
    CalendarEvent event)
    : event_(event) {
  std::u16string event_title =
      !event_.summary().empty()
          ? base::UTF8ToUTF16(event_.summary())
          : l10n_util::GetStringUTF16(
                IDS_GLANCEABLES_UP_NEXT_EVENT_EMPTY_TITLE);

  SetAccessibleName(event_title);
  SetCallback(base::BindRepeating(&GlanceablesUpNextEventItemView::OpenEvent,
                                  base::Unretained(this)));
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 10));

  event_title_label_ =
      AddChildView(std::make_unique<views::Label>(event_title));
  event_title_label_->SetAutoColorReadabilityEnabled(false);
  event_title_label_->SetEnabledColor(gfx::kGoogleGrey200);
  event_title_label_->SetFontList(gfx::FontList({"Google Sans"},
                                                gfx::Font::FontStyle::NORMAL,
                                                14, gfx::Font::Weight::MEDIUM));
  event_title_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);

  event_time_label_ = AddChildView(
      std::make_unique<views::Label>(GetEventTimeLabelText(event_)));
  event_time_label_->SetAutoColorReadabilityEnabled(false);
  event_time_label_->SetEnabledColor(gfx::kGoogleGrey400);
  event_time_label_->SetFontList(gfx::FontList({"Google Sans"},
                                               gfx::Font::FontStyle::NORMAL, 13,
                                               gfx::Font::Weight::NORMAL));

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
