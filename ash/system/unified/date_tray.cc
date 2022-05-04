// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/date_tray.h"

#include "ash/shell.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

DateTray::DateTray(Shelf* shelf, UnifiedSystemTray* tray)
    : TrayBackgroundView(shelf, TrayBackgroundView::kStartRounded),
      time_view_(tray_container()->AddChildView(
          std::make_unique<TimeTrayItemView>(shelf, TimeView::Type::kDate))),
      unified_system_tray_(tray) {
  tray_container()->SetMargin(
      /*main_axis_margin=*/kUnifiedTrayContentPadding -
          ShelfConfig::Get()->status_area_hit_region_padding(),
      /*cross_axis_margin=*/0);
  scoped_unified_system_tray_observer_.Observe(unified_system_tray_);
}

DateTray::~DateTray() = default;

bool DateTray::PerformAction(const ui::Event& event) {
  // Lets the `unified_system_tray_` decide whether to show the bubble or not,
  // since it's the owner of the bubble view.
  if (is_active()) {
    unified_system_tray_->CloseBubble();
  } else {
    unified_system_tray_->OnDateTrayActionPerformed(event);
  }

  return true;
}

std::u16string DateTray::GetAccessibleNameForBubble() {
  if (unified_system_tray_->IsBubbleShown())
    return unified_system_tray_->GetAccessibleNameForQuickSettingsBubble();

  return GetAccessibleNameForTray();
}

void DateTray::HandleLocaleChange() {
  time_view_->HandleLocaleChange();
}

std::u16string DateTray::GetAccessibleNameForTray() {
  base::Time now = base::Time::Now();
  return base::TimeFormatTimeOfDayWithHourClockType(
             now, Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
             base::kKeepAmPm) +
         u", " + base::TimeFormatFriendlyDate(now);
}

void DateTray::UpdateLayout() {
  TrayBackgroundView::UpdateLayout();
  time_view_->UpdateAlignmentForShelf(shelf());
}

void DateTray::UpdateAfterLoginStatusChange() {
  SetVisiblePreferred(true);
}

void DateTray::OnOpeningCalendarView() {
  SetIsActive(true);
}

void DateTray::OnLeavingCalendarView() {
  SetIsActive(false);
}

BEGIN_METADATA(DateTray, ActionableView)
END_METADATA

}  // namespace ash
