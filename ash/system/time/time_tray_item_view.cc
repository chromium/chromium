// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_tray_item_view.h"

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/time_view.h"

namespace ash {

namespace tray {

TimeTrayItemView::TimeTrayItemView(Shelf* shelf)
    : TrayItemView(shelf), session_observer_(this) {
  tray::TimeView::ClockLayout clock_layout =
      shelf->IsHorizontalAlignment()
          ? tray::TimeView::ClockLayout::HORIZONTAL_CLOCK
          : tray::TimeView::ClockLayout::VERTICAL_CLOCK;
  time_view_ = new tray::TimeView(clock_layout,
                                  Shell::Get()->system_tray_model()->clock());
  AddChildView(time_view_);
}

TimeTrayItemView::~TimeTrayItemView() = default;

void TimeTrayItemView::UpdateAlignmentForShelf(Shelf* shelf) {
  tray::TimeView::ClockLayout clock_layout =
      shelf->IsHorizontalAlignment()
          ? tray::TimeView::ClockLayout::HORIZONTAL_CLOCK
          : tray::TimeView::ClockLayout::VERTICAL_CLOCK;
  time_view_->UpdateClockLayout(clock_layout);
}

void TimeTrayItemView::OnSessionStateChanged(
    session_manager::SessionState state) {
  time_view_->SetTextColorBasedOnSession(state);
}

const char* TimeTrayItemView::GetClassName() const {
  return "TimeTrayItemView";
}

}  // namespace tray
}  // namespace ash
