// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class AppTimeControllerTest : public testing::Test {
 protected:
  AppTimeControllerTest() = default;
  AppTimeControllerTest(const AppTimeControllerTest&) = delete;
  AppTimeControllerTest& operator=(const AppTimeControllerTest&) = delete;
  ~AppTimeControllerTest() override = default;

  void EnablePerAppTimeLimits() {
    scoped_feature_list_.InitAndEnableFeature(features::kPerAppTimeLimits);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppTimeControllerTest, EnableFeature) {
  EnablePerAppTimeLimits();
  EXPECT_TRUE(AppTimeController::ArePerAppTimeLimitsEnabled());
}

}  // namespace chromeos
