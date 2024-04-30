// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_mixer.h"

#include <ostream>

#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/ref_color_mixer.h"
#include "ui/gfx/color_palette.h"

namespace {

struct ColorsTestCase {
  ui::ColorId color_id;
  SkColor expected_color;
};

// Returns a key with reasonable values.
ui::ColorProviderKey MakeColorProviderKey(SkColor seed_color) {
  ui::ColorProviderKey key;
  key.user_color = seed_color;
  key.color_mode = ui::ColorProviderKey::ColorMode::kLight;
  return key;
}

// Initialize ColorProvider for a given key.
void InitializeColorProvider(const ui::ColorProviderKey& key,
                             ui::ColorProvider& color_provider) {
  ui::AddRefColorMixer(&color_provider, key);
  // Roughly mimics the ColorMixer configuration for Ash.
  ash::AddCrosStylesColorMixer(&color_provider, key);
  ash::AddAshColorMixer(&color_provider, key);
}

// Initializes the `color_provider` with `seed_color` and the relevant
// ColorMixers.
void SetUpColorProvider(ui::ColorProvider& color_provider, SkColor seed_color) {
  auto key = MakeColorProviderKey(seed_color);
  InitializeColorProvider(key, color_provider);
}

class AshColorMixerTest : public testing::TestWithParam<ColorsTestCase> {
 public:
  AshColorMixerTest() = default;

  void SetUp() override {
    // Hue angle 217 degrees.
    SkColor seed_color = gfx::kGoogleBlue400;
    SetUpColorProvider(color_provider_, seed_color);
  }

  const ui::ColorProvider& color_provider() { return color_provider_; }

 private:
  ui::ColorProvider color_provider_;
};

// Provides nicer logging for mismatched colors.
testing::AssertionResult AssertColorsMatch(const char* m_expr,
                                           const char* n_expr,
                                           SkColor m,
                                           SkColor n) {
  if (m == n) {
    return testing::AssertionSuccess();
  }

  return testing::AssertionFailure()
         << ui::SkColorName(m) << " (actual) and " << ui::SkColorName(n)
         << " (expected) do not match";
}

// Tests that one set of harmonized colors are correct.
TEST_P(AshColorMixerTest, HarmonizedColors) {
  const auto& test_case = GetParam();
  // Prints the color name as the CSS identifier for easier debugging.
  EXPECT_PRED_FORMAT2(AssertColorsMatch,
                      color_provider().GetColor(test_case.color_id),
                      test_case.expected_color)
      << " for " << cros_tokens::ColorIdName(test_case.color_id);
}

INSTANTIATE_TEST_SUITE_P(
    ColorTests,
    AshColorMixerTest,
    testing::ValuesIn<ColorsTestCase>({
        // Green
        {cros_tokens::kCrosRefGreen0, SkColorSetRGB(0, 0, 0)},
        {cros_tokens::kCrosRefGreen10, SkColorSetRGB(0, 33, 14)},
        {cros_tokens::kCrosRefGreen20, SkColorSetRGB(0, 57, 28)},
        {cros_tokens::kCrosRefGreen30, SkColorSetRGB(0, 82, 43)},
        {cros_tokens::kCrosRefGreen40, SkColorSetRGB(0, 109, 59)},
        {cros_tokens::kCrosRefGreen50, SkColorSetRGB(0, 137, 76)},
        {cros_tokens::kCrosRefGreen60, SkColorSetRGB(47, 164, 99)},
        {cros_tokens::kCrosRefGreen70, SkColorSetRGB(79, 192, 123)},
        {cros_tokens::kCrosRefGreen80, SkColorSetRGB(109, 220, 148)},
        {cros_tokens::kCrosRefGreen90, SkColorSetRGB(137, 249, 175)},
        {cros_tokens::kCrosRefGreen95, SkColorSetRGB(194, 255, 208)},
        {cros_tokens::kCrosRefGreen99, SkColorSetRGB(245, 255, 243)},
        {cros_tokens::kCrosRefGreen100, SkColorSetRGB(255, 255, 255)},

        // Red
        {cros_tokens::kCrosRefRed0, SkColorSetRGB(0, 0, 0)},
        {cros_tokens::kCrosRefRed10, SkColorSetRGB(58, 11, 0)},
        {cros_tokens::kCrosRefRed20, SkColorSetRGB(94, 23, 0)},
        {cros_tokens::kCrosRefRed30, SkColorSetRGB(133, 36, 0)},
        {cros_tokens::kCrosRefRed40, SkColorSetRGB(171, 53, 8)},
        {cros_tokens::kCrosRefRed50, SkColorSetRGB(205, 77, 34)},
        {cros_tokens::kCrosRefRed60, SkColorSetRGB(239, 102, 56)},
        {cros_tokens::kCrosRefRed70, SkColorSetRGB(255, 139, 102)},
        {cros_tokens::kCrosRefRed80, SkColorSetRGB(255, 181, 158)},
        {cros_tokens::kCrosRefRed90, SkColorSetRGB(255, 219, 208)},
        {cros_tokens::kCrosRefRed95, SkColorSetRGB(255, 237, 232)},
        {cros_tokens::kCrosRefRed99, SkColorSetRGB(255, 251, 255)},
        {cros_tokens::kCrosRefRed100, SkColorSetRGB(255, 255, 255)},

        // Yellow
        {cros_tokens::kCrosRefYellow0, SkColorSetRGB(0, 0, 0)},
        {cros_tokens::kCrosRefYellow10, SkColorSetRGB(37, 26, 0)},
        {cros_tokens::kCrosRefYellow20, SkColorSetRGB(63, 46, 0)},
        {cros_tokens::kCrosRefYellow30, SkColorSetRGB(90, 67, 0)},
        {cros_tokens::kCrosRefYellow40, SkColorSetRGB(119, 90, 0)},
        {cros_tokens::kCrosRefYellow50, SkColorSetRGB(150, 114, 0)},
        {cros_tokens::kCrosRefYellow60, SkColorSetRGB(181, 138, 0)},
        {cros_tokens::kCrosRefYellow70, SkColorSetRGB(214, 164, 0)},
        {cros_tokens::kCrosRefYellow80, SkColorSetRGB(247, 190, 0)},
        {cros_tokens::kCrosRefYellow90, SkColorSetRGB(255, 223, 153)},
        {cros_tokens::kCrosRefYellow95, SkColorSetRGB(255, 239, 210)},
        {cros_tokens::kCrosRefYellow99, SkColorSetRGB(255, 251, 255)},
        {cros_tokens::kCrosRefYellow100, SkColorSetRGB(255, 255, 255)},

        // Blue
        {cros_tokens::kCrosRefBlue0, SkColorSetRGB(0, 0, 0)},
        {cros_tokens::kCrosRefBlue10, SkColorSetRGB(0, 31, 39)},
        {cros_tokens::kCrosRefBlue20, SkColorSetRGB(0, 54, 66)},
        {cros_tokens::kCrosRefBlue30, SkColorSetRGB(0, 78, 95)},
        {cros_tokens::kCrosRefBlue40, SkColorSetRGB(0, 103, 125)},
        {cros_tokens::kCrosRefBlue50, SkColorSetRGB(0, 130, 157)},
        {cros_tokens::kCrosRefBlue60, SkColorSetRGB(55, 156, 184)},
        {cros_tokens::kCrosRefBlue70, SkColorSetRGB(87, 183, 212)},
        {cros_tokens::kCrosRefBlue80, SkColorSetRGB(117, 211, 240)},
        {cros_tokens::kCrosRefBlue90, SkColorSetRGB(179, 235, 255)},
        {cros_tokens::kCrosRefBlue95, SkColorSetRGB(219, 245, 255)},
        {cros_tokens::kCrosRefBlue99, SkColorSetRGB(249, 253, 255)},
        {cros_tokens::kCrosRefBlue100, SkColorSetRGB(255, 255, 255)},
    }));

struct HuesTestCase {
  SkColor seed_color;
  ColorsTestCase colors;
};

class AshColorMixerHueAngleTest : public testing::TestWithParam<HuesTestCase> {
 public:
  AshColorMixerHueAngleTest() = default;

  void SetUp() override {
    color_provider_ = std::make_unique<ui::ColorProvider>();
  }

  void TearDown() override { color_provider_.reset(); }

  ui::ColorProvider& color_provider() { return *color_provider_; }

 private:
  std::unique_ptr<ui::ColorProvider> color_provider_;
};

TEST_P(AshColorMixerHueAngleTest, Hues) {
  const HuesTestCase& test_case = GetParam();
  SetUpColorProvider(color_provider(), test_case.seed_color);

  const ColorsTestCase& colors = test_case.colors;
  EXPECT_PRED_FORMAT2(AssertColorsMatch,
                      color_provider().GetColor(colors.color_id),
                      colors.expected_color)
      << " using seed " << ui::SkColorName(test_case.seed_color) << " for "
      << cros_tokens::ColorIdName(colors.color_id);
}

constexpr SkColor kRed = SkColorSetRGB(0xFF, 0x00, 0x00);      // Hue 0
constexpr SkColor kMustard = SkColorSetRGB(0xFF, 0xD5, 0x00);  // Hue 50
constexpr SkColor kTeal = SkColorSetRGB(0x00, 0xFF, 0xAA);     // Hue 160
constexpr SkColor kOffRed = SkColorSetRGB(0xFF, 0x00, 0x04);   // Hue 359

INSTANTIATE_TEST_SUITE_P(
    HueTests,
    AshColorMixerHueAngleTest,
    testing::ValuesIn<HuesTestCase>({
        // Hue angle 0
        {kRed, {cros_tokens::kCrosRefGreen40, SkColorSetRGB(46, 108, 0)}},
        {kRed, {cros_tokens::kCrosRefRed40, SkColorSetRGB(179, 42, 25)}},
        {kRed, {cros_tokens::kCrosRefYellow40, SkColorSetRGB(134, 83, 0)}},
        {kRed, {cros_tokens::kCrosRefBlue40, SkColorSetRGB(63, 90, 169)}},
        {kRed,
         {cros_tokens::kCrosRefSparkleAnalog40,
          SkColorSetRGB(0x6f, 0x46, 0xb9)}},
        {kRed,
         {cros_tokens::kCrosRefSparkleMuted40,
          SkColorSetRGB(0x5f, 0x57, 0x8f)}},
        {kRed,
         {cros_tokens::kCrosRefSparkleComplement40,
          SkColorSetRGB(0x40, 0x67, 0x43)}},

        // Hue angle 50
        {kMustard, {cros_tokens::kCrosRefGreen40, SkColorSetRGB(26, 109, 0)}},
        {kMustard, {cros_tokens::kCrosRefRed40, SkColorSetRGB(163, 62, 0)}},
        {kMustard, {cros_tokens::kCrosRefYellow40, SkColorSetRGB(121, 89, 0)}},
        {kMustard, {cros_tokens::kCrosRefBlue40, SkColorSetRGB(0, 103, 125)}},
        {kMustard,
         {cros_tokens::kCrosRefSparkleAnalog40,
          SkColorSetRGB(0x4a, 0x51, 0xc3)}},
        {kMustard,
         {cros_tokens::kCrosRefSparkleMuted40,
          SkColorSetRGB(0x4c, 0x5c, 0x90)}},
        {kMustard,
         {cros_tokens::kCrosRefSparkleComplement40,
          SkColorSetRGB(0x30, 0x68, 0x54)}},

        // Hue angel 160
        {kTeal, {cros_tokens::kCrosRefGreen40, SkColorSetRGB(0, 109, 59)}},
        {kTeal, {cros_tokens::kCrosRefRed40, SkColorSetRGB(171, 53, 8)}},
        {kTeal, {cros_tokens::kCrosRefYellow40, SkColorSetRGB(119, 90, 0)}},
        {kTeal, {cros_tokens::kCrosRefBlue40, SkColorSetRGB(0, 103, 125)}},
        {kTeal,
         {cros_tokens::kCrosRefSparkleAnalog40,
          SkColorSetRGB(0x6f, 0x46, 0xb9)}},
        {kTeal,
         {cros_tokens::kCrosRefSparkleMuted40,
          SkColorSetRGB(0x5f, 0x57, 0x8f)}},
        {kTeal,
         {cros_tokens::kCrosRefSparkleComplement40,
          SkColorSetRGB(0x40, 0x67, 0x43)}},

        // Hue angle 359
        {kOffRed, {cros_tokens::kCrosRefGreen40, SkColorSetRGB(0, 108, 74)}},
        {kOffRed, {cros_tokens::kCrosRefRed40, SkColorSetRGB(189, 8, 55)}},
        {kOffRed, {cros_tokens::kCrosRefYellow40, SkColorSetRGB(130, 85, 0)}},
        {kOffRed, {cros_tokens::kCrosRefBlue40, SkColorSetRGB(63, 90, 169)}},
        {kOffRed,
         {cros_tokens::kCrosRefSparkleAnalog40,
          SkColorSetRGB(0x6f, 0x46, 0xb9)}},
        {kOffRed,
         {cros_tokens::kCrosRefSparkleMuted40,
          SkColorSetRGB(0x5f, 0x57, 0x8f)}},
        {kOffRed,
         {cros_tokens::kCrosRefSparkleComplement40,
          SkColorSetRGB(0x40, 0x67, 0x43)}},
    }));

class AshColorGamingColorsTest : public testing::Test {
 public:
  AshColorGamingColorsTest() = default;

  void SetUp() override {
    color_provider_ = std::make_unique<ui::ColorProvider>();
  }

  void TearDown() override { color_provider_.reset(); }

  ui::ColorProvider& color_provider() { return *color_provider_; }

  void SetUpColorProvider(SkColor seed_color,
                          ui::ColorProviderKey::SchemeVariant scheme,
                          ui::ColorProviderKey::ColorMode color_mode) {
    ui::ColorProviderKey key;
    key.user_color = seed_color;
    key.color_mode = color_mode;
    key.scheme_variant = scheme;

    InitializeColorProvider(key, color_provider());
  }

 private:
  std::unique_ptr<ui::ColorProvider> color_provider_;
};

// A random color
constexpr SkColor kGamingTestSeed = SkColorSetRGB(17, 220, 96);

TEST_F(AshColorGamingColorsTest, MatchesVibrant) {
  SetUpColorProvider(kGamingTestSeed,
                     ui::ColorProviderKey::SchemeVariant::kVibrant,
                     ui::ColorProviderKey::ColorMode::kLight);
  EXPECT_PRED_FORMAT2(
      AssertColorsMatch,
      color_provider().GetColor(
          cros_tokens::kCrosSysGamingControlButtonDefault),
      color_provider().GetColor(cros_tokens::kCrosRefPrimary40));
  EXPECT_PRED_FORMAT2(
      AssertColorsMatch,
      color_provider().GetColor(cros_tokens::kCrosSysGamingControlButtonHover),
      color_provider().GetColor(cros_tokens::kCrosRefPrimary50));
  EXPECT_PRED_FORMAT2(
      AssertColorsMatch,
      color_provider().GetColor(
          cros_tokens::kCrosSysGamingControlButtonBorderHover),
      color_provider().GetColor(cros_tokens::kCrosRefPrimary80));
}

TEST_F(AshColorGamingColorsTest, DoesNotMatchTonal) {
  SetUpColorProvider(kGamingTestSeed,
                     ui::ColorProviderKey::SchemeVariant::kTonalSpot,
                     ui::ColorProviderKey::ColorMode::kLight);

  EXPECT_NE(color_provider().GetColor(
                cros_tokens::kCrosSysGamingControlButtonDefault),
            color_provider().GetColor(cros_tokens::kCrosRefPrimary40));
}

}  // namespace
