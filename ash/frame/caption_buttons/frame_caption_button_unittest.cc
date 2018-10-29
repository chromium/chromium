// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/caption_buttons/frame_caption_button.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"

namespace {

constexpr SkColor kBackgroundColors[] = {
    SK_ColorBLACK,  SK_ColorDKGRAY,
    SK_ColorGRAY,   SK_ColorLTGRAY,
    SK_ColorWHITE,  SK_ColorRED,
    SK_ColorYELLOW, SK_ColorCYAN,
    SK_ColorBLUE,   SkColorSetRGB(230, 138, 90),
};

}  // namespace

TEST(FrameCaptionButtonTest, ThemedColorContrast) {
  for (SkColor background_color : kBackgroundColors) {
    SkColor button_color = ash::FrameCaptionButton::GetButtonColor(
        ash::FrameCaptionButton::ColorMode::kThemed, background_color);
    EXPECT_GE(color_utils::GetContrastRatio(button_color, background_color), 3);
  }
}
