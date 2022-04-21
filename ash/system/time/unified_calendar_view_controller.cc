// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/unified_calendar_view_controller.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/time/calendar_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

UnifiedCalendarViewController::UnifiedCalendarViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)),
      tray_controller_(tray_controller) {}

UnifiedCalendarViewController::~UnifiedCalendarViewController() = default;

views::View* UnifiedCalendarViewController::CreateView() {
  DCHECK(!view_);
  const base::Time start_time = base::Time::Now();
  view_ = new CalendarView(detailed_view_delegate_.get(), tray_controller_);
  base::UmaHistogramTimes("Ash.CalendarView.ConstructionTime",
                          base::Time::Now() - start_time);
  return view_;
}

std::u16string UnifiedCalendarViewController::GetAccessibleName() const {
  // Shows `Now()` as the initial time if calendar view is not created yet.
  base::Time current_time =
      view_ ? view_->calendar_view_controller()->currently_shown_date()
            : base::Time::Now();
  return l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_BUBBLE_ACCESSIBLE_DESCRIPTION,
      base::TimeFormatWithPattern(current_time, "MMMM yyyy"));
}

}  // namespace ash
