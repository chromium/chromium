// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/rounded_label.h"

#include "ash/public/cpp/style/color_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/highlight_border.h"

namespace ash {

RoundedLabel::RoundedLabel(int horizontal_padding,
                           int vertical_padding,
                           int rounding_dp,
                           int preferred_height,
                           const std::u16string& text)
    : views::Label(text, views::style::CONTEXT_LABEL),
      rounding_dp_(rounding_dp),
      preferred_height_(preferred_height) {
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_padding, horizontal_padding)));

  SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  const gfx::RoundedCornersF radii(rounding_dp);
  layer()->SetRoundedCornerRadius(radii);
  layer()->SetIsFastRoundedCorner(true);
}

RoundedLabel::~RoundedLabel() = default;

gfx::Size RoundedLabel::CalculatePreferredSize() const {
  return gfx::Size(views::Label::CalculatePreferredSize().width(),
                   preferred_height_);
}

int RoundedLabel::GetHeightForWidth(int width) const {
  return preferred_height_;
}

void RoundedLabel::OnThemeChanged() {
  views::Label::OnThemeChanged();
  auto* color_provider = ColorProvider::Get();
  const SkColor background_color = color_provider->GetBaseLayerColor(
      ColorProvider::BaseLayerType::kTransparent80);
  background()->SetNativeControlColor(background_color);
  SetBackgroundColor(background_color);
  SetEnabledColor(color_provider->GetContentLayerColor(
      ColorProvider::ContentLayerType::kTextColorPrimary));
}

void RoundedLabel::OnPaintBorder(gfx::Canvas* canvas) {
  views::HighlightBorder::PaintBorderToCanvas(
      canvas, *this, GetLocalBounds(), gfx::RoundedCornersF(rounding_dp_),
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderNoShadow
          : views::HighlightBorder::Type::kHighlightBorder2);
}

}  // namespace ash
