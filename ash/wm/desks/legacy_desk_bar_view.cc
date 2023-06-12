// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/legacy_desk_bar_view.h"

#include "ui/aura/window.h"

namespace ash {

// -----------------------------------------------------------------------------
// LegacyDeskBarView:

LegacyDeskBarView::LegacyDeskBarView(OverviewGrid* overview_grid)
    : DeskBarViewBase(overview_grid->root_window(),
                      DeskBarViewBase::Type::kOverview) {
  overview_grid_ = overview_grid;
}

const char* LegacyDeskBarView::GetClassName() const {
  return "LegacyDeskBarView";
}

gfx::Size LegacyDeskBarView::CalculatePreferredSize() const {
  // For overview bar, it always come with the fixed width (the full available
  // width).
  return {GetAvailableBounds().width(),
          GetPreferredBarHeight(root_, type_, state_)};
}

}  // namespace ash
