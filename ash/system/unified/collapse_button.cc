// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/collapse_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

CollapseButton::CollapseButton(PressedCallback callback)
    : ImageButton(std::move(callback)) {
  views::InstallCircleHighlightPathGenerator(this);
  TrayPopupUtils::ConfigureTrayPopupButton(this);
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
  return gfx::Size(kTrayItemSize, kTrayItemSize);
}

void CollapseButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped(canvas);
  canvas->Translate(gfx::Vector2d(size().width() / 2, size().height() / 2));
  canvas->sk_canvas()->rotate(expanded_amount_ * 180.);
  gfx::ImageSkia image = GetImageToPaint();
  canvas->DrawImageInt(image, -image.width() / 2, -image.height() / 2);
}

std::unique_ptr<views::InkDrop> CollapseButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> CollapseButton::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight>
CollapseButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(this);
}

const char* CollapseButton::GetClassName() const {
  return "CollapseButton";
}

void CollapseButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  AshColorProvider::Get()->DecorateFloatingIconButton(this,
                                                      kUnifiedMenuExpandIcon);
  focus_ring()->SetColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
}

}  // namespace ash
