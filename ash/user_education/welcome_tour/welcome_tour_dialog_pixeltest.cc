// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/welcome_tour/welcome_tour_dialog.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class WelcomeTourDialogPixelTest : public UserEducationAshTestBase {
 private:
  // UserEducationAshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kWelcomeTour,
                              features::kWelcomeTourForceUserEligibility},
        /*disabled_features=*/{});
    UserEducationAshTestBase::SetUp();
    SimulateUserLogin("primary@test");
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WelcomeTourDialogPixelTest, Appearance) {
  ASSERT_TRUE(WelcomeTourDialog::Get());

  // Take a screenshot of the Welcome Tour dialog.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "welcome_tour_dialog",
      /*revision_number=*/3, WelcomeTourDialog::Get()));
}

}  // namespace ash
