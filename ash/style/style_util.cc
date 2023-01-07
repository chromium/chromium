// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_util.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ui/color/color_id.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"

namespace ash {

// static
float StyleUtil::GetInkDropOpacity() {
  return DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
             ? kDarkInkDropOpacity
             : kLightInkDropOpacity;
}

// static
std::unique_ptr<views::InkDrop> StyleUtil::CreateInkDrop(
    views::Button* host,
    bool highlight_on_hover,
    bool highlight_on_focus) {
  return views::InkDrop::CreateInkDropForFloodFillRipple(
      views::InkDrop::Get(host), highlight_on_hover, highlight_on_focus);
}

// static
std::unique_ptr<views::InkDropRipple> StyleUtil::CreateInkDropRipple(
    const gfx::Insets& insets,
    const views::View* host,
    SkColor background_color) {
  const std::pair<SkColor, float> base_color_and_opacity =
      AshColorProvider::Get()->GetInkDropBaseColorAndOpacity(background_color);
  return std::make_unique<views::FloodFillInkDropRipple>(
      host->size(), insets,
      views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
      base_color_and_opacity.first, base_color_and_opacity.second);
}

// static
std::unique_ptr<views::InkDropHighlight> StyleUtil::CreateInkDropHighlight(
    const views::View* host,
    SkColor background_color) {
  const std::pair<SkColor, float> base_color_and_opacity =
      AshColorProvider::Get()->GetInkDropBaseColorAndOpacity(background_color);
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(host->size()), base_color_and_opacity.first);
  highlight->set_visible_opacity(base_color_and_opacity.second);
  return highlight;
}

// static
void StyleUtil::SetRippleParams(views::View* host,
                                const gfx::Insets& insets,
                                SkColor background_color) {
  views::InkDrop::Get(host)->SetCreateRippleCallback(base::BindRepeating(
      &CreateInkDropRipple, insets, host, background_color));
}

// static
void StyleUtil::SetUpInkDropForButton(views::Button* button,
                                      const gfx::Insets& ripple_insets,
                                      bool highlight_on_hover,
                                      bool highlight_on_focus,
                                      SkColor background_color) {
  button->SetInstallFocusRingOnFocus(true);
  views::InkDropHost* const ink_drop = views::InkDrop::Get(button);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
  ink_drop->SetCreateInkDropCallback(base::BindRepeating(
      &CreateInkDrop, button, highlight_on_hover, highlight_on_focus));
  ink_drop->SetCreateRippleCallback(base::BindRepeating(
      &CreateInkDropRipple, ripple_insets, button, background_color));
  ink_drop->SetCreateHighlightCallback(
      base::BindRepeating(&CreateInkDropHighlight, button, background_color));
}

// static
void StyleUtil::ConfigureInkDropAttributes(views::View* view,
                                           uint32_t ripple_config_attributes,
                                           SkColor background_color) {
  const std::pair<SkColor, float> base_color_and_opacity =
      AshColorProvider::Get()->GetInkDropBaseColorAndOpacity(background_color);

  auto* host = views::InkDrop::Get(view);
  if (ripple_config_attributes & kBaseColor)
    host->SetBaseColor(base_color_and_opacity.first);

  if (ripple_config_attributes & kInkDropOpacity)
    host->SetVisibleOpacity(base_color_and_opacity.second);

  if (ripple_config_attributes & kHighlightOpacity)
    host->SetHighlightOpacity(base_color_and_opacity.second);
}

// static
views::FocusRing* StyleUtil::SetUpFocusRingForView(
    views::View* view,
    absl::optional<int> halo_inset) {
  DCHECK(view);
  views::FocusRing::Install(view);
  views::FocusRing* focus_ring = views::FocusRing::Get(view);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  if (halo_inset)
    focus_ring->SetHaloInset(*halo_inset);
  return focus_ring;
}

}  // namespace ash
