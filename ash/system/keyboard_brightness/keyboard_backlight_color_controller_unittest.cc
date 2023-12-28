// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/rgb_keyboard/rgb_keyboard_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

namespace {
constexpr char kUser1[] = "user1@test.com";
const AccountId account_id_1 = AccountId::FromUserEmailGaiaId(kUser1, kUser1);

constexpr char kUser2[] = "user2@test.com";
const AccountId account_id_2 = AccountId::FromUserEmailGaiaId(kUser2, kUser2);

// Creates an image of size |size|.
gfx::ImageSkia CreateImage(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  return image;
}

class TestWallpaperObserver : public ash::WallpaperControllerObserver {
 public:
  TestWallpaperObserver() {
    wallpaper_controller_observation_.Observe(WallpaperControllerImpl::Get());
  }

  TestWallpaperObserver(const TestWallpaperObserver&) = delete;
  TestWallpaperObserver& operator=(const TestWallpaperObserver&) = delete;

  ~TestWallpaperObserver() override = default;

  // ash::WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override {
    DCHECK(ui_run_loop_);
    ui_run_loop_->QuitWhenIdle();
  }

  // Wait until the wallpaper update is completed.
  void WaitForWallpaperColorsChanged() {
    ui_run_loop_ = std::make_unique<base::RunLoop>();
    ui_run_loop_->Run();
  }

 private:
  std::unique_ptr<base::RunLoop> ui_run_loop_;
  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_controller_observation_{this};
};
}  // namespace

class KeyboardBacklightColorControllerTest : public AshTestBase {
 public:
  KeyboardBacklightColorControllerTest() {
    scoped_feature_list_.InitWithFeatures({features::kMultiZoneRgbKeyboard},
                                          {});
    set_start_session(false);
  }

  KeyboardBacklightColorControllerTest(
      const KeyboardBacklightColorControllerTest&) = delete;
  KeyboardBacklightColorControllerTest& operator=(
      const KeyboardBacklightColorControllerTest&) = delete;

  ~KeyboardBacklightColorControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    controller_ =
        std::make_unique<KeyboardBacklightColorController>(local_state());
    wallpaper_controller_ = Shell::Get()->wallpaper_controller();
    WallpaperControllerTestApi(wallpaper_controller_).ResetCalculatedColors();
  }

  void TearDown() override {
    controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  SkColor displayed_color() const {
    return controller_->displayed_color_for_testing_;
  }

  void clear_displayed_color() {
    controller_->displayed_color_for_testing_ = SK_ColorTRANSPARENT;
  }

  void set_rgb_capability(rgbkbd::RgbKeyboardCapabilities capability) {
    RgbKeyboardManager* rgb_keyboard_manager =
        Shell::Get()->rgb_keyboard_manager();
    rgb_keyboard_manager->OnCapabilityUpdatedForTesting(capability);
  }

  // Set the cached wallpaper color to `color`.
  void SetWallpaperColor(SkColor color) {
    WallpaperControllerTestApi test_api(wallpaper_controller_);
    WallpaperCalculatedColors calculated_colors;
    calculated_colors.k_mean_color = color;
    test_api.SetCalculatedColors(calculated_colors);
  }

  std::unique_ptr<KeyboardBacklightColorController> controller_;
  raw_ptr<WallpaperControllerImpl, DanglingUntriaged> wallpaper_controller_ =
      nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_F(KeyboardBacklightColorControllerTest, SetBacklightColorUpdatesPref) {
  SimulateUserLogin(account_id_1);
  controller_->SetBacklightColor(
      personalization_app::mojom::BacklightColor::kBlue, account_id_1);

  EXPECT_EQ(personalization_app::mojom::BacklightColor::kBlue,
            controller_->GetBacklightColor(account_id_1));
}

TEST_F(KeyboardBacklightColorControllerTest, SetBacklightColorAfterSignin) {
  controller_->OnRgbKeyboardSupportedChanged(true);
  // Verify the user starts with wallpaper-extracted color.
  SimulateUserLogin(account_id_1);
  // Expect the default choice to be wallpaper color.
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kWallpaper,
            controller_->GetBacklightColor(account_id_1));
  // Expect no histogram entries because the wallpaper color is not available.
  histogram_tester().ExpectBucketCount(
      "Ash.Personalization.KeyboardBacklight.WallpaperColor.Valid2", false, 0);
  EXPECT_EQ(kDefaultColor, displayed_color());

  controller_->SetBacklightColor(
      personalization_app::mojom::BacklightColor::kBlue, account_id_1);
  ClearLogin();

  // Simulate re-login for user1 and expect blue color to be set.
  SimulateUserLogin(account_id_1);
  EXPECT_EQ(ConvertBacklightColorToSkColor(
                personalization_app::mojom::BacklightColor::kBlue),
            displayed_color());
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kBlue,
            controller_->GetBacklightColor(account_id_1));

  // Simulate login for user2.
  SimulateUserLogin(account_id_2);
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kWallpaper,
            controller_->GetBacklightColor(account_id_2));
}

TEST_F(KeyboardBacklightColorControllerTest,
       DisplaysDefaultColorForNearlyBlackColor) {
  SetWallpaperColor(SkColorSetRGB(/*r=*/0, /*g=*/0, /*b=*/10));
  controller_->OnRgbKeyboardSupportedChanged(true);

  // This triggers twice. Once because OnWallpaperColorsChanged() is triggered
  // in OnRgbKeyboardSupportedChanged() and again in that same method because
  // we're logged in.
  histogram_tester().ExpectBucketCount(
      "Ash.Personalization.KeyboardBacklight.WallpaperColor.Valid2", true, 2);
  EXPECT_EQ(kDefaultColor, displayed_color());
}

TEST_F(KeyboardBacklightColorControllerTest, DisplayWhiteBacklightOnOobe) {
  controller_->OnSessionStateChanged(session_manager::SessionState::ACTIVE);
  EXPECT_NE(ConvertBacklightColorToSkColor(
                personalization_app::mojom::BacklightColor::kWhite),
            displayed_color());

  controller_->OnSessionStateChanged(session_manager::SessionState::LOCKED);
  EXPECT_NE(ConvertBacklightColorToSkColor(
                personalization_app::mojom::BacklightColor::kWhite),
            displayed_color());

  controller_->OnSessionStateChanged(session_manager::SessionState::OOBE);
  EXPECT_EQ(ConvertBacklightColorToSkColor(
                personalization_app::mojom::BacklightColor::kWhite),
            displayed_color());
}

// SwitchUserWithDifferentWallPaperColor test makes sure that the keyboard color
// doesn't switch from user1's color until user2's wallpaper has been loaded in.
TEST_F(KeyboardBacklightColorControllerTest,
       SwitchUserWithDifferentWallPaperColor) {
  controller_->OnRgbKeyboardSupportedChanged(true);
  SimulateUserLogin(account_id_1);
  controller_->SetBacklightColor(
      personalization_app::mojom::BacklightColor::kBlue, account_id_1);
  ClearLogin();

  SimulateUserLogin(account_id_2);
  controller_->SetBacklightColor(
      personalization_app::mojom::BacklightColor::kWallpaper, account_id_2);
  ClearLogin();

  // Simulate re-login for user1 and expect blue color to be set.
  SimulateUserLogin(account_id_1);
  EXPECT_EQ(ConvertBacklightColorToSkColor(
                personalization_app::mojom::BacklightColor::kBlue),
            displayed_color());
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kBlue,
            controller_->GetBacklightColor(account_id_1));

  // Simulate re-login for user2 and expect blue color to be set.
  SimulateUserLogin(account_id_2);
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kWallpaper,
            controller_->GetBacklightColor(account_id_2));
  EXPECT_EQ(ConvertBacklightColorToSkColor(
                personalization_app::mojom::BacklightColor::kBlue),
            displayed_color());

  // Set the wallpaper and check that the displayed color now matches the
  // default color.
  TestWallpaperObserver observer;
  gfx::ImageSkia one_shot_wallpaper =
      CreateImage(640, 480, SkColorSetRGB(/*r=*/0, /*g=*/0, /*b=*/10));
  wallpaper_controller_->ShowOneShotWallpaper(one_shot_wallpaper);
  observer.WaitForWallpaperColorsChanged();

  histogram_tester().ExpectBucketCount(
      "Ash.Personalization.KeyboardBacklight.WallpaperColor.Valid2", true, 1);
  EXPECT_EQ(kDefaultColor, displayed_color());
}

TEST_F(KeyboardBacklightColorControllerTest,
       DisplayWhiteBacklightOnOobeWithLateInitialization) {
  controller_->OnRgbKeyboardSupportedChanged(false);

  // Initialize an OOBE session before rgb keyboard is initialized.
  SessionInfo session_info;
  session_info.state = session_manager::SessionState::OOBE;
  Shell::Get()->session_controller()->SetSessionInfo(session_info);
  EXPECT_NE(ConvertBacklightColorToSkColor(
                personalization_app::mojom::BacklightColor::kWhite),
            displayed_color());

  // Once initialized, rgb keyboard should be set to white.
  controller_->OnRgbKeyboardSupportedChanged(true);
  EXPECT_EQ(ConvertBacklightColorToSkColor(
                personalization_app::mojom::BacklightColor::kWhite),
            displayed_color());
}

TEST_F(KeyboardBacklightColorControllerTest,
       DisplaysWallpaperColorWithLateInitialization) {
  controller_->OnRgbKeyboardSupportedChanged(false);

  TestWallpaperObserver observer;
  SimulateUserLogin(account_id_1);
  gfx::ImageSkia one_shot_wallpaper =
      CreateImage(640, 480, SkColorSetRGB(/*r=*/0, /*g=*/0, /*b=*/10));
  wallpaper_controller_->ShowOneShotWallpaper(one_shot_wallpaper);
  observer.WaitForWallpaperColorsChanged();

  // Color should not be set here as rgb keyboard is not yet initialized.
  EXPECT_NE(kDefaultColor, displayed_color());

  // Once initialized, the color should be set to the default color.
  controller_->OnRgbKeyboardSupportedChanged(true);
  EXPECT_EQ(kDefaultColor, displayed_color());
}

TEST_F(KeyboardBacklightColorControllerTest, TurnsOnKeyboardBrightnessWhenOff) {
  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();

  // Turn off keyboard backlight
  client->set_keyboard_brightness_percent(0);
  SimulateUserLogin(account_id_1);
  controller_->SetBacklightColor(
      personalization_app::mojom::BacklightColor::kBlue, account_id_1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kBlue,
            controller_->GetBacklightColor(account_id_1));
  EXPECT_EQ(client->keyboard_brightness_percent(),
            KeyboardBacklightColorController::kDefaultBacklightBrightness);
}

TEST_F(KeyboardBacklightColorControllerTest,
       DoesNotModifyKeyboardBrightnessWhenOn) {
  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();

  const double kStartingBrightness = 20.0;
  client->set_keyboard_brightness_percent(kStartingBrightness);
  SimulateUserLogin(account_id_1);
  controller_->SetBacklightColor(
      personalization_app::mojom::BacklightColor::kBlue, account_id_1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kBlue,
            controller_->GetBacklightColor(account_id_1));
  EXPECT_EQ(client->keyboard_brightness_percent(), kStartingBrightness);
}

TEST_F(KeyboardBacklightColorControllerTest, GetBacklightZoneColors) {
  controller_->OnRgbKeyboardSupportedChanged(true);
  SimulateUserLogin(account_id_1);

  RgbKeyboardManager* rgb_keyboard_manager =
      Shell::Get()->rgb_keyboard_manager();
  set_rgb_capability(rgbkbd::RgbKeyboardCapabilities::kIndividualKey);
  const auto color_to_be_set =
      personalization_app::mojom::BacklightColor::kBlue;
  controller_->SetBacklightColor(color_to_be_set, account_id_1);
  base::RunLoop().RunUntilIdle();
  // Expects all the zone colors are set to blue.
  std::vector<personalization_app::mojom::BacklightColor> zone_colors =
      controller_->GetBacklightZoneColors(account_id_1);
  EXPECT_EQ(rgb_keyboard_manager->GetZoneCount(),
            static_cast<int>(zone_colors.size()));
  for (auto color : zone_colors) {
    EXPECT_EQ(color, color_to_be_set);
  }
}

TEST_F(KeyboardBacklightColorControllerTest,
       PopulatesBacklightZoneColorsPrefAfterSigningIn) {
  controller_->OnRgbKeyboardSupportedChanged(true);
  RgbKeyboardManager* rgb_keyboard_manager =
      Shell::Get()->rgb_keyboard_manager();
  set_rgb_capability(rgbkbd::RgbKeyboardCapabilities::kIndividualKey);
  SimulateUserLogin(account_id_1);
  // Expects all the zone colors are set to kWallpaper.
  std::vector<personalization_app::mojom::BacklightColor> zone_colors =
      controller_->GetBacklightZoneColors(account_id_1);
  EXPECT_EQ(rgb_keyboard_manager->GetZoneCount(),
            static_cast<int>(zone_colors.size()));
  for (auto color : zone_colors) {
    EXPECT_EQ(color, personalization_app::mojom::BacklightColor::kWallpaper);
  }
}

TEST_F(KeyboardBacklightColorControllerTest, SetBacklightZoneColor) {
  controller_->OnRgbKeyboardSupportedChanged(true);
  RgbKeyboardManager* rgb_keyboard_manager =
      Shell::Get()->rgb_keyboard_manager();
  set_rgb_capability(rgbkbd::RgbKeyboardCapabilities::kIndividualKey);
  SimulateUserLogin(account_id_1);
  const auto default_color =
      personalization_app::mojom::BacklightColor::kWallpaper;
  EXPECT_EQ(default_color, controller_->GetBacklightColor(account_id_1));
  // Expects all the zone colors are set to the wallpaper color.
  std::vector<personalization_app::mojom::BacklightColor> zone_colors =
      controller_->GetBacklightZoneColors(account_id_1);
  EXPECT_EQ(rgb_keyboard_manager->GetZoneCount(),
            static_cast<int>(zone_colors.size()));
  for (auto color : zone_colors) {
    EXPECT_EQ(color, default_color);
  }
  // Expects the backligh color display type to be set to `kStatic`.
  EXPECT_EQ(KeyboardBacklightColorController::DisplayType::kStatic,
            controller_->GetDisplayType(account_id_1));

  // Updates one of the zone to a different color.
  const int zone = 3;
  const auto color_to_be_set = personalization_app::mojom::BacklightColor::kRed;
  controller_->SetBacklightZoneColor(zone, color_to_be_set, account_id_1);
  // Expects the backligh color display type to be set to `kMultiZone`.
  EXPECT_EQ(KeyboardBacklightColorController::DisplayType::kMultiZone,
            controller_->GetDisplayType(account_id_1));
  // Expects zone color to be updated.
  zone_colors = controller_->GetBacklightZoneColors(account_id_1);
  EXPECT_EQ(color_to_be_set, zone_colors.at(zone));
}

}  // namespace ash
