// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_palette_controller.h"

#include <ostream>

#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

namespace {

const char kUser[] = "user@gmail.com";
const AccountId kAccountId = AccountId::FromUserEmailGaiaId(kUser, kUser);

// A nice magenta that is in the acceptable lightness range for dark and light.
// Hue: 281, Saturation: 100, Lightness: 50%.
constexpr SkColor kKMeanColor = SkColorSetRGB(0xae, 0x00, 0xff);

class MockPaletteObserver : public ColorPaletteController::Observer {
 public:
  MOCK_METHOD(void,
              OnColorPaletteChanging,
              (const ColorPaletteSeed& seed),
              (override));
};

// A helper to record updates to a `ui::NativeTheme`.
class TestObserver : public ui::NativeThemeObserver {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override {
    last_theme_ = observed_theme;
    call_count_++;
  }

  int call_count() { return call_count_; }

  ui::NativeTheme* last_theme() { return last_theme_; }

 private:
  ui::NativeTheme* last_theme_ = nullptr;
  int call_count_ = 0;
};

// Matches a `SampleColorScheme` based on the `scheme` and `primary` attributes.
MATCHER_P2(Sample,
           scheme,
           primary_color,
           base::StringPrintf("where scheme is %u and the primary color is %x",
                              static_cast<int>(scheme),
                              primary_color)) {
  return arg.scheme == scheme && arg.primary == primary_color;
}

}  // namespace

class ColorPaletteControllerTest : public NoSessionAshTestBase {
 public:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    GetSessionControllerClient()->Reset();
    GetSessionControllerClient()->AddUserSession(kAccountId, kUser);
    wallpaper_controller_ = Shell::Get()->wallpaper_controller();
    color_palette_controller_ = Shell::Get()->color_palette_controller();

    dark_light_mode_controller_ = Shell::Get()->dark_light_mode_controller();
    // Fix dark mode as off.
    dark_light_mode_controller_->SetDarkModeEnabledForTest(false);
  }

  void TearDown() override { NoSessionAshTestBase::TearDown(); }

  ColorPaletteController* color_palette_controller() {
    return color_palette_controller_;
  }

  DarkLightModeControllerImpl* dark_light_controller() {
    return dark_light_mode_controller_;
  }

  WallpaperControllerImpl* wallpaper_controller() {
    return wallpaper_controller_;
  }

 private:
  base::raw_ptr<DarkLightModeControllerImpl>
      dark_light_mode_controller_;                               // unowned
  base::raw_ptr<WallpaperControllerImpl> wallpaper_controller_;  // unowned

  base::raw_ptr<ColorPaletteController> color_palette_controller_;
};

TEST_F(ColorPaletteControllerTest, ExpectedEmptyValues) {
  EXPECT_EQ(ColorScheme::kTonalSpot,
            color_palette_controller()->GetColorScheme(kAccountId));
  EXPECT_EQ(absl::nullopt,
            color_palette_controller()->GetStaticColor(kAccountId));
}

TEST_F(ColorPaletteControllerTest, SetColorScheme_JellyDisabled_AlwaysTonal) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(chromeos::features::kJelly);
  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors({}, kKMeanColor, SK_ColorWHITE));

  color_palette_controller()->SetColorScheme(ColorScheme::kStatic, kAccountId,
                                             base::DoNothing());
  EXPECT_EQ(
      ColorScheme::kTonalSpot,
      color_palette_controller()->GetColorPaletteSeed(kAccountId)->scheme);

  color_palette_controller()->SetColorScheme(ColorScheme::kExpressive,
                                             kAccountId, base::DoNothing());
  EXPECT_EQ(
      ColorScheme::kTonalSpot,
      color_palette_controller()->GetColorPaletteSeed(kAccountId)->scheme);
}

TEST_F(ColorPaletteControllerTest, SetColorScheme) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors({}, kKMeanColor, SK_ColorWHITE));

  ColorScheme color_scheme = ColorScheme::kExpressive;

  color_palette_controller()->SetColorScheme(color_scheme, kAccountId,
                                             base::DoNothing());

  EXPECT_EQ(color_scheme,
            color_palette_controller()->GetColorScheme(kAccountId));
  EXPECT_EQ(absl::nullopt,
            color_palette_controller()->GetStaticColor(kAccountId));
  auto color_palette_seed =
      color_palette_controller()->GetColorPaletteSeed(kAccountId);
  EXPECT_EQ(color_scheme, color_palette_seed->scheme);
}

TEST_F(ColorPaletteControllerTest, SetStaticColor) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
  SkColor static_color = SK_ColorGRAY;

  color_palette_controller()->SetStaticColor(static_color, kAccountId,
                                             base::DoNothing());

  EXPECT_EQ(static_color,
            color_palette_controller()->GetStaticColor(kAccountId));
  EXPECT_EQ(ColorScheme::kStatic,
            color_palette_controller()->GetColorScheme(kAccountId));
  auto color_palette_seed =
      color_palette_controller()->GetColorPaletteSeed(kAccountId);
  EXPECT_EQ(ColorScheme::kStatic, color_palette_seed->scheme);
  EXPECT_EQ(static_color, color_palette_seed->seed_color);
}

// If the Jelly flag is off, we always return the KMeans color from the
// wallpaper controller regardless of scheme.
TEST_F(ColorPaletteControllerTest, SetStaticColor_JellyDisabled_AlwaysKMeans) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(chromeos::features::kJelly);

  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors({}, kKMeanColor, SK_ColorWHITE));

  color_palette_controller()->SetColorScheme(ColorScheme::kStatic, kAccountId,
                                             base::DoNothing());
  color_palette_controller()->SetStaticColor(SK_ColorRED, kAccountId,
                                             base::DoNothing());

  // TODO(skau): Check that this matches kKMean after color blending has been
  // moved.
  EXPECT_NE(
      SK_ColorWHITE,
      color_palette_controller()->GetColorPaletteSeed(kAccountId)->seed_color);
}

TEST_F(ColorPaletteControllerTest, ColorModeTriggersObserver) {
  // Initialize Dark mode to a known state.
  dark_light_controller()->SetDarkModeEnabledForTest(false);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());

  EXPECT_CALL(observer, OnColorPaletteChanging(testing::Field(
                            &ColorPaletteSeed::color_mode,
                            ui::ColorProviderManager::ColorMode::kDark)))
      .Times(1);
  dark_light_controller()->SetDarkModeEnabledForTest(true);
}

TEST_F(ColorPaletteControllerTest, NativeTheme_DarkModeChanged_JellyDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(chromeos::features::kJelly);

  // Set to a known state.
  dark_light_controller()->SetDarkModeEnabledForTest(true);
  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors({}, kKMeanColor, SK_ColorWHITE));

  TestObserver observer;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver> observation(
      &observer);
  observation.Observe(ui::NativeTheme::GetInstanceForNativeUi());

  dark_light_controller()->SetDarkModeEnabledForTest(false);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(1, observer.call_count());
  ASSERT_TRUE(observer.last_theme());
  EXPECT_EQ(ui::NativeTheme::ColorScheme::kLight,
            observer.last_theme()->GetDefaultSystemColorScheme());
  // TODO(skau): Check that this matches kKMean after color blending has been
  // moved.
  EXPECT_NE(SK_ColorWHITE, observer.last_theme()->user_color().value());
  // Pre-Jelly, this should always be TonalSpot.
  EXPECT_THAT(
      observer.last_theme()->scheme_variant(),
      testing::Optional(ui::ColorProviderManager::SchemeVariant::kTonalSpot));
}

TEST_F(ColorPaletteControllerTest, NativeTheme_DarkModeChanged_JellyEnabled) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);

  const SkColor kCelebiColor = SK_ColorBLUE;

  // Set to a known state.
  dark_light_controller()->SetDarkModeEnabledForTest(true);
  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors({}, SK_ColorWHITE, kCelebiColor));
  color_palette_controller()->SetColorScheme(ColorScheme::kVibrant, kAccountId,
                                             base::DoNothing());

  TestObserver observer;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver> observation(
      &observer);
  observation.Observe(ui::NativeTheme::GetInstanceForNativeUi());

  dark_light_controller()->SetDarkModeEnabledForTest(false);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(1, observer.call_count());
  ASSERT_TRUE(observer.last_theme());
  EXPECT_EQ(ui::NativeTheme::ColorScheme::kLight,
            observer.last_theme()->GetDefaultSystemColorScheme());
  EXPECT_EQ(kCelebiColor, observer.last_theme()->user_color().value());
  EXPECT_THAT(
      observer.last_theme()->scheme_variant(),
      testing::Optional(ui::ColorProviderManager::SchemeVariant::kVibrant));
}

// Emulates Dark mode changes on login screen that can result from pod
// selection.
TEST_F(ColorPaletteControllerTest, NativeTheme_DarkModeChanged_NoSession) {
  GetSessionControllerClient()->Reset();

  // Set to a known state.
  dark_light_controller()->SetDarkModeEnabledForTest(true);

  TestObserver observer;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver> observation(
      &observer);
  observation.Observe(ui::NativeTheme::GetInstanceForNativeUi());

  dark_light_controller()->SetDarkModeEnabledForTest(false);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(1, observer.call_count());
  ASSERT_TRUE(observer.last_theme());
  EXPECT_EQ(ui::NativeTheme::ColorScheme::kLight,
            observer.last_theme()->GetDefaultSystemColorScheme());
}

TEST_F(ColorPaletteControllerTest, GetSeedWithUnsetWallpaper) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);

  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.ResetCalculatedColors();

  // If we calculated wallpaper colors are unset, we can't produce a valid
  // seed.
  EXPECT_FALSE(color_palette_controller()->GetCurrentSeed().has_value());
}

TEST_F(ColorPaletteControllerTest, GenerateSampleScheme) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);

  SkColor seed = SkColorSetRGB(0xf5, 0x42, 0x45);  // Hue 359* Saturation 73%
                                                   // Vibrance 96%

  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors({}, SK_ColorWHITE, seed));

  const ColorScheme schemes[] = {ColorScheme::kExpressive,
                                 ColorScheme::kTonalSpot};
  std::vector<SampleColorScheme> results;
  base::RunLoop runner;
  color_palette_controller()->GenerateSampleColorSchemes(
      schemes,
      base::BindLambdaForTesting(
          [&results, &runner](const std::vector<SampleColorScheme>& samples) {
            results.insert(results.begin(), samples.begin(), samples.end());
            runner.Quit();
          }));

  runner.Run();
  EXPECT_THAT(
      results,
      testing::UnorderedElementsAre(
          Sample(ColorScheme::kTonalSpot, SkColorSetRGB(0x94, 0x47, 0x44)),
          Sample(ColorScheme::kExpressive, SkColorSetRGB(0x3b, 0x69, 0x30))));
}

TEST_F(ColorPaletteControllerTest, GenerateSampleScheme_AllValues_Teal) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);

  SkColor seed = SkColorSetRGB(0x00, 0xbf, 0x7f);  // Hue 160* Saturation 100%
                                                   // Vibrance 75%

  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors({}, SK_ColorWHITE, seed));

  const ColorScheme schemes[] = {ColorScheme::kVibrant};
  std::vector<SampleColorScheme> results;
  base::RunLoop runner;
  color_palette_controller()->GenerateSampleColorSchemes(
      schemes,
      base::BindLambdaForTesting(
          [&results, &runner](const std::vector<SampleColorScheme>& samples) {
            results.insert(results.begin(), samples.begin(), samples.end());
            runner.Quit();
          }));

  runner.Run();
  ASSERT_THAT(results, testing::SizeIs(1));
  auto& result = results.front();
  EXPECT_THAT(result, testing::Eq(SampleColorScheme{
                          .scheme = ColorScheme::kVibrant,
                          .primary = SkColorSetRGB(0x00, 0x6c, 0x46),
                          .secondary = SkColorSetRGB(0x4d, 0xff, 0xb2),
                          .tertiary = SkColorSetRGB(0xa8, 0xef, 0xef)}));
}

// Helper to print better matcher errors.
void PrintTo(const SampleColorScheme& scheme, std::ostream* os) {
  *os << base::StringPrintf(
      "SampleColorScheme(scheme: %u primary: %x secondary: %x tertiary: %x)",
      scheme.scheme, scheme.primary, scheme.secondary, scheme.tertiary);
}

}  // namespace ash
