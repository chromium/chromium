// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/rounded_label_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

RoundedLabelButton::RoundedLabelButton(views::ButtonListener* listener,
                                       const base::string16& text)
    : views::LabelButton(listener, text) {
  SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  label()->SetElideBehavior(gfx::NO_ELIDE);
  label()->SetSubpixelRenderingEnabled(false);
  label()->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kTrayItemSize / 2.f);

  focus_ring()->SetColor(UnifiedSystemTrayView::GetFocusRingColor());
}

RoundedLabelButton::~RoundedLabelButton() = default;

gfx::Size RoundedLabelButton::CalculatePreferredSize() const {
  return gfx::Size(label()->GetPreferredSize().width() + kTrayItemSize,
                   kTrayItemSize);
}

int RoundedLabelButton::GetHeightForWidth(int width) const {
  return kTrayItemSize;
}

void RoundedLabelButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::RectF rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawRoundRect(rect, kTrayItemCornerRadius, flags);

  views::LabelButton::PaintButtonContents(canvas);
}

std::unique_ptr<views::InkDrop> RoundedLabelButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> RoundedLabelButton::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight>
RoundedLabelButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(this);
}

const char* RoundedLabelButton::GetClassName() const {
  return "RoundedLabelButton";
}

}  // namespace ash
