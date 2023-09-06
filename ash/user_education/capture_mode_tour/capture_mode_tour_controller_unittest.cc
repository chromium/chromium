// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/capture_mode_tour/capture_mode_tour_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// CaptureModeTourControllerTest -----------------------------------------------

// Base class for tests of the `CaptureModeTourController`.
class CaptureModeTourControllerTest : public UserEducationAshTestBase {
 public:
  CaptureModeTourControllerTest() {
    // NOTE: The `CaptureModeTourController` exists only when the Capture Mode
    // Tour feature is enabled. Controller existence is verified in test
    // coverage for the controller's owner.
    scoped_feature_list_.InitAndEnableFeature(features::kCaptureModeTour);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

}  // namespace ash
