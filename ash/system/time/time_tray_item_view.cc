// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_tray_item_view.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/time_view.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/views/border.h"

namespace ash {

TimeTrayItemView::TimeTrayItemView(Shelf* shelf, TimeView::Type type)
    : TrayItemView(shelf), session_observer_(this) {
  TimeView::ClockLayout clock_layout =
      shelf->IsHorizontalAlignment() ? TimeView::ClockLayout::HORIZONTAL_CLOCK
                                     : TimeView::ClockLayout::VERTICAL_CLOCK;
  time_view_ = AddChildView(std::make_unique<TimeView>(
      clock_layout, Shell::Get()->system_tray_model()->clock(), type));
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

const char* TimeTrayItemView::GetClassName() const {
  return "TimeTrayItemView";
}

void TimeTrayItemView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  time_view_->SetTextColor(
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState()));
}

}  // namespace ash
