// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_color_generator.h"

namespace ash {

namespace {

// The array of the candidate colors for app icons.
constexpr std::array<SkColor, 7> kColorArray = {
    SK_ColorWHITE,  SK_ColorRED,  SK_ColorGREEN,  SK_ColorBLUE,
    SK_ColorYELLOW, SK_ColorCYAN, SK_ColorMAGENTA};

}  // namespace

AshTestColorGenerator::AshTestColorGenerator(SkColor default_color)
    : default_color_(default_color) {}

AshTestColorGenerator::~AshTestColorGenerator() = default;

SkColor AshTestColorGenerator::GetAlternativeColor() {
  const SkColor color = kColorArray[next_color_index_];
  next_color_index_ = (next_color_index_ + 1) % kColorArray.size();
  return color;
}

}  // namespace ash
