// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm_mode/wm_mode_controller.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class WmModeTests : public AshTestBase {
 public:
  WmModeTests() = default;
  WmModeTests(const WmModeTests&) = delete;
  WmModeTests& operator=(const WmModeTests&) = delete;
  ~WmModeTests() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kWmMode);
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WmModeTests, Basic) {
  auto* controller = WmModeController::Get();
  EXPECT_FALSE(controller->is_active());
  controller->Toggle();
  EXPECT_TRUE(controller->is_active());
  controller->Toggle();
  EXPECT_FALSE(controller->is_active());
}

}  // namespace ash
