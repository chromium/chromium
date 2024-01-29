// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/sparkle_colors.h"

#include "ash/style/harmonized_colors.h"
#include "third_party/material_color_utilities/src/cpp/cam/hct.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_recipe.h"

namespace ash {

namespace {

// Mappings of tones to ColorId as they comprise the tonal palette.
// https://m3.material.io/styles/color/the-color-system/key-colors-tones
constexpr std::array<std::pair<int, ui::ColorId>, 13> kAnalogIds{
    {{0, cros_tokens::kCrosRefSparkleAnalog0},
     {10, cros_tokens::kCrosRefSparkleAnalog10},
     {20, cros_tokens::kCrosRefSparkleAnalog20},
     {30, cros_tokens::kCrosRefSparkleAnalog30},
     {40, cros_tokens::kCrosRefSparkleAnalog40},
     {50, cros_tokens::kCrosRefSparkleAnalog50},
     {60, cros_tokens::kCrosRefSparkleAnalog60},
     {70, cros_tokens::kCrosRefSparkleAnalog70},
     {80, cros_tokens::kCrosRefSparkleAnalog80},
     {90, cros_tokens::kCrosRefSparkleAnalog90},
     {95, cros_tokens::kCrosRefSparkleAnalog95},
     {99, cros_tokens::kCrosRefSparkleAnalog99},
     {100, cros_tokens::kCrosRefSparkleAnalog100}}};

constexpr std::array<std::pair<int, ui::ColorId>, 13> kMutedIds{
    {{0, cros_tokens::kCrosRefSparkleMuted0},
     {10, cros_tokens::kCrosRefSparkleMuted10},
     {20, cros_tokens::kCrosRefSparkleMuted20},
     {30, cros_tokens::kCrosRefSparkleMuted30},
     {40, cros_tokens::kCrosRefSparkleMuted40},
     {50, cros_tokens::kCrosRefSparkleMuted50},
     {60, cros_tokens::kCrosRefSparkleMuted60},
     {70, cros_tokens::kCrosRefSparkleMuted70},
     {80, cros_tokens::kCrosRefSparkleMuted80},
     {90, cros_tokens::kCrosRefSparkleMuted90},
     {95, cros_tokens::kCrosRefSparkleMuted95},
     {99, cros_tokens::kCrosRefSparkleMuted99},
     {100, cros_tokens::kCrosRefSparkleMuted100}}};

constexpr std::array<std::pair<int, ui::ColorId>, 13> kComplementIds{
    {{0, cros_tokens::kCrosRefSparkleComplement0},
     {10, cros_tokens::kCrosRefSparkleComplement10},
     {20, cros_tokens::kCrosRefSparkleComplement20},
     {30, cros_tokens::kCrosRefSparkleComplement30},
     {40, cros_tokens::kCrosRefSparkleComplement40},
     {50, cros_tokens::kCrosRefSparkleComplement50},
     {60, cros_tokens::kCrosRefSparkleComplement60},
     {70, cros_tokens::kCrosRefSparkleComplement70},
     {80, cros_tokens::kCrosRefSparkleComplement80},
     {90, cros_tokens::kCrosRefSparkleComplement90},
     {95, cros_tokens::kCrosRefSparkleComplement95},
     {99, cros_tokens::kCrosRefSparkleComplement99},
     {100, cros_tokens::kCrosRefSparkleComplement100}}};

struct HueChroma {
  int hue;
  int chroma;
};

// For the ColorId in `data`, generate the appropriate tone values combined with
// hue and chroma from `hc` and add the mappings to `mixer`.
void InsertIntoMixer(ui::ColorMixer& mixer,
                     const HueChroma& hc,
                     const std::array<std::pair<int, ui::ColorId>, 13>& data) {
  for (const auto& [tone, color_id] : data) {
    material_color_utilities::Hct hct(hc.hue, hc.chroma, tone);

    // `Hct::ToInt()` is in Argb which matches the representation of SkColor
    // (ARGB).
    mixer[color_id] = {hct.ToInt()};
  }
}

}  // namespace

void AddSparkleColors(ui::ColorMixer& mixer, const ui::ColorProviderKey& key) {
  int angle = 217;  // Default to blue
  if (key.user_color) {
    angle = HueAngle(*key.user_color);
  }

  HueChroma analog;
  HueChroma muted;
  HueChroma complement;
  if (angle > 49 && angle < 121) {
    analog = {.hue = 283, .chroma = 60};
    muted = {.hue = 273, .chroma = 35};
    complement = {.hue = 170, .chroma = 30};
  } else {
    analog = {.hue = 303, .chroma = 60};
    muted = {.hue = 293, .chroma = 35};
    complement = {.hue = 150, .chroma = 30};
  }

  InsertIntoMixer(mixer, analog, kAnalogIds);
  InsertIntoMixer(mixer, muted, kMutedIds);
  InsertIntoMixer(mixer, complement, kComplementIds);
}

}  // namespace ash
