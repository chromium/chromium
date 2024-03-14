// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/unified_calendar_view_controller.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

UnifiedCalendarViewController::~UnifiedCalendarViewController() = default;

std::unique_ptr<views::View> UnifiedCalendarViewController::CreateView() {
  DCHECK(!view_);
  const base::Time start_time = base::Time::Now();
  auto view =
      std::make_unique<CalendarView>(/*use_glanceables_container_style=*/false);
  base::UmaHistogramTimes("Ash.CalendarView.ConstructionTime",
                          base::Time::Now() - start_time);
  view_ = view.get();
  return view;
}

std::u16string UnifiedCalendarViewController::GetAccessibleName() const {
  // Shows `Now()` as the initial time if calendar view is not created yet.
  base::Time current_time =
      view_ ? view_->calendar_view_controller()->currently_shown_date()
            : base::Time::Now();
  return l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_BUBBLE_ACCESSIBLE_DESCRIPTION,
      calendar_utils::GetMonthDayYearWeek(current_time));
}

}  // namespace ash
