// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_LEGACY_DESK_BAR_VIEW_H_
#define ASH_WM_DESKS_LEGACY_DESK_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/overview/overview_grid.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// A bar that resides at the top portion of the overview, which contains desk
// mini views, the new desk button, the library button, and the scroll arrow
// buttons.
class ASH_EXPORT LegacyDeskBarView : public DeskBarViewBase {
 public:
  METADATA_HEADER(LegacyDeskBarView);

  explicit LegacyDeskBarView(base::WeakPtr<OverviewGrid> overview_grid);

  LegacyDeskBarView(const LegacyDeskBarView&) = delete;
  LegacyDeskBarView& operator=(const LegacyDeskBarView&) = delete;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_LEGACY_DESK_BAR_VIEW_H_
