// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/rounded_label_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
namespace {

SkColor GetBackgroundColor() {
  return AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
}

}  // namespace

RoundedLabelButton::RoundedLabelButton(PressedCallback callback,
                                       const base::string16& text)
    : views::LabelButton(std::move(callback), text) {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  label()->SetElideBehavior(gfx::NO_ELIDE);
  label()->SetSubpixelRenderingEnabled(false);
  label()->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kTrayItemSize / 2.f);
  SetBackground(views::CreateRoundedRectBackground(GetBackgroundColor(),
                                                   kTrayItemCornerRadius));
}

void RoundedLabelButton::OnThemeChanged() {
  views::LabelButton::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  color_provider->DecoratePillButton(this, /*icon=*/nullptr);
  focus_ring()->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  background()->SetNativeControlColor(GetBackgroundColor());
}

RoundedLabelButton::~RoundedLabelButton() = default;

gfx::Size RoundedLabelButton::CalculatePreferredSize() const {
  return gfx::Size(label()->GetPreferredSize().width() + kTrayItemSize,
                   kTrayItemSize);
}

int RoundedLabelButton::GetHeightForWidth(int width) const {
  return kTrayItemSize;
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
