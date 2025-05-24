// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_bubble.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ui/aura/window.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/label.h"
#include "ui/views/corewm/tooltip_view_aura.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

ShelfTooltipBubble::ShelfTooltipBubble(
    views::View* anchor,
    ShelfAlignment alignment,
    const std::u16string& text,
    std::optional<views::BubbleBorder::Arrow> arrow_position)
    : ShelfBubble(anchor, alignment, /*for_tooltip=*/true, arrow_position) {
  set_close_on_deactivate(false);
  SetCanActivate(false);
  set_accept_events(false);
  set_shadow(views::BubbleBorder::NO_SHADOW);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  set_margins(gfx::Insets(0));
  auto* tooltip_view = AddChildView(StyleUtil::CreateAshStyleTooltipView());
  tooltip_view->SetText(text);

  CreateBubble();

  CollisionDetectionUtils::IgnoreWindowForCollisionDetection(
      GetWidget()->GetNativeWindow());
}

gfx::Size ShelfTooltipBubble::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const gfx::Size size =
      BubbleDialogDelegateView::CalculatePreferredSize(available_size);

  return size;
}

bool ShelfTooltipBubble::ShouldCloseOnPressDown() {
  // Let the manager close us.
  return true;
}

bool ShelfTooltipBubble::ShouldCloseOnMouseExit() {
  return true;
}

}  // namespace ash
