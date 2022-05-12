// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/rgb_keyboard_manager.h"

#include <stdint.h>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/rgbkbd/fake_rgbkbd_client.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class RgbKeyboardManagerTest : public testing::Test {
 public:
  RgbKeyboardManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kRgbKeyboard,
                              features::kExperimentalRgbKeyboardPatterns},
        /*disabled_features=*/{});
    // ImeControllerImpl must be initialized before RgbKeyboardManager.
    ime_controller_ = std::make_unique<ImeControllerImpl>();
    // This is instantiating a global instance that will be deallocated in
    // the destructor of RgbKeyboardManagerTest.
    RgbkbdClient::InitializeFake();
    client_ = static_cast<FakeRgbkbdClient*>(RgbkbdClient::Get());
    // Default capabilities to 'RgbKeyboardCapabilities::kIndividualKey'
    client_->set_rgb_keyboard_capabilities(
        rgbkbd::RgbKeyboardCapabilities::kIndividualKey);
    manager_ = std::make_unique<RgbKeyboardManager>(ime_controller_.get());
  }

  RgbKeyboardManagerTest(const RgbKeyboardManagerTest&) = delete;
  RgbKeyboardManagerTest& operator=(const RgbKeyboardManagerTest&) = delete;
  ~RgbKeyboardManagerTest() override {
    // Destroy the global instance.
    RgbkbdClient::Shutdown();
  };

 protected:
  // ImeControllerImpl must be destroyed after RgbKeyboardManager.
  std::unique_ptr<ImeControllerImpl> ime_controller_;
  std::unique_ptr<RgbKeyboardManager> manager_;
  raw_ptr<FakeRgbkbdClient> client_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(RgbKeyboardManagerTest, GetKeyboardCapabilities) {
  EXPECT_EQ(manager_->GetRgbKeyboardCapabilities(),
            rgbkbd::RgbKeyboardCapabilities::kIndividualKey);
  EXPECT_EQ(1, client_->get_rgb_keyboard_capabilities_call_count());
}

TEST_F(RgbKeyboardManagerTest, SetStaticRgbValues) {
  const uint8_t expected_r = 1;
  const uint8_t expected_g = 2;
  const uint8_t expected_b = 3;

  manager_->SetStaticBackgroundColor(expected_r, expected_g, expected_b);
  const RgbColor& rgb_values = client_->recently_sent_rgb();

  EXPECT_EQ(expected_r, std::get<0>(rgb_values));
  EXPECT_EQ(expected_g, std::get<1>(rgb_values));
  EXPECT_EQ(expected_b, std::get<2>(rgb_values));
}

TEST_F(RgbKeyboardManagerTest, SetRainbowMode) {
  EXPECT_FALSE(client_->is_rainbow_mode_set());

  manager_->SetRainbowMode();

  EXPECT_TRUE(client_->is_rainbow_mode_set());
}

TEST_F(RgbKeyboardManagerTest, RainbowModeResetsStatic) {
  EXPECT_FALSE(client_->is_rainbow_mode_set());

  const uint8_t expected_r = 1;
  const uint8_t expected_g = 2;
  const uint8_t expected_b = 3;

  manager_->SetStaticBackgroundColor(expected_r, expected_g, expected_b);
  const RgbColor& rgb_values = client_->recently_sent_rgb();

  EXPECT_EQ(expected_r, std::get<0>(rgb_values));
  EXPECT_EQ(expected_g, std::get<1>(rgb_values));
  EXPECT_EQ(expected_b, std::get<2>(rgb_values));

  manager_->SetRainbowMode();
  EXPECT_TRUE(client_->is_rainbow_mode_set());

  const RgbColor& updated_rgb_values = client_->recently_sent_rgb();
  EXPECT_EQ(0u, std::get<0>(updated_rgb_values));
  EXPECT_EQ(0u, std::get<1>(updated_rgb_values));
  EXPECT_EQ(0u, std::get<2>(updated_rgb_values));
}

TEST_F(RgbKeyboardManagerTest, StaticResetRainbowMode) {
  EXPECT_FALSE(client_->is_rainbow_mode_set());
  manager_->SetRainbowMode();
  EXPECT_TRUE(client_->is_rainbow_mode_set());

  const uint8_t expected_r = 1;
  const uint8_t expected_g = 2;
  const uint8_t expected_b = 3;

  manager_->SetStaticBackgroundColor(expected_r, expected_g, expected_b);
  const RgbColor& rgb_values = client_->recently_sent_rgb();

  EXPECT_FALSE(client_->is_rainbow_mode_set());

  EXPECT_EQ(expected_r, std::get<0>(rgb_values));
  EXPECT_EQ(expected_g, std::get<1>(rgb_values));
  EXPECT_EQ(expected_b, std::get<2>(rgb_values));
}

TEST_F(RgbKeyboardManagerTest, OnCapsLockChanged) {
  ime_controller_->UpdateCapsLockState(/*caps_enabled=*/true);
  EXPECT_TRUE(client_->get_caps_lock_state());
  ime_controller_->UpdateCapsLockState(/*caps_enabled=*/false);
  EXPECT_FALSE(client_->get_caps_lock_state());
}

TEST_F(RgbKeyboardManagerTest, OnLoginCapsLock) {
  // Simulate CapsLock enabled upon login.
  ime_controller_->SetCapsLockEnabled(/*caps_enabled=*/true);

  // Simulate RgbKeyboardManager starting up on login.
  manager_.reset();
  manager_ = std::make_unique<RgbKeyboardManager>(ime_controller_.get());
  EXPECT_TRUE(client_->get_caps_lock_state());
}

// TODO(jimmyxgong): This is just a stub test, there is only one enum available
// so just check num times the function has been called.
TEST_F(RgbKeyboardManagerTest, SetAnimationMode) {
  EXPECT_EQ(0, client_->animation_mode_call_count());

  manager_->SetAnimationMode(rgbkbd::RgbAnimationMode::kBasicTestPattern);

  EXPECT_EQ(1, client_->animation_mode_call_count());
}
}  // namespace ash
