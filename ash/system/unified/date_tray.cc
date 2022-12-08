// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/date_tray.h"

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

DateTray::DateTray(Shelf* shelf, UnifiedSystemTray* tray)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kDateTray,
                         TrayBackgroundView::kStartRounded),
      time_view_(tray_container()->AddChildView(
          std::make_unique<TimeTrayItemView>(shelf, TimeView::Type::kDate))),
      unified_system_tray_(tray) {
  SetID(VIEW_ID_SA_DATE_TRAY);
  SetPressedCallback(
      base::BindRepeating(&DateTray::OnButtonPressed, base::Unretained(this)));

  tray_container()->SetMargin(
      /*main_axis_margin=*/kUnifiedTrayContentPadding -
          ShelfConfig::Get()->status_area_hit_region_padding(),
      /*cross_axis_margin=*/0);
  scoped_unified_system_tray_observer_.Observe(unified_system_tray_);
}

DateTray::~DateTray() = default;

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
  return l10n_util::GetStringFUTF16(
      IDS_ASH_DATE_TRAY_ACCESSIBLE_DESCRIPTION,
      base::TimeFormatFriendlyDate(now),
      base::TimeFormatTimeOfDayWithHourClockType(
          now, Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
          base::kKeepAmPm));
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

void DateTray::OnButtonPressed(const ui::Event& event) {
  // Lets the `unified_system_tray_` decide whether to show the bubble or not,
  // since it's the owner of the bubble view.
  if (is_active()) {
    unified_system_tray_->CloseBubble();
    return;
  }

  // Need to set the date tray as active before notifying the system tray of
  // an action because we need the system tray to know that the date tray is
  // already active when it is creating the `UnifiedSystemTrayBubble`.
  SetIsActive(true);
  unified_system_tray_->OnDateTrayActionPerformed(event);
}

BEGIN_METADATA(DateTray, ActionableView)
END_METADATA

}  // namespace ash
