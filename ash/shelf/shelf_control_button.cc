// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_control_button.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shell.h"
#include "ash/style/style_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class ShelfControlButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  ShelfControlButtonHighlightPathGenerator() = default;

  ShelfControlButtonHighlightPathGenerator(
      const ShelfControlButtonHighlightPathGenerator&) = delete;
  ShelfControlButtonHighlightPathGenerator& operator=(
      const ShelfControlButtonHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    auto* shelf_config = ShelfConfig::Get();
    // Some control buttons have a slightly larger size to fill the shelf and
    // maximize the click target, but we still want their "visual" size to be
    // the same.
    gfx::RectF visual_bounds = rect;
    visual_bounds.set_size(
        gfx::SizeF(shelf_config->control_size(), shelf_config->control_size()));
    if (Shell::Get()->IsInTabletMode() && shelf_config->is_in_app()) {
      visual_bounds.Inset(gfx::InsetsF::VH(
          shelf_config->in_app_control_button_height_inset(), 0));
    }
    return gfx::RRectF(visual_bounds, shelf_config->control_border_radius());
  }
};

}  // namespace

ShelfControlButton::ShelfControlButton(
    Shelf* shelf,
    ShelfButtonDelegate* shelf_button_delegate)
    : ShelfButton(shelf, shelf_button_delegate) {
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);

  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
  views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<ShelfControlButtonHighlightPathGenerator>());
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

ShelfControlButton::~ShelfControlButton() = default;

gfx::PointF ShelfControlButton::GetCenterPoint() const {
  return gfx::RectF(GetLocalBounds()).CenterPoint();
}

gfx::Size ShelfControlButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(ShelfConfig::Get()->control_size(),
                   ShelfConfig::Get()->control_size());
}

BEGIN_METADATA(ShelfControlButton)
END_METADATA

}  // namespace ash
