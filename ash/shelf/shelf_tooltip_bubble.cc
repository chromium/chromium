// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_bubble.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ui/aura/window.h"
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
                                       SkColor background_color,
                                       const std::u16string& text)
    : ShelfBubble(anchor, alignment, background_color) {
  set_margins(
      gfx::Insets::VH(kTooltipTopBottomMargin, kTooltipLeftRightMargin));
  set_close_on_deactivate(false);
  SetCanActivate(false);
  set_accept_events(false);
  set_shadow(views::BubbleBorder::NO_SHADOW);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  views::Label* label = new views::Label(text);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  const auto* color_provider = AshColorProvider::Get();
  const bool is_dark_light_mode_enabled = features::IsDarkLightModeEnabled();
  auto background_color_type = AshColorProvider::BaseLayerType::kTransparent80;
  auto text_color_type = AshColorProvider::ContentLayerType::kTextColorPrimary;
  const SkColor tooltip_background =
      is_dark_light_mode_enabled
          ? color_provider->GetInvertedBaseLayerColor(background_color_type)
          : color_provider->GetBaseLayerColor(background_color_type);
  const SkColor tooltip_text =
      is_dark_light_mode_enabled
          ? color_provider->GetInvertedContentLayerColor(text_color_type)
          : color_provider->GetContentLayerColor(text_color_type);

  set_color(tooltip_background);
  label->SetEnabledColor(tooltip_text);
  label->SetBackgroundColor(tooltip_background);
  AddChildView(label);

  CreateBubble();
  CollisionDetectionUtils::IgnoreWindowForCollisionDetection(
      GetWidget()->GetNativeWindow());
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
