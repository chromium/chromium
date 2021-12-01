// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_tray_item_view.h"

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/time_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/memory/scoped_refptr.h"
#include "ui/views/border.h"

namespace ash {

namespace tray {

TimeTrayItemView::TimeTrayItemView(
    Shelf* shelf,
    scoped_refptr<UnifiedSystemTrayModel> model,
    absl::optional<TimeView::OnTimeViewActionPerformedCallback> callback)
    : TrayItemView(shelf), model_(model), session_observer_(this) {
  system_tray_model_observation_.Observe(model_.get());

  TimeView::ClockLayout clock_layout =
      shelf->IsHorizontalAlignment() ? TimeView::ClockLayout::HORIZONTAL_CLOCK
                                     : TimeView::ClockLayout::VERTICAL_CLOCK;
  time_view_ = new TimeView(
      clock_layout, Shell::Get()->system_tray_model()->clock(), callback);

  AddChildView(time_view_);

  OnSystemTrayButtonSizeChanged(model_->GetSystemTrayButtonSize());
}

TimeTrayItemView::~TimeTrayItemView() = default;

void TimeTrayItemView::UpdateAlignmentForShelf(Shelf* shelf) {
  TimeView::ClockLayout clock_layout =
      shelf->IsHorizontalAlignment() ? TimeView::ClockLayout::HORIZONTAL_CLOCK
                                     : TimeView::ClockLayout::VERTICAL_CLOCK;
  time_view_->UpdateClockLayout(clock_layout);
}

void TimeTrayItemView::HandleLocaleChange() {
  time_view_->Refresh();
}

void TimeTrayItemView::OnSessionStateChanged(
    session_manager::SessionState state) {
  time_view_->SetTextColor(TrayIconColor(state));
}

void TimeTrayItemView::OnSystemTrayButtonSizeChanged(
    UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size) {
  time_view_->SetShowDate(
      features::IsCalendarViewEnabled() &&
      system_tray_size == UnifiedSystemTrayModel::SystemTrayButtonSize::kLarge);
}

void TimeTrayItemView::Reset() {
  system_tray_model_observation_.Reset();
}

const char* TimeTrayItemView::GetClassName() const {
  return "TimeTrayItemView";
}

void TimeTrayItemView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  time_view_->SetTextColor(
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState()));
}

}  // namespace tray
}  // namespace ash
