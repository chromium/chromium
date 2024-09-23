// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_palette_controller.h"

#include <ostream>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/personalization_app/time_of_day_test_utils.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/color_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/mojom/color_scheme.mojom-shared.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_manager/known_user.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

const char kUser[] = "user@gmail.com";
const AccountId kAccountId = AccountId::FromUserEmailGaiaId(kUser, kUser);
const style::mojom::ColorScheme kLocalColorScheme =
    style::mojom::ColorScheme::kVibrant;
const style::mojom::ColorScheme kDefaultColorScheme =
    style::mojom::ColorScheme::kTonalSpot;
const SkColor kCelebiColor = gfx::kGoogleBlue400;

// Returns a wallpaper info that captures the time of day wallpaper.
WallpaperInfo CreateTimeOfDayWallpaperInfo() {
  WallpaperInfo info =
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_STRETCH,
                    WallpaperType::kDefault, base::Time::Now().LocalMidnight());
  info.collection_id = wallpaper_constants::kTimeOfDayWallpaperCollectionId;
  info.asset_id = 1;
  info.unit_id = 1;
  return info;
}

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
  raw_ptr<ui::NativeTheme> last_theme_ = nullptr;
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

  void UpdateWallpaperColor(SkColor color) {
    WallpaperControllerTestApi wallpaper(wallpaper_controller());
    wallpaper.SetCalculatedColors(
        WallpaperCalculatedColors(kKMeanColor, color));
    base::RunLoop().RunUntilIdle();
  }

  void SetUseKMeansPref(bool value) {
    PrefService* pref_service =
        Shell::Get()->session_controller()->GetUserPrefServiceForUser(
            kAccountId);
    pref_service->SetBoolean(prefs::kDynamicColorUseKMeans, value);
  }

 private:
  raw_ptr<DarkLightModeControllerImpl, DanglingUntriaged>
      dark_light_mode_controller_;  // unowned
  raw_ptr<WallpaperControllerImpl, DanglingUntriaged>
      wallpaper_controller_;  // unowned

  raw_ptr<ColorPaletteController, DanglingUntriaged> color_palette_controller_;
};

TEST_F(ColorPaletteControllerTest, ExpectedEmptyValues) {
  EXPECT_EQ(kDefaultColorScheme,
            color_palette_controller()->GetColorScheme(kAccountId));
  EXPECT_EQ(std::nullopt,
            color_palette_controller()->GetStaticColor(kAccountId));
}

// Verifies that when the TimeOfDayWallpaper feature is active but the wallpaper
// isn't a Time Of day wallpaper, the default color scheme is TonalSpot.
TEST_F(ColorPaletteControllerTest,
       ExpectedColorScheme_TimeOfDay_UsesDefaultScheme) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      personalization_app::GetTimeOfDayEnabledFeatures(), {});
  EXPECT_EQ(kDefaultColorScheme,
            color_palette_controller()->GetColorScheme(kAccountId));
}

TEST_F(ColorPaletteControllerTest, SetColorScheme) {
  SimulateUserLogin(kAccountId);
  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors(kKMeanColor, SK_ColorWHITE));
  const style::mojom::ColorScheme color_scheme =
      style::mojom::ColorScheme::kExpressive;

  color_palette_controller()->SetColorScheme(color_scheme, kAccountId,
                                             base::DoNothing());

  EXPECT_EQ(color_scheme,
            color_palette_controller()->GetColorScheme(kAccountId));
  EXPECT_EQ(std::nullopt,
            color_palette_controller()->GetStaticColor(kAccountId));
  auto color_palette_seed =
      color_palette_controller()->GetColorPaletteSeed(kAccountId);
  EXPECT_EQ(color_scheme, color_palette_seed->scheme);
  // Verify that the color scheme was saved to local state.
  auto local_color_scheme =
      user_manager::KnownUser(local_state())
          .FindIntPath(kAccountId, prefs::kDynamicColorColorScheme);
  EXPECT_EQ(color_scheme,
            static_cast<style::mojom::ColorScheme>(local_color_scheme.value()));
}

TEST_F(ColorPaletteControllerTest, SetStaticColor) {
  SimulateUserLogin(kAccountId);
  const SkColor static_color = SK_ColorGRAY;

  color_palette_controller()->SetStaticColor(static_color, kAccountId,
                                             base::DoNothing());

  EXPECT_EQ(static_color,
            color_palette_controller()->GetStaticColor(kAccountId));
  EXPECT_EQ(style::mojom::ColorScheme::kStatic,
            color_palette_controller()->GetColorScheme(kAccountId));
  auto color_palette_seed =
      color_palette_controller()->GetColorPaletteSeed(kAccountId);
  EXPECT_EQ(style::mojom::ColorScheme::kStatic, color_palette_seed->scheme);
  EXPECT_EQ(static_color, color_palette_seed->seed_color);
  auto local_color_scheme =
      user_manager::KnownUser(local_state())
          .FindIntPath(kAccountId, prefs::kDynamicColorColorScheme);
  EXPECT_EQ(style::mojom::ColorScheme::kStatic,
            static_cast<style::mojom::ColorScheme>(local_color_scheme.value()));
  const base::Value* value =
      user_manager::KnownUser(local_state())
          .FindPath(kAccountId, prefs::kDynamicColorSeedColor);
  // Verify that the color was saved to local state.
  const auto local_static_color = base::ValueToInt64(value);
  EXPECT_EQ(static_color, static_cast<SkColor>(local_static_color.value()));
}

TEST_F(ColorPaletteControllerTest, UpdateColorScheme_NotifiesObserver) {
  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kVibrant, kAccountId, base::DoNothing());
  SimulateUserLogin(kAccountId);
  UpdateWallpaperColor(SK_ColorBLUE);
  const style::mojom::ColorScheme color_scheme =
      style::mojom::ColorScheme::kExpressive;
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetUserPrefServiceForUser(kAccountId);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer, OnColorPaletteChanging(testing::Field(
                            &ColorPaletteSeed::scheme, color_scheme)))
      .Times(1);

  pref_service->SetInteger(prefs::kDynamicColorColorScheme,
                           static_cast<int>(color_scheme));
  task_environment()->RunUntilIdle();
}

TEST_F(ColorPaletteControllerTest, UpdateStaticColor_NotifiesObserver) {
  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kVibrant, kAccountId, base::DoNothing());
  SimulateUserLogin(kAccountId);
  color_palette_controller()->SetStaticColor(SK_ColorRED, kAccountId,
                                             base::DoNothing());
  UpdateWallpaperColor(SK_ColorBLUE);
  const SkColor static_color = SK_ColorGRAY;
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetUserPrefServiceForUser(kAccountId);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer, OnColorPaletteChanging(testing::Field(
                            &ColorPaletteSeed::seed_color, static_color)))
      .Times(1);

  pref_service->SetUint64(prefs::kDynamicColorSeedColor, static_color);
  task_environment()->RunUntilIdle();
}

TEST_F(ColorPaletteControllerTest, UpdateUseKMeans_NotifiesObserver) {
  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kTonalSpot, kAccountId, base::DoNothing());
  SetUseKMeansPref(true);
  UpdateWallpaperColor(kCelebiColor);
  SimulateUserLogin(kAccountId);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer,
              OnColorPaletteChanging(testing::AllOf(
                  testing::Field(&ColorPaletteSeed::scheme,
                                 style::mojom::ColorScheme::kTonalSpot),
                  testing::Field(&ColorPaletteSeed::seed_color, kCelebiColor))))
      .Times(1);

  SetUseKMeansPref(false);
  task_environment()->RunUntilIdle();
}

TEST_F(ColorPaletteControllerTest, ColorModeTriggersObserver) {
  // A seed color needs to be present for the observer to trigger.
  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors(kKMeanColor, SK_ColorWHITE));

  // Initialize Dark mode to a known state.
  dark_light_controller()->SetDarkModeEnabledForTest(false);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());

  EXPECT_CALL(observer, OnColorPaletteChanging(testing::Field(
                            &ColorPaletteSeed::color_mode,
                            ui::ColorProviderKey::ColorMode::kDark)))
      .Times(1);
  dark_light_controller()->SetDarkModeEnabledForTest(true);
}

TEST_F(ColorPaletteControllerTest, NativeTheme_DarkModeChanged) {
  // Set to a known state.
  dark_light_controller()->SetDarkModeEnabledForTest(true);
  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors(SK_ColorWHITE, kCelebiColor));
  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kVibrant, kAccountId, base::DoNothing());

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
  EXPECT_THAT(observer.last_theme()->scheme_variant(),
              testing::Optional(ui::ColorProviderKey::SchemeVariant::kVibrant));
}

TEST_F(ColorPaletteControllerTest, GetSeedWithUnsetWallpaper) {
  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.ResetCalculatedColors();

  // If we calculated wallpaper colors are unset, we can't produce a valid
  // seed.
  EXPECT_FALSE(color_palette_controller()->GetCurrentSeed().has_value());
}

TEST_F(ColorPaletteControllerTest, GenerateSampleScheme) {
  SimulateUserLogin(kAccountId);
  SetUseKMeansPref(false);

  SkColor seed = SkColorSetRGB(0xf5, 0x42, 0x45);  // Hue 359* Saturation 73%
                                                   // Vibrance 96%

  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(WallpaperCalculatedColors(SK_ColorWHITE, seed));

  const style::mojom::ColorScheme schemes[] = {
      style::mojom::ColorScheme::kExpressive,
      style::mojom::ColorScheme::kTonalSpot};
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
  EXPECT_THAT(results, testing::UnorderedElementsAre(
                           Sample(style::mojom::ColorScheme::kTonalSpot,
                                  SkColorSetRGB(0xff, 0xb3, 0xae)),
                           Sample(style::mojom::ColorScheme::kExpressive,
                                  SkColorSetRGB(0xc8, 0xbf, 0xff))));
}

TEST_F(ColorPaletteControllerTest, GenerateSampleScheme_AllValues_Teal) {
  SkColor seed = SkColorSetRGB(0x00, 0xbf, 0x7f);  // Hue 160* Saturation 100%
                                                   // Vibrance 75%

  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(WallpaperCalculatedColors(SK_ColorWHITE, seed));

  const style::mojom::ColorScheme schemes[] = {
      style::mojom::ColorScheme::kVibrant};
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
                          .scheme = style::mojom::ColorScheme::kVibrant,
                          .primary = SkColorSetRGB(0x00, 0xc3, 0x82),
                          .secondary = SkColorSetRGB(0x00, 0x88, 0x59),
                          .tertiary = SkColorSetRGB(0x70, 0xb7, 0xb7)}));
}

TEST_F(ColorPaletteControllerTest, NewUser_UsesCelebiColor) {
  SimulateNewUserFirstLogin("thecreek@song.com");
  base::RunLoop().RunUntilIdle();

  UpdateWallpaperColor(kCelebiColor);

  ASSERT_EQ(kCelebiColor,
            color_palette_controller()->GetCurrentSeed()->seed_color);
}

TEST_F(ColorPaletteControllerTest,
       UseKMeans_LogsInForTheFirstTime_UsesCelebiColor) {
  const SkColor celebi_color = SK_ColorBLUE;
  SetUseKMeansPref(true);

  SimulateNewUserFirstLogin("themesong@instrumentparade.com");
  base::RunLoop().RunUntilIdle();
  UpdateWallpaperColor(celebi_color);

  ASSERT_EQ(celebi_color,
            color_palette_controller()->GetCurrentSeed()->seed_color);
}

TEST_F(ColorPaletteControllerTest, ExistingUser_UsesKMeansColor) {
  const bool dark_mode = true;
  dark_light_controller()->SetDarkModeEnabledForTest(dark_mode);

  SimulateUserLogin(kAccountId);
  base::RunLoop().RunUntilIdle();
  UpdateWallpaperColor(kCelebiColor);

  ASSERT_EQ(ColorUtil::AdjustKMeansColor(kKMeanColor, dark_mode),
            color_palette_controller()->GetCurrentSeed()->seed_color);
}

TEST_F(ColorPaletteControllerTest,
       GetWallpaperColorOrDefault_UseKMeans_TonalSpot_ReturnsKMeans) {
  const bool dark_mode = true;
  dark_light_controller()->SetDarkModeEnabledForTest(dark_mode);
  SimulateUserLogin(kAccountId);
  UpdateWallpaperColor(kCelebiColor);
  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kTonalSpot, kAccountId, base::DoNothing());
  SetUseKMeansPref(true);

  SkColor color =
      color_palette_controller()->GetUserWallpaperColorOrDefault(SK_ColorBLUE);

  ASSERT_EQ(ColorUtil::AdjustKMeansColor(kKMeanColor, dark_mode), color);
}

TEST_F(ColorPaletteControllerTest,
       GetWallpaperColorOrDefault_UseKMeans_NotTonalSpot_ReturnsCelebiColor) {
  SimulateUserLogin(kAccountId);
  UpdateWallpaperColor(kCelebiColor);
  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kVibrant, kAccountId, base::DoNothing());
  SetUseKMeansPref(true);

  SkColor color =
      color_palette_controller()->GetUserWallpaperColorOrDefault(SK_ColorBLUE);

  ASSERT_EQ(kCelebiColor, color);
}

TEST_F(ColorPaletteControllerTest,
       GetWallpaperColorOrDefault_UseKMeansIsFalse_TonalSpot_ReturnsCelebi) {
  SimulateUserLogin(kAccountId);
  UpdateWallpaperColor(kCelebiColor);
  SetUseKMeansPref(false);
  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kTonalSpot, kAccountId, base::DoNothing());

  SkColor color =
      color_palette_controller()->GetUserWallpaperColorOrDefault(SK_ColorBLUE);

  ASSERT_EQ(kCelebiColor, color);
}

TEST_F(ColorPaletteControllerTest, GuestLogin_UsesCelebiColor) {
  const SkColor celebi_color = SK_ColorBLUE;

  SimulateGuestLogin();
  base::RunLoop().RunUntilIdle();
  UpdateWallpaperColor(celebi_color);

  ASSERT_EQ(celebi_color,
            color_palette_controller()->GetCurrentSeed()->seed_color);
}

TEST_F(ColorPaletteControllerTest, WallpaperChanged_TurnsOffKMeans) {
  const SkColor celebi_color = SK_ColorBLUE;
  SetUseKMeansPref(true);
  SimulateUserLogin(kAccountId);
  UpdateWallpaperColor(celebi_color);
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
  SkBitmap bitmap;
  bitmap.allocN32Pixels(display_size.width(), display_size.height(),
                        /*isOpaque=*/true);
  bitmap.eraseColor(SK_ColorRED);
  const auto image = gfx::ImageSkia::CreateFrom1xBitmap(std::move(bitmap));

  // Update wallpaper.
  wallpaper_controller()->SetDecodedCustomWallpaper(
      kAccountId, "bluey", ash::WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, base::DoNothing(), /*file_path=*/"", image);

  ASSERT_EQ(celebi_color,
            color_palette_controller()->GetCurrentSeed()->seed_color);
}

TEST_F(ColorPaletteControllerTest, UseKMeansColor_OnlyTonalSpotUsesKMeans) {
  const bool dark_mode = true;
  dark_light_controller()->SetDarkModeEnabledForTest(dark_mode);
  SimulateUserLogin(kAccountId);
  SetUseKMeansPref(true);
  UpdateWallpaperColor(kCelebiColor);
  base::RunLoop().RunUntilIdle();

  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kTonalSpot, kAccountId, base::DoNothing());
  ASSERT_EQ(ColorUtil::AdjustKMeansColor(kKMeanColor, dark_mode),
            color_palette_controller()->GetCurrentSeed()->seed_color);

  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kNeutral, kAccountId, base::DoNothing());
  ASSERT_EQ(kCelebiColor,
            color_palette_controller()->GetCurrentSeed()->seed_color);

  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kVibrant, kAccountId, base::DoNothing());
  ASSERT_EQ(kCelebiColor,
            color_palette_controller()->GetCurrentSeed()->seed_color);

  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kExpressive, kAccountId, base::DoNothing());
  ASSERT_EQ(kCelebiColor,
            color_palette_controller()->GetCurrentSeed()->seed_color);
}

TEST_F(ColorPaletteControllerTest, WithoutUseKMeansColor_AllSchemesUseCelebi) {
  const SkColor celebi_color = SK_ColorBLUE;
  SimulateUserLogin(kAccountId);
  SetUseKMeansPref(false);
  UpdateWallpaperColor(SK_ColorBLUE);
  base::RunLoop().RunUntilIdle();

  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kTonalSpot, kAccountId, base::DoNothing());
  ASSERT_EQ(celebi_color,
            color_palette_controller()->GetCurrentSeed()->seed_color);

  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kNeutral, kAccountId, base::DoNothing());
  ASSERT_EQ(celebi_color,
            color_palette_controller()->GetCurrentSeed()->seed_color);

  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kVibrant, kAccountId, base::DoNothing());
  ASSERT_EQ(celebi_color,
            color_palette_controller()->GetCurrentSeed()->seed_color);

  color_palette_controller()->SetColorScheme(
      style::mojom::ColorScheme::kExpressive, kAccountId, base::DoNothing());
  ASSERT_EQ(celebi_color,
            color_palette_controller()->GetCurrentSeed()->seed_color);
}

TEST_F(ColorPaletteControllerTest, GetSampleColorSchemes_WithKMeans) {
  SimulateUserLogin(kAccountId);
  SetUseKMeansPref(true);

  SkColor seed = SkColorSetRGB(0xf5, 0x42, 0x45);  // Hue 359* Saturation 73%
                                                   // Vibrance 96%

  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(WallpaperCalculatedColors(SK_ColorWHITE, seed));

  const style::mojom::ColorScheme schemes[] = {
      style::mojom::ColorScheme::kExpressive,
      style::mojom::ColorScheme::kTonalSpot};
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
  // The tonal spot primary color differs from that in the
  // |GenerateSampleScheme| test, but the expressive primary color does not.
  EXPECT_THAT(results, testing::UnorderedElementsAre(
                           Sample(style::mojom::ColorScheme::kTonalSpot,
                                  SkColorSetRGB(0x74, 0xd5, 0xe4)),
                           Sample(style::mojom::ColorScheme::kExpressive,
                                  SkColorSetRGB(0xc8, 0xbf, 0xff))));
}

TEST_F(ColorPaletteControllerTest, OneNotificationOnActiveUserChange) {
  TestObserver observer;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver> observation(
      &observer);
  observation.Observe(ui::NativeTheme::GetInstanceForNativeUi());

  SimulateUserLogin(kAccountId);

  EXPECT_EQ(1, observer.call_count());
}

class ColorPaletteControllerLocalPrefTest : public ColorPaletteControllerTest {
 public:
  void SetUp() override {
    ColorPaletteControllerTest::SetUp();
    GetSessionControllerClient()->Reset();
  }

  //  Sets the local ColorScheme to kVibrant. The synced color scheme remains
  //  the default, kTonalSpot.
  void SetUpLocalPrefs() {
    user_manager::KnownUser(local_state())
        .SetIntegerPref(kAccountId, prefs::kDynamicColorColorScheme,
                        static_cast<int>(kLocalColorScheme));
  }

  style::mojom::ColorScheme GetLocalColorScheme() {
    auto local_color_scheme =
        user_manager::KnownUser(local_state())
            .FindIntPath(kAccountId, prefs::kDynamicColorColorScheme);
    return static_cast<style::mojom::ColorScheme>(local_color_scheme.value());
  }

  std::optional<bool> GetLocalUseKMeans() {
    const base::Value* local_color_scheme =
        user_manager::KnownUser(local_state())
            .FindPath(kAccountId, prefs::kDynamicColorUseKMeans);
    if (!local_color_scheme) {
      return {};
    }
    return local_color_scheme->GetIfBool();
  }

  void SetUseKMeansLocalPref(bool use_k_means) {
    user_manager::KnownUser(local_state())
        .SetBooleanPref(kAccountId, prefs::kDynamicColorUseKMeans, use_k_means);
  }
};

TEST_F(ColorPaletteControllerLocalPrefTest, OnUserLogin_UpdatesLocalPrefs) {
  SetUpLocalPrefs();
  const auto wallpaper_color = SK_ColorGRAY;
  UpdateWallpaperColor(wallpaper_color);
  SetUseKMeansLocalPref(false);
  EXPECT_EQ(kLocalColorScheme, GetLocalColorScheme());

  SimulateUserLogin(kAccountId);

  // Expect that the local prefs are updated when the user logs in.
  EXPECT_EQ(kDefaultColorScheme, GetLocalColorScheme());
  EXPECT_TRUE(*GetLocalUseKMeans());
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       SelectLocalAccount_NotifiesObservers) {
  SetUpLocalPrefs();
  SessionController::Get()->SetClient(nullptr);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer, OnColorPaletteChanging(testing::Field(
                            &ColorPaletteSeed::scheme, kLocalColorScheme)))
      .Times(1);

  color_palette_controller()->SelectLocalAccount(kAccountId);
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       SelectLocalAccount_UseKMeansIsFalse_UsesCelebiColor) {
  SetUseKMeansLocalPref(false);
  UpdateWallpaperColor(kCelebiColor);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer,
              OnColorPaletteChanging(testing::AllOf(
                  testing::Field(&ColorPaletteSeed::scheme,
                                 style::mojom::ColorScheme::kTonalSpot),
                  testing::Field(&ColorPaletteSeed::seed_color, kCelebiColor))))
      .Times(1);

  color_palette_controller()->SelectLocalAccount(kAccountId);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       SelectLocalAccount_UseKMeansIsTrue_TonalSpot_UsesKMeansColor) {
  const bool dark_mode = true;
  dark_light_controller()->SetDarkModeEnabledForTest(dark_mode);
  SimulateUserLogin(kAccountId);
  SetUseKMeansLocalPref(true);
  SessionController::Get()->SetClient(nullptr);
  UpdateWallpaperColor(kCelebiColor);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer,
              OnColorPaletteChanging(testing::AllOf(
                  testing::Field(&ColorPaletteSeed::scheme,
                                 style::mojom::ColorScheme::kTonalSpot),
                  testing::Field(
                      &ColorPaletteSeed::seed_color,
                      ColorUtil::AdjustKMeansColor(kKMeanColor, dark_mode)))))
      .Times(1);

  color_palette_controller()->SelectLocalAccount(kAccountId);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       SelectLocalAccount_UseKMeansIsTrue_Vibrant_UsesCelebiColor) {
  SetUpLocalPrefs();
  SessionController::Get()->SetClient(nullptr);
  SetUseKMeansLocalPref(true);
  UpdateWallpaperColor(kCelebiColor);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer,
              OnColorPaletteChanging(testing::AllOf(
                  testing::Field(&ColorPaletteSeed::scheme,
                                 style::mojom::ColorScheme::kVibrant),
                  testing::Field(&ColorPaletteSeed::seed_color, kCelebiColor))))
      .Times(1);

  color_palette_controller()->SelectLocalAccount(kAccountId);
  base::RunLoop().RunUntilIdle();
}

// Verifies that when the TimeOfDayWallpaper wallpaper is active, the default
// color scheme is Neutral instead of TonalSpot in local_state.
TEST_F(ColorPaletteControllerLocalPrefTest, NoLocalAccount_TimeOfDayScheme) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      personalization_app::GetTimeOfDayEnabledFeatures(), {});
  // Sets the current wallpaper to be ToD.
  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.ShowWallpaperImage(CreateTimeOfDayWallpaperInfo(),
                               /*preview_mode=*/false, /*is_override=*/false);
  base::RunLoop().RunUntilIdle();

  // Since `kAccountId` is not logged in, this triggers default local_state
  // behavior.
  EXPECT_EQ(style::mojom::ColorScheme::kNeutral,
            color_palette_controller()->GetColorScheme(kAccountId));
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       SelectLocalAccount_NoLocalState_NotifiesObserversWithDefault) {
  SessionController::Get()->SetClient(nullptr);
  UpdateWallpaperColor(kCelebiColor);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(
      observer,
      OnColorPaletteChanging(testing::AllOf(
          testing::Field(&ColorPaletteSeed::scheme, kDefaultColorScheme),
          testing::Field(&ColorPaletteSeed::seed_color, kCelebiColor))))
      .Times(1);

  color_palette_controller()->SelectLocalAccount(kAccountId);
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       UpdateWallpaperColor_WithSession_NotifiesObservers) {
  SetUpLocalPrefs();
  SimulateUserLogin(kAccountId);
  color_palette_controller()->SetColorScheme(kLocalColorScheme, kAccountId,
                                             base::DoNothing());
  base::RunLoop().RunUntilIdle();

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer, OnColorPaletteChanging(testing::Field(
                            &ColorPaletteSeed::scheme, kLocalColorScheme)))
      .Times(1);

  UpdateWallpaperColor(SK_ColorWHITE);
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       UpdateWallpaperColor_WithoutSession_DoesNotNotifyObservers) {
  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer, OnColorPaletteChanging(testing::_)).Times(0);

  UpdateWallpaperColor(SK_ColorWHITE);
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       UpdateWallpaperColor_WithOobeSession_NotifiesObservers) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);
  // Set the UseKMeans pref to make sure that it does not affect OOBE.
  SetUseKMeansLocalPref(true);
  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  // OOBE should always use the celebi color.
  EXPECT_CALL(observer, OnColorPaletteChanging(testing::Field(
                            &ColorPaletteSeed::seed_color, kCelebiColor)))
      .Times(1);

  UpdateWallpaperColor(kCelebiColor);
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       UpdateWallpaperColor_WithOobeLogin_NotifiesObservers) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  // Set the UseKMeans pref to make sure that it does not affect OOBE.
  SetUseKMeansLocalPref(true);
  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  // OOBE should always use the celebi color.
  EXPECT_CALL(observer, OnColorPaletteChanging(testing::Field(
                            &ColorPaletteSeed::seed_color, kCelebiColor)))
      .Times(1);

  LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      OobeDialogState::GAIA_SIGNIN);
  UpdateWallpaperColor(kCelebiColor);
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       UpdateWallpaperColor_WithNonOobeLogin_DoesNotNotifyObservers) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer, OnColorPaletteChanging(testing::_)).Times(0);

  LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      OobeDialogState::HIDDEN);
  UpdateWallpaperColor(SK_ColorWHITE);
}

// Helper to print better matcher errors.
void PrintTo(const SampleColorScheme& scheme, std::ostream* os) {
  *os << base::StringPrintf(
      "SampleColorScheme(scheme: %u primary: %x secondary: %x tertiary: %x)",
      static_cast<unsigned>(scheme.scheme), scheme.primary, scheme.secondary,
      scheme.tertiary);
}

}  // namespace ash
