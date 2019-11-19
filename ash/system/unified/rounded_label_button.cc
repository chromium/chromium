// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/rounded_label_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

RoundedLabelButton::RoundedLabelButton(views::ButtonListener* listener,
                                       const base::string16& text)
    : views::LabelButton(listener, text) {
  SetEnabledTextColors(AshColorProvider::Get()->DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextPrimary, kUnifiedMenuTextColor));
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  label()->SetElideBehavior(gfx::NO_ELIDE);
  label()->SetSubpixelRenderingEnabled(false);
  label()->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  views::InstallPillHighlightPathGenerator(this);
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
  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kInactiveControlBackground,
      kUnifiedMenuButtonColor));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawRoundRect(rect, kTrayItemSize / 2, flags);

  views::LabelButton::PaintButtonContents(canvas);
}

std::unique_ptr<views::InkDrop> RoundedLabelButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> RoundedLabelButton::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent(),
      UnifiedSystemTrayView::GetBackgroundColor());
}

std::unique_ptr<views::InkDropHighlight>
RoundedLabelButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      UnifiedSystemTrayView::GetBackgroundColor());
}

std::unique_ptr<views::InkDropMask> RoundedLabelButton::CreateInkDropMask()
    const {
  return std::make_unique<views::RoundRectInkDropMask>(size(), gfx::Insets(),
                                                       kTrayItemSize / 2);
}

const char* RoundedLabelButton::GetClassName() const {
  return "RoundedLabelButton";
}

}  // namespace ash
