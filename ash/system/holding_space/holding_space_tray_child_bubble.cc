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

namespace {

// TopAlignedBoxLayout ---------------------------------------------------------

// A vertical `views::BoxLayout` which overrides layout behavior when there is
// insufficient layout space to accommodate all children's preferred sizes.
// Unlike `views::BoxLayout` which will not allow children to exceed its content
// bounds, TopAlignedBoxLayout will ensure that children still receive their
// preferred sizes. This prevents layout jank that would otherwise occur when
// the host view's bounds are being animated due to content changes.
class TopAlignedBoxLayout : public views::BoxLayout {
 public:
  TopAlignedBoxLayout(const gfx::Insets& insets, int spacing)
      : views::BoxLayout(views::BoxLayout::Orientation::kVertical,
                         insets,
                         spacing) {}

 private:
  // views::BoxLayout:
  void Layout(views::View* host) override {
    if (host->height() >= host->GetPreferredSize().height()) {
      views::BoxLayout::Layout(host);
      return;
    }

    gfx::Rect contents_bounds(host->GetContentsBounds());
    contents_bounds.Inset(inside_border_insets());

    // If we only have a single child view and that child view is okay with
    // being sized arbitrarily small, short circuit layout logic and give that
    // child all available layout space. This is the case for the
    // `PinnedFileSection` which supports scrolling its content when necessary.
    if (host->children().size() == 1u &&
        host->children()[0]->GetMinimumSize().IsEmpty()) {
      host->children()[0]->SetBoundsRect(contents_bounds);
      return;
    }

    int top = contents_bounds.y();
    int left = contents_bounds.x();
    int width = contents_bounds.width();

    for (views::View* child : host->children()) {
      gfx::Size size(width, child->GetHeightForWidth(width));
      child->SetBounds(left, top, size.width(), size.height());
      top += size.height() + between_child_spacing();
    }
  }
};

}  // namespace

// HoldingSpaceTrayChildBubble -------------------------------------------------

HoldingSpaceTrayChildBubble::HoldingSpaceTrayChildBubble(
    HoldingSpaceItemViewDelegate* delegate)
    : delegate_(delegate) {}

HoldingSpaceTrayChildBubble::~HoldingSpaceTrayChildBubble() = default;

void HoldingSpaceTrayChildBubble::Init() {
  // Layout.
  SetLayoutManager(std::make_unique<TopAlignedBoxLayout>(
      kHoldingSpaceChildBubblePadding, kHoldingSpaceChildBubbleChildSpacing));

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
