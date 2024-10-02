// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

class FullscreenPixelTest : public AshTestBase {
 public:
  FullscreenPixelTest() : scoped_features_(chromeos::features::kJelly) {}

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Verifies the primary fullscreen of an active user session.
TEST_F(FullscreenPixelTest, VerifyDefaultPrimaryDisplay) {
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "primary_display", /*revision_number=*/7, Shell::GetPrimaryRootWindow()));
}

}  // namespace ash
