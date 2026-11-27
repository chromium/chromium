// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_utils.h"

#include <optional>
#include <tuple>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/color/color_provider_key.h"

namespace {

TEST(ThemeServiceUtilsTest, EnumConversions) {
  constexpr std::tuple<ui::mojom::BrowserColorVariant,
                       sync_pb::UserColorTheme::BrowserColorVariant,
                       std::optional<ui::ColorProviderKey::SchemeVariant>>
      kVariants[] = {
          {ui::mojom::BrowserColorVariant::kSystem,
           sync_pb::UserColorTheme_BrowserColorVariant_SYSTEM, std::nullopt},
          {ui::mojom::BrowserColorVariant::kTonalSpot,
           sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT,
           ui::ColorProviderKey::SchemeVariant::kTonalSpot},
          {ui::mojom::BrowserColorVariant::kNeutral,
           sync_pb::UserColorTheme_BrowserColorVariant_NEUTRAL,
           ui::ColorProviderKey::SchemeVariant::kNeutral},
          {ui::mojom::BrowserColorVariant::kVibrant,
           sync_pb::UserColorTheme_BrowserColorVariant_VIBRANT,
           ui::ColorProviderKey::SchemeVariant::kVibrant},
          {ui::mojom::BrowserColorVariant::kExpressive,
           sync_pb::UserColorTheme_BrowserColorVariant_EXPRESSIVE,
           ui::ColorProviderKey::SchemeVariant::kExpressive},
      };
  for (const auto& [mojom_variant, proto_variant, scheme_variant] : kVariants) {
    EXPECT_EQ(BrowserColorVariantToProtoEnum(mojom_variant), proto_variant);
    EXPECT_EQ(ProtoEnumToBrowserColorVariant(proto_variant), mojom_variant);
    EXPECT_EQ(BrowserColorVariantToSchemeVariant(mojom_variant),
              scheme_variant);
  }
  EXPECT_EQ(
      ProtoEnumToBrowserColorVariant(
          sync_pb::
              UserColorTheme_BrowserColorVariant_BROWSER_COLOR_VARIANT_UNSPECIFIED),
      ui::mojom::BrowserColorVariant::kSystem);

  constexpr std::pair<ThemeService::BrowserColorScheme,
                      sync_pb::ThemeSpecifics::BrowserColorScheme>
      kSchemes[] = {
          {ThemeService::BrowserColorScheme::kSystem,
           sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM},
          {ThemeService::BrowserColorScheme::kLight,
           sync_pb::ThemeSpecifics_BrowserColorScheme_LIGHT},
          {ThemeService::BrowserColorScheme::kDark,
           sync_pb::ThemeSpecifics_BrowserColorScheme_DARK},
      };
  for (const auto& [scheme, proto_scheme] : kSchemes) {
    EXPECT_EQ(BrowserColorSchemeToProtoEnum(scheme), proto_scheme);
    EXPECT_EQ(ProtoEnumToBrowserColorScheme(proto_scheme), scheme);
  }
  EXPECT_EQ(
      ProtoEnumToBrowserColorScheme(
          sync_pb::
              ThemeSpecifics_BrowserColorScheme_BROWSER_COLOR_SCHEME_UNSPECIFIED),
      ThemeService::BrowserColorScheme::kSystem);
}

TEST(ThemeServiceUtilsTest, GenerateAndroidCustomThemeColors) {
  constexpr SkColor kBlueSeed = SkColorSetRGB(0x8C, 0xAB, 0xE4);
  AndroidCustomThemeColors tonal_spot = GenerateAndroidCustomThemeColors(
      kBlueSeed, ui::mojom::BrowserColorVariant::kTonalSpot);
  // Expect Tone 67 (`#81A4E2`) rather than Tone 80 (`#AAC7FF`) for
  // `primary_dark`.
  EXPECT_EQ(tonal_spot, (AndroidCustomThemeColors{
                            .primary_light = SkColorSetRGB(0x3A, 0x5E, 0x98),
                            .primary_dark = SkColorSetRGB(0x81, 0xA4, 0xE2),
                            .background_light = SkColorSetRGB(0xF9, 0xF9, 0xFF),
                            .background_dark = SkColorSetRGB(0x33, 0x3C, 0x4D),
                        }));

  // `kSystem` defaults to `kTonalSpot` with themed backgrounds.
  EXPECT_EQ(GenerateAndroidCustomThemeColors(
                kBlueSeed, ui::mojom::BrowserColorVariant::kSystem),
            tonal_spot);

  // Other variants generate distinct palettes.
  EXPECT_NE(GenerateAndroidCustomThemeColors(
                kBlueSeed, ui::mojom::BrowserColorVariant::kNeutral),
            tonal_spot);
  EXPECT_NE(GenerateAndroidCustomThemeColors(
                kBlueSeed, ui::mojom::BrowserColorVariant::kVibrant),
            tonal_spot);
  EXPECT_NE(GenerateAndroidCustomThemeColors(
                kBlueSeed, ui::mojom::BrowserColorVariant::kExpressive),
            tonal_spot);

  // Pure black seed color steps to (1, 1, 1) to produce a blue primary rather
  // than libmonet's pink fallback (`#904760` / `#E08AA5`).
  AndroidCustomThemeColors black_colors = GenerateAndroidCustomThemeColors(
      SK_ColorBLACK, ui::mojom::BrowserColorVariant::kTonalSpot);
  EXPECT_EQ(black_colors,
            (AndroidCustomThemeColors{
                .primary_light = SkColorSetRGB(0x00, 0x68, 0x74),
                .primary_dark = SkColorSetRGB(0x4D, 0xB1, 0xBF),
                .background_light = SkColorSetRGB(0xF5, 0xFA, 0xFB),
                .background_dark = SkColorSetRGB(0x27, 0x3F, 0x43),
            }));
  EXPECT_EQ(black_colors, GenerateAndroidCustomThemeColors(
                              SkColorSetRGB(0x01, 0x01, 0x01),
                              ui::mojom::BrowserColorVariant::kTonalSpot));
}

}  // namespace
