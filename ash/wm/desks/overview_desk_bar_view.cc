// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/overview_desk_bar_view.h"

#include "ash/ash_element_identifiers.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace ash {

OverviewDeskBarView::OverviewDeskBarView(
    base::WeakPtr<OverviewGrid> overview_grid,
    base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator,
    const gfx::Rect& initial_widget_bounds)
    : DeskBarViewBase(overview_grid->root_window(),
                      DeskBarViewBase::Type::kOverview,
                      window_occlusion_calculator),
      initial_widget_bounds_(initial_widget_bounds) {
  SetProperty(views::kElementIdentifierKey, kOverviewDeskBarElementId);
  overview_grid_ = overview_grid;
}

gfx::Size OverviewDeskBarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // For overview bar, it always come with the fixed width (the full available
  // width).
  return {GetAvailableBounds().width(),
          GetPreferredBarHeight(root_, type_, state_)};
}

gfx::Rect OverviewDeskBarView::GetAvailableBounds() const {
  // If the widget is not set yet (which is the case for a brief period during
  // desk bar initialization), use the widget's calculated initial bounds as
  // that's what will be available after the widget does get set.
  return GetWidget() ? GetWidget()->GetRootView()->bounds()
                     : initial_widget_bounds_;
}

BEGIN_METADATA(OverviewDeskBarView)
END_METADATA

}  // namespace ash
