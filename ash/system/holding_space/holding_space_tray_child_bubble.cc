// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_child_bubble.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_views_section.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

HoldingSpaceTrayChildBubble::HoldingSpaceTrayChildBubble(
    HoldingSpaceItemViewDelegate* delegate)
    : delegate_(delegate) {}

HoldingSpaceTrayChildBubble::~HoldingSpaceTrayChildBubble() = default;

void HoldingSpaceTrayChildBubble::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kHoldingSpaceChildBubblePadding,
      kHoldingSpaceChildBubbleChildSpacing));

  // Layer.
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetBackgroundBlur(kUnifiedMenuBackgroundBlur);
  layer()->SetColor(AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kUnifiedTrayCornerRadius});

  // Sections.
  for (auto& section : CreateSections()) {
    sections_.push_back(AddChildView(std::move(section)));
    sections_.back()->Init();
  }
}

void HoldingSpaceTrayChildBubble::Reset() {
  for (HoldingSpaceItemViewsSection* section : sections_)
    section->Reset();
}

const char* HoldingSpaceTrayChildBubble::GetClassName() const {
  return "HoldingSpaceTrayChildBubble";
}

void HoldingSpaceTrayChildBubble::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void HoldingSpaceTrayChildBubble::ChildVisibilityChanged(views::View* child) {
  // This child bubble should be visible iff it has visible children.
  bool visible = false;
  for (const views::View* c : children()) {
    if (c->GetVisible()) {
      visible = true;
      break;
    }
  }

  if (visible != GetVisible())
    SetVisible(visible);

  PreferredSizeChanged();
}

}  // namespace ash
