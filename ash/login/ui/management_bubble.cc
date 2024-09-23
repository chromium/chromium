// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/management_bubble.h"

#include "ash/login/ui/lock_contents_view_constants.h"
#include "ash/login/ui/login_tooltip_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

// Width of the management bubble.
constexpr int kManagementBubbleWidth = 400;

// Maximum width of the management bubble label.
constexpr int kManagementBubbleLabelMaxWidth =
    kManagementBubbleWidth - 2 * kBubblePaddingDp - kInfoIconSizeDp -
    kBubbleBetweenChildSpacingDp;

ManagementBubble::ManagementBubble(const std::u16string& message,
                                   base::WeakPtr<views::View> anchor_view)
    : LoginTooltipView(message, std::move(anchor_view)) {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(kBubblePaddingDp), kBubbleBetweenChildSpacingDp));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  label()->SetMaximumWidth(kManagementBubbleLabelMaxWidth);

  set_positioning_strategy(PositioningStrategy::kShowAbove);
}

gfx::Size ManagementBubble::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kManagementBubbleWidth,
                   GetLayoutManager()->GetPreferredHeightForWidth(
                       this, kManagementBubbleWidth));
}

BEGIN_METADATA(ManagementBubble)
END_METADATA

}  // namespace ash
