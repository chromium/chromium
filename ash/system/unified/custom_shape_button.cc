// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/custom_shape_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace {
class CustomShapeButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  CustomShapeButtonHighlightPathGenerator() = default;

  SkPath GetHighlightPath(const views::View* view) override {
    return static_cast<const ash::CustomShapeButton*>(view)
        ->CreateCustomShapePath(view->GetLocalBounds());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomShapeButtonHighlightPathGenerator);
};
}  // namespace

namespace ash {

CustomShapeButton::CustomShapeButton(views::ButtonListener* listener)
    : ImageButton(listener) {
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<CustomShapeButtonHighlightPathGenerator>());
}

CustomShapeButton::~CustomShapeButton() = default;

void CustomShapeButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintCustomShapePath(canvas);
  views::ImageButton::PaintButtonContents(canvas);
}

std::unique_ptr<views::InkDrop> CustomShapeButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> CustomShapeButton::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent(),
      UnifiedSystemTrayView::GetBackgroundColor());
}

std::unique_ptr<views::InkDropHighlight>
CustomShapeButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      UnifiedSystemTrayView::GetBackgroundColor());
}

const char* CustomShapeButton::GetClassName() const {
  return "CustomShapeButton";
}

void CustomShapeButton::PaintCustomShapePath(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  const SkColor button_color =
      AshColorProvider::Get()->DeprecatedGetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kInactiveControlBackground,
          kUnifiedMenuButtonColor);
  flags.setColor(GetEnabled()
                     ? button_color
                     : AshColorProvider::GetDisabledColor(button_color));
  flags.setStyle(cc::PaintFlags::kFill_Style);

  canvas->DrawPath(CreateCustomShapePath(GetLocalBounds()), flags);
}

}  // namespace ash
