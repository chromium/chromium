// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/custom_shape_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace {
class CustomShapeButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  CustomShapeButtonHighlightPathGenerator() = default;

  CustomShapeButtonHighlightPathGenerator(
      const CustomShapeButtonHighlightPathGenerator&) = delete;
  CustomShapeButtonHighlightPathGenerator& operator=(
      const CustomShapeButtonHighlightPathGenerator&) = delete;

  SkPath GetHighlightPath(const views::View* view) override {
    return static_cast<const ash::CustomShapeButton*>(view)
        ->CreateCustomShapePath(view->GetLocalBounds());
  }
};
}  // namespace

namespace ash {

CustomShapeButton::CustomShapeButton(PressedCallback callback)
    : ImageButton(std::move(callback)) {
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<CustomShapeButtonHighlightPathGenerator>());
}

CustomShapeButton::~CustomShapeButton() = default;

void CustomShapeButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintCustomShapePath(canvas);
  views::ImageButton::PaintButtonContents(canvas);
}

const char* CustomShapeButton::GetClassName() const {
  return "CustomShapeButton";
}

void CustomShapeButton::OnThemeChanged() {
  ImageButton::OnThemeChanged();
  views::FocusRing::Get(this)->SetColor(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor));
  SchedulePaint();
}

void CustomShapeButton::PaintCustomShapePath(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  const SkColor button_color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  flags.setColor(GetEnabled()
                     ? button_color
                     : AshColorProvider::GetDisabledColor(button_color));
  flags.setStyle(cc::PaintFlags::kFill_Style);

  canvas->DrawPath(CreateCustomShapePath(GetLocalBounds()), flags);
}

}  // namespace ash
