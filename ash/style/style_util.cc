// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_util.h"

#include "ash/style/ash_color_provider.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/button/button.h"

namespace ash {

namespace {

std::unique_ptr<views::InkDrop> CreateInkDrop(views::Button* host,
                                              bool highlight_on_hover,
                                              bool highlight_on_focus) {
  return views::InkDrop::CreateInkDropForFloodFillRipple(
      views::InkDrop::Get(host), highlight_on_hover, highlight_on_focus);
}

std::unique_ptr<views::InkDropRipple> CreateInkDropRipple(
    const gfx::Insets& insets,
    const views::View* host,
    SkColor bg_color) {
  const std::pair<SkColor, float> base_color_and_opacity =
      AshColorProvider::Get()->GetInkDropBaseColorAndOpacity(bg_color);
  return std::make_unique<views::FloodFillInkDropRipple>(
      host->size(), insets,
      views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
      base_color_and_opacity.first, base_color_and_opacity.second);
}

std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight(
    views::View* host,
    SkColor bg_color) {
  const std::pair<SkColor, float> base_color_and_opacity =
      AshColorProvider::Get()->GetInkDropBaseColorAndOpacity(bg_color);
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(host->size()), base_color_and_opacity.first);
  highlight->set_visible_opacity(base_color_and_opacity.second);
  return highlight;
}

}  // namespace

// static
void StyleUtil::SetRippleParams(views::View* host,
                                const gfx::Insets& insets,
                                SkColor bg_color) {
  views::InkDrop::Get(host)->SetCreateRippleCallback(
      base::BindRepeating(&CreateInkDropRipple, insets, host, bg_color));
}

// static
void StyleUtil::SetUpInkDropForButton(views::Button* button,
                                      const gfx::Insets& ripple_insets,
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
      &CreateInkDropRipple, ripple_insets, button, bg_color));
  ink_drop->SetCreateHighlightCallback(
      base::BindRepeating(&CreateInkDropHighlight, button, bg_color));
}

// static
void StyleUtil::ConfigureInkDropAttributes(views::View* view,
                                           uint32_t ripple_config_attributes,
                                           SkColor bg_color) {
  const std::pair<SkColor, float> base_color_and_opacity =
      AshColorProvider::Get()->GetInkDropBaseColorAndOpacity(bg_color);

  auto* host = views::InkDrop::Get(view);
  if (ripple_config_attributes & kBaseColor)
    host->SetBaseColor(base_color_and_opacity.first);

  if (ripple_config_attributes & kInkDropOpacity)
    host->SetVisibleOpacity(base_color_and_opacity.second);

  if (ripple_config_attributes & kHighlightOpacity)
    host->SetHighlightOpacity(base_color_and_opacity.second);
}

}  // namespace ash
