// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_bubble.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_id.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ui/aura/window.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// Shelf item tooltip height.
constexpr int kTooltipHeight = 24;

// The maximum width of the tooltip bubble.  Borrowed the value from
// ash/tooltip/tooltip_controller.cc
constexpr int kTooltipMaxWidth = 250;

// Shelf item tooltip internal text margins.
constexpr int kTooltipTopBottomMargin = 4;
constexpr int kTooltipLeftRightMargin = 8;

}  // namespace

ShelfTooltipBubble::ShelfTooltipBubble(views::View* anchor,
                                       ShelfAlignment alignment,
                                       const std::u16string& text)
    : ShelfBubble(anchor, alignment) {
  set_margins(
      gfx::Insets::VH(kTooltipTopBottomMargin, kTooltipLeftRightMargin));
  set_close_on_deactivate(false);
  SetCanActivate(false);
  set_accept_events(false);
  set_shadow(views::BubbleBorder::NO_SHADOW);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto label = std::make_unique<views::Label>(text);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Initialize color ids
  label->SetEnabledColorId(kColorAshShelfTooltipForegroundColor);
  label->SetBackgroundColorId(kColorAshShelfTooltipBackgroundColor);
  AddChildView(std::move(label));
  CreateBubble();

  CollisionDetectionUtils::IgnoreWindowForCollisionDetection(
      GetWidget()->GetNativeWindow());
}

void ShelfTooltipBubble::OnThemeChanged() {
  ShelfBubble::OnThemeChanged();

  const auto* color_provider = GetColorProvider();

  // TODO(b/261653838): Update this function to use color id instead.
  set_color(color_provider->GetColor(kColorAshShelfTooltipBackgroundColor));

  // Updates the background color in the bubble frame view.
  GetBubbleFrameView()->SetBackgroundColor(color());
}

gfx::Size ShelfTooltipBubble::CalculatePreferredSize() const {
  const gfx::Size size = BubbleDialogDelegateView::CalculatePreferredSize();
  const int kTooltipMinHeight = kTooltipHeight - 2 * kTooltipTopBottomMargin;
  return gfx::Size(std::min(size.width(), kTooltipMaxWidth),
                   std::max(size.height(), kTooltipMinHeight));
}

bool ShelfTooltipBubble::ShouldCloseOnPressDown() {
  // Let the manager close us.
  return true;
}

bool ShelfTooltipBubble::ShouldCloseOnMouseExit() {
  return true;
}

}  // namespace ash
