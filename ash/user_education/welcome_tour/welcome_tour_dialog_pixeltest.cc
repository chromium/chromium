// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_helper.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/welcome_tour/welcome_tour_dialog.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class WelcomeTourDialogPixelTest
    : public UserEducationAshTestBase,
      public testing::WithParamInterface</*enable_system_blur=*/bool> {
 private:
  // UserEducationAshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.system_blur_enabled = GetParam();
    return init_params;
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kWelcomeTour,
                              features::kWelcomeTourForceUserEligibility},
        /*disabled_features=*/{});
    UserEducationAshTestBase::SetUp();
    SimulateUserLogin({"primary@test"});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WelcomeTourDialogPixelTest,
    testing::Bool());

TEST_P(WelcomeTourDialogPixelTest, Appearance) {
  ASSERT_TRUE(WelcomeTourDialog::Get());

  // Take a screenshot of the Welcome Tour dialog.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("welcome_tour_dialog"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 5 : 0,
      WelcomeTourDialog::Get()));
}

}  // namespace ash
