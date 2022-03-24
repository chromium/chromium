// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/rgb_keyboard_manager.h"

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

namespace ash {

class RgbKeyboardManagerTest : public testing::Test {
 public:
  RgbKeyboardManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(::features::kRgbKeyboard);

    manager_ = std::make_unique<RgbKeyboardManager>();
  }

  RgbKeyboardManagerTest(const RgbKeyboardManagerTest&) = delete;
  RgbKeyboardManagerTest& operator=(const RgbKeyboardManagerTest&) = delete;
  ~RgbKeyboardManagerTest() override = default;

 protected:
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

}  // namespace ash