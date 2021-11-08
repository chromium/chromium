// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_util.h"

#include "ash/style/ash_color_provider.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/button/button.h"

namespace ash {

namespace {

constexpr gfx::Insets kInkDropInsets(4);

gfx::Insets GetInkDropInsets(TrayPopupInkDropStyle ink_drop_style) {
  if (ink_drop_style == TrayPopupInkDropStyle::HOST_CENTERED ||
      ink_drop_style == TrayPopupInkDropStyle::INSET_BOUNDS) {
    return kInkDropInsets;
  }
  return gfx::Insets();
}

std::unique_ptr<views::InkDrop> CreateInkDrop(views::Button* host,
                                              bool highlight_on_hover,
                                              bool highlight_on_focus) {
  return views::InkDrop::CreateInkDropForFloodFillRipple(
      views::InkDrop::Get(host), highlight_on_hover, highlight_on_focus);
}

std::unique_ptr<views::InkDropRipple> CreateInkDropRipple(
    TrayPopupInkDropStyle ink_drop_style,
    const views::Button* host,
    SkColor bg_color) {
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes(bg_color);
  return std::make_unique<views::FloodFillInkDropRipple>(
      host->size(), GetInkDropInsets(ink_drop_style),
      views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
      ripple_attributes.base_color, ripple_attributes.inkdrop_opacity);
}

std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight(
    const views::View* host,
    SkColor bg_color) {
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes(bg_color);
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(host->size()), ripple_attributes.base_color);
  highlight->set_visible_opacity(ripple_attributes.highlight_opacity);
  return highlight;
}

}  // namespace

// static
void StyleUtil::SetUpInkDropForButton(views::Button* button,
                                      TrayPopupInkDropStyle ink_drop_style,
                                      bool highlight_on_hover,
                                      bool highlight_on_focus,
                                      SkColor bg_color) {
  button->SetInstallFocusRingOnFocus(true);
  views::InkDropHost* const ink_drop = views::InkDrop::Get(button);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
  ink_drop->SetCreateInkDropCallback(base::BindRepeating(
      &CreateInkDrop, button, highlight_on_hover, highlight_on_focus));
  ink_drop->SetCreateRippleCallback(base::BindRepeating(
      &CreateInkDropRipple, ink_drop_style, button, bg_color));
  ink_drop->SetCreateHighlightCallback(
      base::BindRepeating(&CreateInkDropHighlight, button, bg_color));
}

// static
void StyleUtil::ConfigureInkDropAttributes(views::View* view,
                                           uint32_t ripple_config_attributes,
                                           SkColor bg_color) {
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes(bg_color);

  auto* host = views::InkDrop::Get(view);
  if (ripple_config_attributes & kBaseColor)
    host->SetBaseColor(ripple_attributes.base_color);

  if (ripple_config_attributes & kInkDropOpacity)
    host->SetVisibleOpacity(ripple_attributes.inkdrop_opacity);

  if (ripple_config_attributes & kHighlightOpacity)
    host->SetHighlightOpacity(ripple_attributes.highlight_opacity);
}

}  // namespace ash
