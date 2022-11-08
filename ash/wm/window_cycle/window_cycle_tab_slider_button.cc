// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_tab_slider_button.h"

#include "ash/style/ash_color_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// The height of the tab slider button.
constexpr int kTabSliderButtonHeight = 32;

// The horizontal insets between the label and the button.
constexpr int kTabSliderButtonHorizontalInsets = 20;

// The font size of the button label.
constexpr int kTabSliderButtonLabelFontSizeDp = 13;

}  // namespace

WindowCycleTabSliderButton::WindowCycleTabSliderButton(
    views::Button::PressedCallback callback,
    const std::u16string& label_text)
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
}

void WindowCycleTabSliderButton::SetToggled(bool is_toggled) {
  if (is_toggled == toggled_)
    return;
  toggled_ = is_toggled;
  SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
      toggled_ ? AshColorProvider::ContentLayerType::kButtonLabelColorPrimary
               : AshColorProvider::ContentLayerType::kTextColorPrimary));
}

gfx::Size WindowCycleTabSliderButton::CalculatePreferredSize() const {
  return gfx::Size(label()->GetPreferredSize().width() +
                       2 * kTabSliderButtonHorizontalInsets,
                   kTabSliderButtonHeight);
}

BEGIN_METADATA(WindowCycleTabSliderButton, views::LabelButton)
END_METADATA

}  // namespace ash
