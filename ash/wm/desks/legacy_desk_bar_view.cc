// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/legacy_desk_bar_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

// -----------------------------------------------------------------------------
// LegacyDeskBarView:

LegacyDeskBarView::LegacyDeskBarView(base::WeakPtr<OverviewGrid> overview_grid)
    : DeskBarViewBase(overview_grid->root_window(),
                      DeskBarViewBase::Type::kOverview) {
  overview_grid_ = overview_grid;
}

gfx::Size LegacyDeskBarView::CalculatePreferredSize() const {
  // For overview bar, it always come with the fixed width (the full available
  // width).
  return {GetAvailableBounds().width(),
          GetPreferredBarHeight(root_, type_, state_)};
}

gfx::Rect LegacyDeskBarView::GetAvailableBounds() const {
  // Information is retrieved from the widget as it comes with the full
  // available bounds at initialization time and remains unchanged.
  return GetWidget()->GetRootView()->bounds();
}

BEGIN_METADATA(LegacyDeskBarView, DeskBarViewBase)
END_METADATA

}  // namespace ash
