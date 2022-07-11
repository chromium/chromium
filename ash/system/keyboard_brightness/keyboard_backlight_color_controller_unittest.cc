// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr char kUser1[] = "user1@test.com";
const AccountId account_id_1 = AccountId::FromUserEmailGaiaId(kUser1, kUser1);

constexpr char kUser2[] = "user2@test.com";
const AccountId account_id_2 = AccountId::FromUserEmailGaiaId(kUser2, kUser2);
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

    controller_ = Shell::Get()->keyboard_backlight_color_controller();
  }

 protected:
  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  KeyboardBacklightColorController* controller_ = nullptr;

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
  // Expect the Wallpaper color to be set after user signs in.
  histogram_tester().ExpectBucketCount(
      "Ash.Personalization.KeyboardBacklight.WallpaperColor.Valid", false, 1);
  histogram_tester().ExpectTotalCount(
      "Ash.Personalization.KeyboardBacklight.WallpaperColor.Valid", 1);
  controller_->SetBacklightColor(
      personalization_app::mojom::BacklightColor::kRainbow, account_id_1);
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kRainbow,
            controller_->GetBacklightColor(account_id_1));

  // Simulate login for user2.
  SimulateUserLogin(account_id_2);
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kWallpaper,
            controller_->GetBacklightColor(account_id_2));

  SimulateUserLogin(account_id_1);
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kRainbow,
            controller_->GetBacklightColor(account_id_1));
}

}  // namespace ash
