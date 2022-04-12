// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/rgb_keyboard_manager.h"

#include <stdint.h>
#include <memory>
#include <vector>

#include "ash/ime/ime_controller_impl.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

namespace ash {

class RgbKeyboardManagerTest : public testing::Test {
 public:
  RgbKeyboardManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(::features::kRgbKeyboard);

    // ImeControllerImpl must be initializezd before RgbKeyboardManager.
    ime_controller_ = std::make_unique<ImeControllerImpl>();
    manager_ = std::make_unique<RgbKeyboardManager>(ime_controller_.get());
  }

  RgbKeyboardManagerTest(const RgbKeyboardManagerTest&) = delete;
  RgbKeyboardManagerTest& operator=(const RgbKeyboardManagerTest&) = delete;
  ~RgbKeyboardManagerTest() override = default;

 protected:
  // ImeControllerImpl must be destroyed after RgbKeyboardManager.
  std::unique_ptr<ImeControllerImpl> ime_controller_;
  std::unique_ptr<RgbKeyboardManager> manager_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(RgbKeyboardManagerTest, GetKeyboardCapabilities) {
  EXPECT_EQ(manager_->GetRgbKeyboardCapabilities(),
            RgbKeyboardCapabilities::kNone);
}

TEST_F(RgbKeyboardManagerTest, SetStaticRgbValues) {
  const uint8_t expected_r = 1;
  const uint8_t expected_g = 2;
  const uint8_t expected_b = 3;

  manager_->SetStaticBackgroundColor(expected_r, expected_g, expected_b);
  const std::vector<uint8_t> rgb_values = manager_->recently_sent_rgb();

  EXPECT_EQ(expected_r, rgb_values[0]);
  EXPECT_EQ(expected_g, rgb_values[1]);
  EXPECT_EQ(expected_b, rgb_values[2]);
}

TEST_F(RgbKeyboardManagerTest, SetRainbowMode) {
  EXPECT_FALSE(manager_->is_rainbow_mode_set());

  manager_->SetRainbowMode();

  EXPECT_TRUE(manager_->is_rainbow_mode_set());
}

TEST_F(RgbKeyboardManagerTest, RainbowModeResetsStatic) {
  EXPECT_FALSE(manager_->is_rainbow_mode_set());

  const uint8_t expected_r = 1;
  const uint8_t expected_g = 2;
  const uint8_t expected_b = 3;

  manager_->SetStaticBackgroundColor(expected_r, expected_g, expected_b);
  std::vector<uint8_t> rgb_values = manager_->recently_sent_rgb();

  EXPECT_EQ(expected_r, rgb_values[0]);
  EXPECT_EQ(expected_g, rgb_values[1]);
  EXPECT_EQ(expected_b, rgb_values[2]);

  manager_->SetRainbowMode();
  EXPECT_TRUE(manager_->is_rainbow_mode_set());

  rgb_values = manager_->recently_sent_rgb();
  EXPECT_EQ(0u, rgb_values[0]);
  EXPECT_EQ(0u, rgb_values[1]);
  EXPECT_EQ(0u, rgb_values[2]);
}

TEST_F(RgbKeyboardManagerTest, StaticResetRainbowMode) {
  EXPECT_FALSE(manager_->is_rainbow_mode_set());
  manager_->SetRainbowMode();
  EXPECT_TRUE(manager_->is_rainbow_mode_set());

  const uint8_t expected_r = 1;
  const uint8_t expected_g = 2;
  const uint8_t expected_b = 3;

  manager_->SetStaticBackgroundColor(expected_r, expected_g, expected_b);
  const std::vector<uint8_t> rgb_values = manager_->recently_sent_rgb();

  EXPECT_EQ(expected_r, rgb_values[0]);
  EXPECT_EQ(expected_g, rgb_values[1]);
  EXPECT_EQ(expected_b, rgb_values[2]);

  EXPECT_FALSE(manager_->is_rainbow_mode_set());
}

TEST_F(RgbKeyboardManagerTest, SetCapsLockState) {
  EXPECT_FALSE(manager_->is_caps_lock_set());
  manager_->SetCapsLockState(/*is_caps_lock_set=*/true);
  EXPECT_TRUE(manager_->is_caps_lock_set());
  manager_->SetCapsLockState(/*is_caps_lock_set=*/false);
  EXPECT_FALSE(manager_->is_caps_lock_set());
}

TEST_F(RgbKeyboardManagerTest, OnCapsLockChanged) {
  EXPECT_FALSE(manager_->is_caps_lock_set());
  ime_controller_->UpdateCapsLockState(/*caps_enabled=*/true);
  EXPECT_TRUE(manager_->is_caps_lock_set());
  ime_controller_->UpdateCapsLockState(/*caps_enabled=*/false);
  EXPECT_FALSE(manager_->is_caps_lock_set());
}

TEST_F(RgbKeyboardManagerTest, OnLoginCapsLock) {
  // Simulate CapsLock enabled upon login.
  ime_controller_->SetCapsLockEnabled(/*caps_enabled=*/true);

  // Simulate RgbKeyboardManager starting up on login.
  manager_.reset();
  manager_ = std::make_unique<RgbKeyboardManager>(ime_controller_.get());
  EXPECT_TRUE(manager_->is_caps_lock_set());
}

}  // namespace ash