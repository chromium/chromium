// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_OVERVIEW_DESK_BAR_VIEW_H_
#define ASH_WM_DESKS_OVERVIEW_DESK_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/overview/overview_grid.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class WindowOcclusionCalculator;

// A bar that resides at the top portion of the overview, which contains desk
// mini views, the new desk button, the library button, and the scroll arrow
// buttons.
class ASH_EXPORT OverviewDeskBarView : public DeskBarViewBase {
  METADATA_HEADER(OverviewDeskBarView, DeskBarViewBase)

 public:
  OverviewDeskBarView(
      base::WeakPtr<OverviewGrid> overview_grid,
      base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator,
      const gfx::Rect& initial_widget_bounds);

  OverviewDeskBarView(const OverviewDeskBarView&) = delete;
  OverviewDeskBarView& operator=(const OverviewDeskBarView&) = delete;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // DeskBarViewBase:
  gfx::Rect GetAvailableBounds() const override;

 private:
  const gfx::Rect initial_widget_bounds_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_OVERVIEW_DESK_BAR_VIEW_H_
