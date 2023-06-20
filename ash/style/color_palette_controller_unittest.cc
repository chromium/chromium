// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_palette_controller.h"

#include <ostream>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/known_user.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

namespace {

const char kUser[] = "user@gmail.com";
const AccountId kAccountId = AccountId::FromUserEmailGaiaId(kUser, kUser);
const ColorScheme kLocalColorScheme = ColorScheme::kVibrant;
const ColorScheme kDefaultColorScheme = ColorScheme::kTonalSpot;
const SkColor kDefaultWallpaperColor = gfx::kGoogleBlue400;

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
  raw_ptr<DarkLightModeControllerImpl, DanglingUntriaged>
      dark_light_mode_controller_;  // unowned
  raw_ptr<WallpaperControllerImpl, DanglingUntriaged>
      wallpaper_controller_;  // unowned

  raw_ptr<ColorPaletteController, DanglingUntriaged> color_palette_controller_;
};

TEST_F(ColorPaletteControllerTest, ExpectedEmptyValues) {
  EXPECT_EQ(kDefaultColorScheme,
            color_palette_controller()->GetColorScheme(kAccountId));
  EXPECT_EQ(absl::nullopt,
            color_palette_controller()->GetStaticColor(kAccountId));
}

// Verifies that when the TimeOfDayWallpaper feature is active, the default
// color scheme is Neutral instead of TonalSpot.
TEST_F(ColorPaletteControllerTest, ExpectedColorScheme_TimeOfDay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::features::kTimeOfDayWallpaper, chromeos::features::kJelly}, {});
  EXPECT_EQ(ColorScheme::kNeutral,
            color_palette_controller()->GetColorScheme(kAccountId));
}

TEST_F(ColorPaletteControllerTest,
       SetColorScheme_JellyDisabled_UsesDefaultScheme) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(chromeos::features::kJelly);
  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors({}, kKMeanColor, SK_ColorWHITE));

  color_palette_controller()->SetColorScheme(ColorScheme::kStatic, kAccountId,
                                             base::DoNothing());
  EXPECT_EQ(
      kDefaultColorScheme,
      color_palette_controller()->GetColorPaletteSeed(kAccountId)->scheme);

  color_palette_controller()->SetColorScheme(ColorScheme::kExpressive,
                                             kAccountId, base::DoNothing());
  EXPECT_EQ(
      kDefaultColorScheme,
      color_palette_controller()->GetColorPaletteSeed(kAccountId)->scheme);
}

TEST_F(ColorPaletteControllerTest, SetColorScheme) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
  SimulateUserLogin(kAccountId);
  WallpaperControllerTestApi wallpaper(wallpaper_controller());
  wallpaper.SetCalculatedColors(
      WallpaperCalculatedColors({}, kKMeanColor, SK_ColorWHITE));
  const ColorScheme color_scheme = ColorScheme::kExpressive;

  color_palette_controller()->SetColorScheme(color_scheme, kAccountId,
                                             base::DoNothing());

  EXPECT_EQ(color_scheme,
            color_palette_controller()->GetColorScheme(kAccountId));
  EXPECT_EQ(absl::nullopt,
            color_palette_controller()->GetStaticColor(kAccountId));
  auto color_palette_seed =
      color_palette_controller()->GetColorPaletteSeed(kAccountId);
  EXPECT_EQ(color_scheme, color_palette_seed->scheme);
  // Verify that the color scheme was saved to local state.
  auto local_color_scheme =
      user_manager::KnownUser(local_state())
          .FindIntPath(kAccountId, prefs::kDynamicColorColorScheme);
  EXPECT_EQ(color_scheme, static_cast<ColorScheme>(local_color_scheme.value()));
}

TEST_F(ColorPaletteControllerTest, SetStaticColor) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
  SimulateUserLogin(kAccountId);
  const SkColor static_color = SK_ColorGRAY;

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
  auto local_color_scheme =
      user_manager::KnownUser(local_state())
          .FindIntPath(kAccountId, prefs::kDynamicColorColorScheme);
  EXPECT_EQ(ColorScheme::kStatic,
            static_cast<ColorScheme>(local_color_scheme.value()));
  const base::Value* value =
      user_manager::KnownUser(local_state())
          .FindPath(kAccountId, prefs::kDynamicColorSeedColor);
  // Verify that the color was saved to local state.
  const auto local_static_color = base::ValueToInt64(value);
  EXPECT_EQ(static_color, static_cast<SkColor>(local_static_color.value()));
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
          Sample(ColorScheme::kTonalSpot, SkColorSetRGB(0xff, 0xb3, 0xae)),
          Sample(ColorScheme::kExpressive, SkColorSetRGB(0xc8, 0xbf, 0xff))));
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
                          .primary = SkColorSetRGB(0x00, 0xc3, 0x82),
                          .secondary = SkColorSetRGB(0x00, 0x88, 0x59),
                          .tertiary = SkColorSetRGB(0x70, 0xb7, 0xb7)}));
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

  ColorScheme GetLocalColorScheme() {
    auto local_color_scheme =
        user_manager::KnownUser(local_state())
            .FindIntPath(kAccountId, prefs::kDynamicColorColorScheme);
    return static_cast<ColorScheme>(local_color_scheme.value());
  }

  void UpdateWallpaperColor(SkColor color) {
    WallpaperControllerTestApi wallpaper(wallpaper_controller());
    wallpaper.SetCalculatedColors(
        WallpaperCalculatedColors({}, kKMeanColor, color));
    base::RunLoop().RunUntilIdle();
  }
};

TEST_F(ColorPaletteControllerLocalPrefTest, OnUserLogin_UpdatesLocalPrefs) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
  SetUpLocalPrefs();
  const auto wallpaper_color = SK_ColorGRAY;
  UpdateWallpaperColor(wallpaper_color);
  EXPECT_EQ(kLocalColorScheme, GetLocalColorScheme());

  SimulateUserLogin(kAccountId);

  // Expect that the local prefs are updated when the user logs in.
  EXPECT_EQ(kDefaultColorScheme, GetLocalColorScheme());
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       SelectLocalAccount_NotifiesObservers) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
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

// Verifies that when the TimeOfDayWallpaper feature is active, the default
// color scheme is Neutral instead of TonalSpot in local_state.
TEST_F(ColorPaletteControllerLocalPrefTest, NoLocalAccount_TimeOfDayScheme) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::features::kTimeOfDayWallpaper, chromeos::features::kJelly}, {});
  // Since `kAccountId` is not logged in, this triggers default local_state
  // behavior.
  EXPECT_EQ(ColorScheme::kNeutral,
            color_palette_controller()->GetColorScheme(kAccountId));
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       SelectLocalAccount_NoLocalState_NotifiesObserversWithDefault) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
  SessionController::Get()->SetClient(nullptr);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(
      observer,
      OnColorPaletteChanging(testing::AllOf(
          testing::Field(&ColorPaletteSeed::scheme, kDefaultColorScheme),
          testing::Field(&ColorPaletteSeed::seed_color,
                         kDefaultWallpaperColor))))
      .Times(1);

  color_palette_controller()->SelectLocalAccount(kAccountId);
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       SelectLocalAccount_JellyDisabled_SkipsNotification) {
  SessionController::Get()->SetClient(nullptr);

  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer, OnColorPaletteChanging(testing::_)).Times(0);

  color_palette_controller()->SelectLocalAccount(kAccountId);
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       UpdateWallpaperColor_WithSession_NotifiesObservers) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
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
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
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
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);
  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer, OnColorPaletteChanging(testing::_)).Times(1);

  UpdateWallpaperColor(SK_ColorWHITE);
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       UpdateWallpaperColor_WithOobeLogin_NotifiesObservers) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  MockPaletteObserver observer;
  base::ScopedObservation<ColorPaletteController,
                          ColorPaletteController::Observer>
      observation(&observer);
  observation.Observe(color_palette_controller());
  EXPECT_CALL(observer, OnColorPaletteChanging(testing::_)).Times(1);

  LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      OobeDialogState::GAIA_SIGNIN);
  UpdateWallpaperColor(SK_ColorWHITE);
}

TEST_F(ColorPaletteControllerLocalPrefTest,
       UpdateWallpaperColor_WithNonOobeLogin_DoesNotNotifyObservers) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kJelly);
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
      scheme.scheme, scheme.primary, scheme.secondary, scheme.tertiary);
}

}  // namespace ash
