// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/custom_shape_button.h"

#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
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
  StyleUtil::SetUpInkDropForButton(this);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<CustomShapeButtonHighlightPathGenerator>());
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
}

CustomShapeButton::~CustomShapeButton() = default;

void CustomShapeButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintCustomShapePath(canvas);
  views::ImageButton::PaintButtonContents(canvas);
}

void CustomShapeButton::PaintCustomShapePath(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  const SkColor button_color =
      GetColorProvider()->GetColor(kColorAshControlBackgroundColorInactive);
  flags.setColor(GetEnabled() ? button_color
                              : ColorUtil::GetDisabledColor(button_color));
  flags.setStyle(cc::PaintFlags::kFill_Style);

  canvas->DrawPath(CreateCustomShapePath(GetLocalBounds()), flags);
}

BEGIN_METADATA(CustomShapeButton)
END_METADATA

}  // namespace ash
