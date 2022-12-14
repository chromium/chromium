// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_tray_item_view.h"

#include <memory>

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/time_view.h"

namespace ash {

TimeTrayItemView::TimeTrayItemView(Shelf* shelf, TimeView::Type type)
    : TrayItemView(shelf) {
  TimeView::ClockLayout clock_layout =
      shelf->IsHorizontalAlignment() ? TimeView::ClockLayout::HORIZONTAL_CLOCK
                                     : TimeView::ClockLayout::VERTICAL_CLOCK;
  time_view_ = AddChildView(std::make_unique<TimeView>(
      clock_layout, Shell::Get()->system_tray_model()->clock(), type));
  time_view_->SetTextColorId(kColorAshIconColorPrimary);
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

const char* TimeTrayItemView::GetClassName() const {
  return "TimeTrayItemView";
}

}  // namespace ash
