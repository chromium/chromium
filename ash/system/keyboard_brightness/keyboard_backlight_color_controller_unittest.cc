// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
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
      : scoped_feature_list_(features::kRgbKeyboard) {}

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
  KeyboardBacklightColorController* controller_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(KeyboardBacklightColorControllerTest, SetBacklightColorUpdatesPref) {
  controller_->SetBacklightColor(
      personalization_app::mojom::BacklightColor::kBlue);

  PrefService* prefs_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kBlue,
            static_cast<personalization_app::mojom::BacklightColor>(
                prefs_service->GetInteger(
                    prefs::kPersonalizationKeyboardBacklightColor)));
}

TEST_F(KeyboardBacklightColorControllerTest, SetBacklightColorAfterSignin) {
  // Verify the user starts with wallpaper-extracted color.
  SimulateUserLogin(account_id_1);
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kWallpaper,
            controller_->GetBacklightColor());
  controller_->SetBacklightColor(
      personalization_app::mojom::BacklightColor::kRainbow);
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kRainbow,
            controller_->GetBacklightColor());

  // Simulate login for user2.
  SimulateUserLogin(account_id_2);
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kWallpaper,
            controller_->GetBacklightColor());

  SimulateUserLogin(account_id_1);
  EXPECT_EQ(personalization_app::mojom::BacklightColor::kRainbow,
            controller_->GetBacklightColor());
}

}  // namespace ash
