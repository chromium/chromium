// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_dialog.h"

#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "base/functional/callback_helpers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class WelcomeTourDialogPixelTest : public AshTestBase {
 private:
  // UserEducationAshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }
};

TEST_F(WelcomeTourDialogPixelTest, Appearance) {
  WelcomeTourDialog::CreateAndShow(
      /*start_tutorial_callback=*/base::DoNothing());
  ASSERT_TRUE(WelcomeTourDialog::Get());

  // Take a screenshot of the Welcome Tour dialog.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "welcome_tour_dialog",
      /*revision_number=*/0, WelcomeTourDialog::Get()));
}

}  // namespace ash
