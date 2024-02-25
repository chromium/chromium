// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scroll_arrow_view.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/style/ash_color_provider.h"
#include "base/i18n/rtl.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"

namespace ash {

ScrollArrowView::ScrollArrowView(ArrowType arrow_type,
                                 bool is_horizontal_alignment,
                                 Shelf* shelf,
                                 ShelfButtonDelegate* shelf_button_delegate)
    : ShelfButton(shelf, shelf_button_delegate),
      arrow_type_(arrow_type),
      is_horizontal_alignment_(is_horizontal_alignment) {
  SetHasInkDropActionOnClick(true);
  views::InkDrop::Get(this)->SetMode(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
}

ScrollArrowView::~ScrollArrowView() = default;

void ScrollArrowView::NotifyClick(const ui::Event& event) {
  Button::NotifyClick(event);
  shelf_button_delegate()->ButtonPressed(
      /*sender=*/this, event, views::InkDrop::Get(this)->GetInkDrop());
}

void ScrollArrowView::PaintButtonContents(gfx::Canvas* canvas) {
  const bool show_left_arrow = (arrow_type_ == kLeft && !base::i18n::IsRTL()) ||
                               (arrow_type_ == kRight && base::i18n::IsRTL());
  gfx::ImageSkia img = CreateVectorIcon(
      show_left_arrow ? kOverflowShelfLeftIcon : kOverflowShelfRightIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));

  if (!is_horizontal_alignment_) {
    img = gfx::ImageSkiaOperations::CreateRotatedImage(
        img, base::i18n::IsRTL() ? SkBitmapOperations::ROTATION_270_CW
                                 : SkBitmapOperations::ROTATION_90_CW);
  }

  gfx::PointF center_point(width() / 2.f, height() / 2.f);
  canvas->DrawImageInt(img, center_point.x() - img.width() / 2,
                       center_point.y() - img.height() / 2);
}

void ScrollArrowView::OnThemeChanged() {
  ShelfButton::OnThemeChanged();
  SchedulePaint();
}

BEGIN_METADATA(ScrollArrowView)
END_METADATA

}  // namespace ash
