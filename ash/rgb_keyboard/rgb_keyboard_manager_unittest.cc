// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/rgb_keyboard_manager.h"

#include <memory>

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

}  // namespace ash