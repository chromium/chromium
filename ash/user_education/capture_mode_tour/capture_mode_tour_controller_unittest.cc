// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/capture_mode_tour/capture_mode_tour_controller.h"

#include <map>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/user_education/user_education_types.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/tutorial_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Aliases.
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Pair;
using user_education::TutorialDescription;

}  // namespace

// CaptureModeTourControllerTest -----------------------------------------------

// Base class for tests of the `CaptureModeTourController`.
class CaptureModeTourControllerTest : public NoSessionAshTestBase {
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

// Verifies that `GetTutorialDescriptions()` returns expected values.
TEST_F(CaptureModeTourControllerTest, GetTutorialDescriptions) {
  auto* capture_mode_tour_controller = CaptureModeTourController::Get();
  ASSERT_TRUE(capture_mode_tour_controller);

  std::map<TutorialId, TutorialDescription> tutorial_descriptions_by_id =
      static_cast<UserEducationFeatureController*>(capture_mode_tour_controller)
          ->GetTutorialDescriptions();

  // TODO(http://b/275892879): Implement tutorial descriptions.
  EXPECT_THAT(tutorial_descriptions_by_id,
              ElementsAre(Pair(Eq(TutorialId::kCaptureModeTourPrototype1), _),
                          Pair(Eq(TutorialId::kCaptureModeTourPrototype2), _)));
}

}  // namespace ash
