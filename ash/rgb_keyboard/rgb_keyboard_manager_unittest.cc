// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/rgb_keyboard_manager.h"

#include <stdint.h>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/rgb_keyboard/histogram_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/rgbkbd/fake_rgbkbd_client.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"
#include "rgb_keyboard_util.h"
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
    InitializeManagerWithCapability(
        rgbkbd::RgbKeyboardCapabilities::kIndividualKey);
  }

  RgbKeyboardManagerTest(const RgbKeyboardManagerTest&) = delete;
  RgbKeyboardManagerTest& operator=(const RgbKeyboardManagerTest&) = delete;
  ~RgbKeyboardManagerTest() override {
    // Ordering for deletion is Manger -> Client -> IME Controller
    manager_.reset();
    RgbkbdClient::Shutdown();
    ime_controller_.reset();
  };

 protected:
  void InitializeManagerWithCapability(
      rgbkbd::RgbKeyboardCapabilities capability) {
    client_->set_rgb_keyboard_capabilities(capability);
    // |ime_controller_| is initialized in RgbKeyboardManagerTest's ctor.
    DCHECK(ime_controller_);
    manager_.reset();
    manager_ = std::make_unique<RgbKeyboardManager>(ime_controller_.get());
  }
  // ImeControllerImpl must be destroyed after RgbKeyboardManager.
  std::unique_ptr<ImeControllerImpl> ime_controller_;
  std::unique_ptr<RgbKeyboardManager> manager_;
  raw_ptr<FakeRgbkbdClient> client_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(RgbKeyboardManagerTest, GetKeyboardCapabilities) {
  // kIndividualKey is the default for this test suite.
  EXPECT_EQ(rgbkbd::RgbKeyboardCapabilities::kIndividualKey,
            client_->get_rgb_keyboard_capabilities());
}

class KeyboardCapabilityHistogramEmittedTest
    : public RgbKeyboardManagerTest,
      public testing::WithParamInterface<
          std::pair<rgbkbd::RgbKeyboardCapabilities,
                    ash::rgb_keyboard::metrics::RgbKeyboardCapabilityType>> {
 public:
  KeyboardCapabilityHistogramEmittedTest() {
    std::tie(capability_, metric_) = GetParam();
  }

 protected:
  rgbkbd::RgbKeyboardCapabilities capability_;
  ash::rgb_keyboard::metrics::RgbKeyboardCapabilityType metric_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    KeyboardCapabilityHistogramEmittedTest,
    testing::Values(
        std::make_pair(
            rgbkbd::RgbKeyboardCapabilities::kNone,
            ash::rgb_keyboard::metrics::RgbKeyboardCapabilityType::kNone),
        std::make_pair(rgbkbd::RgbKeyboardCapabilities::kIndividualKey,
                       ash::rgb_keyboard::metrics::RgbKeyboardCapabilityType::
                           kIndividualKey),
        std::make_pair(rgbkbd::RgbKeyboardCapabilities::kFourZoneFortyLed,
                       ash::rgb_keyboard::metrics::RgbKeyboardCapabilityType::
                           kFourZoneFortyLed),
        std::make_pair(rgbkbd::RgbKeyboardCapabilities::kFourZoneTwelveLed,
                       ash::rgb_keyboard::metrics::RgbKeyboardCapabilityType::
                           kFourZoneTwelveLed),
        std::make_pair(rgbkbd::RgbKeyboardCapabilities::kFourZoneFifteenLed,
                       ash::rgb_keyboard::metrics::RgbKeyboardCapabilityType::
                           kFourZoneFifteenLed)));

TEST_P(KeyboardCapabilityHistogramEmittedTest,
       KeyboardCapabilityHistogramEmitted) {
  base::HistogramTester histogram_tester;
  InitializeManagerWithCapability(capability_);
  EXPECT_EQ(capability_, client_->get_rgb_keyboard_capabilities());
  histogram_tester.ExpectBucketCount(
      rgb_keyboard::metrics::kRgbKeyboardCapabilityTypeHistogramName, metric_,
      1);
}

class RgbChangeTypeHistogramEmittedTest
    : public RgbKeyboardManagerTest,
      public testing::WithParamInterface<rgbkbd::RgbKeyboardCapabilities> {
 public:
  RgbChangeTypeHistogramEmittedTest() = default;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    RgbChangeTypeHistogramEmittedTest,
    testing::Values(rgbkbd::RgbKeyboardCapabilities::kIndividualKey,
                    rgbkbd::RgbKeyboardCapabilities::kFourZoneFortyLed,
                    rgbkbd::RgbKeyboardCapabilities::kFourZoneTwelveLed,
                    rgbkbd::RgbKeyboardCapabilities::kFourZoneFifteenLed));

TEST_P(RgbChangeTypeHistogramEmittedTest, RgbChangeTypeHistogramEmitted) {
  base::HistogramTester histogram_tester;
  const auto capability = GetParam();
  const auto name =
      std::string(ash::rgb_keyboard::metrics::kRgbKeyboardHistogramPrefix +
                  ash::rgb_keyboard::metrics::GetCapabilityTypeStr(capability));
  InitializeManagerWithCapability(capability);
  manager_->SetStaticBackgroundColor(/*r=*/1, /*g=*/2, /*b=*/3);
  histogram_tester.ExpectBucketCount(
      name,
      ash::rgb_keyboard::metrics::RgbKeyboardBacklightChangeType::
          kStaticBackgroundColorChanged,
      1);
  manager_->SetRainbowMode();
  histogram_tester.ExpectBucketCount(
      name,
      ash::rgb_keyboard::metrics::RgbKeyboardBacklightChangeType::
          kRainbowModeSelected,
      1);
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
  InitializeManagerWithCapability(
      rgbkbd::RgbKeyboardCapabilities::kIndividualKey);
  ime_controller_->UpdateCapsLockState(/*caps_enabled=*/true);
  EXPECT_TRUE(client_->get_caps_lock_state());
  ime_controller_->UpdateCapsLockState(/*caps_enabled=*/false);
  EXPECT_FALSE(client_->get_caps_lock_state());
}

TEST_F(RgbKeyboardManagerTest, OnLoginCapsLock) {
  // Simulate CapsLock enabled upon login.
  ime_controller_->SetCapsLockEnabled(/*caps_enabled=*/true);

  // Simulate RgbKeyboardManager starting up on login.
  InitializeManagerWithCapability(
      rgbkbd::RgbKeyboardCapabilities::kIndividualKey);
  EXPECT_TRUE(client_->get_caps_lock_state());
}

TEST_F(RgbKeyboardManagerTest, DefaultState) {
  const RgbColor& default_rgb_values = client_->recently_sent_rgb();

  EXPECT_FALSE(client_->is_rainbow_mode_set());

  EXPECT_EQ(SkColorGetR(kDefaultColor), std::get<0>(default_rgb_values));
  EXPECT_EQ(SkColorGetG(kDefaultColor), std::get<1>(default_rgb_values));
  EXPECT_EQ(SkColorGetB(kDefaultColor), std::get<2>(default_rgb_values));
}

// TODO(jimmyxgong): This is just a stub test, there is only one enum available
// so just check num times the function has been called.
TEST_F(RgbKeyboardManagerTest, SetAnimationMode) {
  EXPECT_EQ(0, client_->animation_mode_call_count());

  manager_->SetAnimationMode(rgbkbd::RgbAnimationMode::kBasicTestPattern);

  EXPECT_EQ(1, client_->animation_mode_call_count());
}

TEST_F(RgbKeyboardManagerTest, SetCapsLockStateDisallowedForZonedKeyboards) {
  InitializeManagerWithCapability(
      rgbkbd::RgbKeyboardCapabilities::kFourZoneFortyLed);
  EXPECT_FALSE(client_->get_caps_lock_state());
  ime_controller_->UpdateCapsLockState(/*caps_enabled=*/true);
  // Caps lock state should still be false since RgbKeyboardManager should have
  // prevented the call to SetCapsLockState.
  EXPECT_FALSE(client_->get_caps_lock_state());
}

TEST_F(RgbKeyboardManagerTest, SetCapsLockStateAllowedForPerKeyKeboards) {
  InitializeManagerWithCapability(
      rgbkbd::RgbKeyboardCapabilities::kIndividualKey);
  EXPECT_FALSE(client_->get_caps_lock_state());
  ime_controller_->UpdateCapsLockState(/*caps_enabled=*/true);
  EXPECT_TRUE(client_->get_caps_lock_state());
}
}  // namespace ash
