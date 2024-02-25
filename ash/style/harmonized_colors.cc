// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/harmonized_colors.h"

#include <functional>
#include <utility>

#include "base/ranges/algorithm.h"
#include "third_party/material_color_utilities/src/cpp/palettes/tones.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_recipe.h"

namespace ash {

namespace {

using material_color_utilities::TonalPalette;

// Upper limits for the angle range.
constexpr std::array<SkColor, 4> kAngles = {49, 159, 219, 360};
constexpr std::array<SkColor, 4> kGreens = {0x5BA22FU, 0x4FA834U, 0x34A866,
                                            0x34A87AU};
constexpr std::array<SkColor, 4> kYellows = {0xEF9800U, 0xFBBC04, 0xFBC104U,
                                             0xFBA904U};
constexpr std::array<SkColor, 4> kReds = {0xF95C45U, 0xEA7135, 0xEA6235,
                                          0xEA3553};
constexpr std::array<SkColor, 4> kBlues = {0x3F5AA9, 0x00829D, 0x00829D,
                                           0x3F5AA9};

struct HarmonizedSeeds {
  SkColor red;
  SkColor green;
  SkColor yellow;
  SkColor blue;
};

std::pair<int, HarmonizedSeeds> MakeSeedEntry(int i) {
  HarmonizedSeeds seed;
  seed.red = kReds[i];
  seed.green = kGreens[i];
  seed.yellow = kYellows[i];
  seed.blue = kBlues[i];
  return std::make_pair(kAngles[i], seed);
}

const base::flat_map<int, HarmonizedSeeds> MakeMap() {
  std::vector<std::pair<int, HarmonizedSeeds>> storage;
  storage.reserve(4);
  for (int i = 0; i < 4; i++) {
    storage.push_back(MakeSeedEntry(i));
  }
  return base::flat_map<int, HarmonizedSeeds>(base::sorted_unique_t(),
                                              std::move(storage));
}

// Mappings of tones to ColorId as they comprise the tonal palette.
// https://m3.material.io/styles/color/the-color-system/key-colors-tones
constexpr std::array<std::pair<int, ui::ColorId>, 13> kRedIds{
    std::make_pair(0, cros_tokens::kCrosRefRed0),
    {10, cros_tokens::kCrosRefRed10},
    {20, cros_tokens::kCrosRefRed20},
    {30, cros_tokens::kCrosRefRed30},
    {40, cros_tokens::kCrosRefRed40},
    {50, cros_tokens::kCrosRefRed50},
    {60, cros_tokens::kCrosRefRed60},
    {70, cros_tokens::kCrosRefRed70},
    {80, cros_tokens::kCrosRefRed80},
    {90, cros_tokens::kCrosRefRed90},
    {95, cros_tokens::kCrosRefRed95},
    {99, cros_tokens::kCrosRefRed99},
    {100, cros_tokens::kCrosRefRed100}};

constexpr std::array<std::pair<int, ui::ColorId>, 13> kGreenIds{
    std::make_pair(0, cros_tokens::kCrosRefGreen0),
    {10, cros_tokens::kCrosRefGreen10},
    {20, cros_tokens::kCrosRefGreen20},
    {30, cros_tokens::kCrosRefGreen30},
    {40, cros_tokens::kCrosRefGreen40},
    {50, cros_tokens::kCrosRefGreen50},
    {60, cros_tokens::kCrosRefGreen60},
    {70, cros_tokens::kCrosRefGreen70},
    {80, cros_tokens::kCrosRefGreen80},
    {90, cros_tokens::kCrosRefGreen90},
    {95, cros_tokens::kCrosRefGreen95},
    {99, cros_tokens::kCrosRefGreen99},
    {100, cros_tokens::kCrosRefGreen100}};

constexpr std::array<std::pair<int, ui::ColorId>, 13> kYellowIds{
    std::make_pair(0, cros_tokens::kCrosRefYellow0),
    {10, cros_tokens::kCrosRefYellow10},
    {20, cros_tokens::kCrosRefYellow20},
    {30, cros_tokens::kCrosRefYellow30},
    {40, cros_tokens::kCrosRefYellow40},
    {50, cros_tokens::kCrosRefYellow50},
    {60, cros_tokens::kCrosRefYellow60},
    {70, cros_tokens::kCrosRefYellow70},
    {80, cros_tokens::kCrosRefYellow80},
    {90, cros_tokens::kCrosRefYellow90},
    {95, cros_tokens::kCrosRefYellow95},
    {99, cros_tokens::kCrosRefYellow99},
    {100, cros_tokens::kCrosRefYellow100}};

constexpr std::array<std::pair<int, ui::ColorId>, 13> kBlueIds{
    std::make_pair(0, cros_tokens::kCrosRefBlue0),
    {10, cros_tokens::kCrosRefBlue10},
    {20, cros_tokens::kCrosRefBlue20},
    {30, cros_tokens::kCrosRefBlue30},
    {40, cros_tokens::kCrosRefBlue40},
    {50, cros_tokens::kCrosRefBlue50},
    {60, cros_tokens::kCrosRefBlue60},
    {70, cros_tokens::kCrosRefBlue70},
    {80, cros_tokens::kCrosRefBlue80},
    {90, cros_tokens::kCrosRefBlue90},
    {95, cros_tokens::kCrosRefBlue95},
    {99, cros_tokens::kCrosRefBlue99},
    {100, cros_tokens::kCrosRefBlue100}};

// For the ColorId in `data`, insert the color value from `palette` for the
// associated luma in `data`.
void InsertIntoMixer(ui::ColorMixer& mixer,
                     const TonalPalette& palette,
                     const std::array<std::pair<int, ui::ColorId>, 13>& data) {
  for (const auto& entry : data) {
    // Under the hood, SkColor is in ARGB like Argb from the material color
    // utilities library.
    SkColor color = palette.get(entry.first);
    ui::ColorId color_id = entry.second;

    mixer[color_id] = {color};
  }
}

}  // namespace

// Returns the hue angle for `seed_color`.
int HueAngle(SkColor seed_color) {
  SkScalar hsv[3];
  SkColorToHSV(seed_color, hsv);

  // Hue is in degrees.
  int hue_angle = static_cast<int>(hsv[0]);
  DCHECK_LE(hue_angle, 360);
  DCHECK_GE(hue_angle, 0);

  return hue_angle;
}

void AddHarmonizedColors(ui::ColorMixer& mixer,
                         const ui::ColorProviderKey& key) {
  // Zip the arrays into a map indexed by the angle lower bound.
  static const base::flat_map<int, HarmonizedSeeds> kSeedMap = MakeMap();

  HarmonizedSeeds seeds;
  if (!key.user_color) {
    // If there's no seed color, always use the last one.
    auto last = kSeedMap.end();
    --last;
    seeds = last->second;
  } else {
    int angle = HueAngle(*key.user_color);
    DCHECK_LT(angle, 360);
    auto iter = kSeedMap.upper_bound(angle);
    DCHECK(iter != kSeedMap.end());
    seeds = iter->second;
  }

  InsertIntoMixer(mixer, TonalPalette(seeds.red), kRedIds);
  InsertIntoMixer(mixer, TonalPalette(seeds.green), kGreenIds);
  InsertIntoMixer(mixer, TonalPalette(seeds.blue), kBlueIds);
  InsertIntoMixer(mixer, TonalPalette(seeds.yellow), kYellowIds);
}

}  // namespace ash
