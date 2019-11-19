// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/collapse_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"

namespace ash {

CollapseButton::CollapseButton(views::ButtonListener* listener)
    : CustomShapeButton(listener) {
  OnEnabledChanged();
}

CollapseButton::~CollapseButton() = default;

void CollapseButton::SetExpandedAmount(double expanded_amount) {
  expanded_amount_ = expanded_amount;
  if (expanded_amount == 0.0 || expanded_amount == 1.0) {
    SetTooltipText(l10n_util::GetStringUTF16(expanded_amount == 1.0
                                                 ? IDS_ASH_STATUS_TRAY_COLLAPSE
                                                 : IDS_ASH_STATUS_TRAY_EXPAND));
  }
  SchedulePaint();
}

gfx::Size CollapseButton::CalculatePreferredSize() const {
  return gfx::Size(kTrayItemSize, kTrayItemSize * 3 / 2);
}

SkPath CollapseButton::CreateCustomShapePath(const gfx::Rect& bounds) const {
  SkPath path;
  SkScalar bottom_radius = SkIntToScalar(kTrayItemSize / 2);
  SkScalar radii[8] = {
      0, 0, 0, 0, bottom_radius, bottom_radius, bottom_radius, bottom_radius};
  path.addRoundRect(gfx::RectToSkRect(bounds), radii);
  return path;
}

void CollapseButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintCustomShapePath(canvas);

  gfx::ScopedCanvas scoped(canvas);
  canvas->Translate(gfx::Vector2d(size().width() / 2, size().height() * 2 / 3));
  canvas->sk_canvas()->rotate(expanded_amount_ * 180.);
  gfx::ImageSkia image = GetImageToPaint();
  canvas->DrawImageInt(image, -image.width() / 2, -image.height() / 2);
}

const char* CollapseButton::GetClassName() const {
  return "CollapseButton";
}

void CollapseButton::OnEnabledChanged() {
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconPrimary,
      AshColorProvider::AshColorMode::kDark);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(
               kUnifiedMenuExpandIcon,
               GetEnabled() ? icon_color
                            : AshColorProvider::GetDisabledColor(icon_color)));
}

}  // namespace ash
