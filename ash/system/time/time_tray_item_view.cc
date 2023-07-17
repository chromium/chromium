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
#include "chromeos/constants/chromeos_features.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace ash {

TimeTrayItemView::TimeTrayItemView(Shelf* shelf, TimeView::Type type)
    : TrayItemView(shelf) {
  TimeView::ClockLayout clock_layout =
      shelf->IsHorizontalAlignment() ? TimeView::ClockLayout::HORIZONTAL_CLOCK
                                     : TimeView::ClockLayout::VERTICAL_CLOCK;
  time_view_ = AddChildView(std::make_unique<TimeView>(
      clock_layout, Shell::Get()->system_tray_model()->clock(), type));
  time_view_->SetTextColorId(kColorAshIconColorPrimary);
  UpdateLabelOrImageViewColor(/*active=*/false);
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

void TimeTrayItemView::UpdateLabelOrImageViewColor(bool active) {
  if (!chromeos::features::IsJellyEnabled()) {
    return;
  }
  TrayItemView::UpdateLabelOrImageViewColor(active);

  const auto color_id = active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                               : cros_tokens::kCrosSysOnSurface;
  time_view_->SetTextColorId(color_id);
  time_view_->SetDateViewColorId(color_id);
}

const char* TimeTrayItemView::GetClassName() const {
  return "TimeTrayItemView";
}

}  // namespace ash
