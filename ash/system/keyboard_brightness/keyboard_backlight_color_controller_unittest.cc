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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/session_manager/session_manager_types.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  KeyboardBacklightColorControllerTest()
      : scoped_feature_list_(features::kRgbKeyboard) {
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

    controller_ = std::make_unique<KeyboardBacklightColorController>();
    controller_->OnRgbKeyboardSupportedChanged(true);
    wallpaper_controller_ = Shell::Get()->wallpaper_controller();
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

  bool keyboard_brightness_on_for_testing() const {
    return controller_->keyboard_brightness_on_for_testing_;
  }

  void set_keyboard_brightness_off_for_testing() const {
    controller_->keyboard_brightness_on_for_testing_ = false;
  }

  void clear_displayed_color() {
    controller_->displayed_color_for_testing_ = SK_ColorTRANSPARENT;
  }

  std::unique_ptr<KeyboardBacklightColorController> controller_;
  WallpaperControllerImpl* wallpaper_controller_ = nullptr;

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
  // Verify the user starts with wallpaper-extracted color.
  SimulateUserLogin(account_id_1);
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kWallpaper,
            controller_->GetBacklightColor(account_id_1));
  // Expect the Wallpaper color to be set to the default as wallpaper color is
  // not valid in this state.
  histogram_tester().ExpectBucketCount(
      "Ash.Personalization.KeyboardBacklight.WallpaperColor.Valid", false, 1);
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
  TestWallpaperObserver observer;
  SimulateUserLogin(account_id_1);
  gfx::ImageSkia one_shot_wallpaper =
      CreateImage(640, 480, SkColorSetRGB(/*r=*/0, /*g=*/0, /*b=*/10));
  wallpaper_controller_->ShowOneShotWallpaper(one_shot_wallpaper);
  observer.WaitForWallpaperColorsChanged();

  histogram_tester().ExpectBucketCount(
      "Ash.Personalization.KeyboardBacklight.WallpaperColor.Valid", true, 1);
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
  SimulateUserLogin(account_id_1);
  set_keyboard_brightness_off_for_testing();
  controller_->SetBacklightColor(
      personalization_app::mojom::BacklightColor::kBlue, account_id_1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kBlue,
            controller_->GetBacklightColor(account_id_1));
  EXPECT_TRUE(keyboard_brightness_on_for_testing());
}

}  // namespace ash
