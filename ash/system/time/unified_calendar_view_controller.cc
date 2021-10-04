// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/unified_calendar_view_controller.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/time/calendar_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "base/i18n/time_formatting.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

UnifiedCalendarViewController::UnifiedCalendarViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)),
      calendar_view_controller_(std::make_unique<CalendarViewController>()),
      tray_controller_(tray_controller) {}

UnifiedCalendarViewController::~UnifiedCalendarViewController() {}

views::View* UnifiedCalendarViewController::CreateView() {
  DCHECK(!view_);
  view_ = new CalendarView(detailed_view_delegate_.get(), tray_controller_,
                           calendar_view_controller_.get());
  return view_;
}

std::u16string UnifiedCalendarViewController::GetAccessibleName() const {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_BUBBLE_ACCESSIBLE_DESCRIPTION,
      base::TimeFormatWithPattern(calendar_view_controller_->current_date(),
                                  "MMMM yyyy"));
}

}  // namespace ash
