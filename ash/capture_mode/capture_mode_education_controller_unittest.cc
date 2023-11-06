// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_education_controller.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class CaptureModeEducationControllerTest : public AshTestBase {
 public:
  CaptureModeEducationControllerTest()
      : scoped_feature_list_(features::kCaptureModeEducation) {}
  CaptureModeEducationControllerTest(
      const CaptureModeEducationControllerTest&) = delete;
  CaptureModeEducationControllerTest& operator=(
      const CaptureModeEducationControllerTest&) = delete;
  ~CaptureModeEducationControllerTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CaptureModeEducationControllerTest, Exists) {
  // Check that the default arm is enabled.
  ASSERT_EQ(features::kCaptureModeEducationParam.Get(),
            features::CaptureModeEducationParam::kShortcutNudge);
  ASSERT_TRUE(CaptureModeController::Get()->education_controller());
}

}  // namespace ash
