// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BAR_VIEW_H_
#define ASH_WM_DESKS_DESK_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class WindowOcclusionCalculator;

// Desk bar that contains desk related UI including desk thumbnails, new desk
// button, library button, and scroll arrow buttons when the available space is
// not enough. For now this is only used for desk bar triggered by desk button
// on the shelf.
class ASH_EXPORT DeskBarView : public DeskBarViewBase {
  METADATA_HEADER(DeskBarView, DeskBarViewBase)

 public:
  DeskBarView(
      aura::Window* root,
      base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator);

  DeskBarView(const DeskBarView&) = delete;
  DeskBarView& operator=(const DeskBarView&) = delete;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // DeskBarViewBase:
  gfx::Rect GetAvailableBounds() const override;
  void UpdateBarBounds() override;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_BAR_VIEW_H_
