// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_view.h"

#include "ash/wm/desks/desks_constants.h"
#include "ui/aura/window.h"

namespace ash {

// -----------------------------------------------------------------------------
// DeskBarView:

DeskBarView::DeskBarView(aura::Window* root)
    : DeskBarViewBase(root, DeskBarViewBase::Type::kDeskButton) {}

const char* DeskBarView::GetClassName() const {
  return "DeskBarView";
}

gfx::Size DeskBarView::CalculatePreferredSize() const {
  // For desk button bar, it comes with dynamic width. Thus, we calculate
  // the preferred width (summation of all child elements and paddings) and
  // use the full available width as the upper limit.
  int width = 0;
  for (auto* child : scroll_view_contents_->children()) {
    if (!child->GetVisible()) {
      continue;
    }
    if (width) {
      width += kDeskBarMiniViewsSpacing;
    }
    width += child->GetPreferredSize().width();
  }
  width += kDeskBarScrollViewMinimumHorizontalPaddingDeskButton * 2 +
           kDeskBarDeskPreviewViewFocusRingThicknessAndPadding * 2;
  width = std::min(width, GetAvailableBounds().width());

  return {width, GetPreferredBarHeight(root_, type_, state_)};
}

}  // namespace ash
