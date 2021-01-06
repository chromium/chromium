// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_tab_slider_button.h"

#include "ash/style/ash_color_provider.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

// The height of the tab slider button.
constexpr int kTabSliderButtonHeight = 32;

// The round radius of the slider, which is half of its diameter (height).
constexpr int kTabSliderRoundRadius = int{kTabSliderButtonHeight / 2};

// The horizontal insets between the label and the button.
constexpr int kTabSliderButtonHorizontalInsets = 20;

// The font size of the button label.
constexpr int kTabSliderButtonLabelFontSizeDp = 13;

}  // namespace

WindowCycleTabSliderButton::WindowCycleTabSliderButton(
    views::Button::PressedCallback callback,
    const base::string16& label_text)
    : LabelButton(std::move(callback), label_text) {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  label()->SetFontList(
      label()
          ->font_list()
          .DeriveWithSizeDelta(kTabSliderButtonLabelFontSizeDp -
                               label()->font_list().GetFontSize())
          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));

  SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));

  SetBorder(views::CreateEmptyBorder(gfx::Insets()));
}

void WindowCycleTabSliderButton::SetToggled(bool is_toggled) {
  if (is_toggled == toggled_)
    return;
  toggled_ = is_toggled;
  SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
      toggled_ ? AshColorProvider::ContentLayerType::kButtonLabelColorPrimary
               : AshColorProvider::ContentLayerType::kTextColorPrimary));
  // SchedulePaint triggers OnPaintBackground
  SchedulePaint();
}

void WindowCycleTabSliderButton::OnPaintBackground(gfx::Canvas* canvas) {
  if (!toggled_)
    return;

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorActive));
  canvas->DrawRoundRect(GetContentsBounds(), kTabSliderRoundRadius, flags);
}

gfx::Size WindowCycleTabSliderButton::CalculatePreferredSize() const {
  return gfx::Size(label()->GetPreferredSize().width() +
                       2 * kTabSliderButtonHorizontalInsets,
                   kTabSliderButtonHeight);
}

BEGIN_METADATA(WindowCycleTabSliderButton, views::LabelButton)
END_METADATA

}  // namespace ash
