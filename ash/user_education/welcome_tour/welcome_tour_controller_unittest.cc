// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_controller.h"

#include <map>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/user_education/tutorial_controller.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Aliases.
using testing::_;
using testing::Contains;
using testing::Eq;
using testing::Pair;
using user_education::TutorialDescription;
using user_education::TutorialIdentifier;

}  // namespace

// WelcomeTourControllerTest ---------------------------------------------------

// Base class for tests of the `WelcomeTourController`.
class WelcomeTourControllerTest : public NoSessionAshTestBase {
 public:
  WelcomeTourControllerTest() {
    // NOTE: The `WelcomeTourController` exists only when the Welcome Tour
    // feature is enabled. Controller existence is verified in test coverage
    // for the controller's owner.
    scoped_feature_list_.InitAndEnableFeature(features::kWelcomeTour);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

// Verifies that `GetTutorialDescriptions()` returns expected values.
TEST_F(WelcomeTourControllerTest, GetTutorialDescriptions) {
  auto* welcome_tour_controller = WelcomeTourController::Get();
  ASSERT_TRUE(welcome_tour_controller);

  std::map<TutorialIdentifier, TutorialDescription>
      tutorial_descriptions_by_id =
          static_cast<TutorialController*>(welcome_tour_controller)
              ->GetTutorialDescriptions();

  // TODO(http://b/275616974): Implement tutorial descriptions.
  EXPECT_THAT(tutorial_descriptions_by_id,
              Contains(Pair(Eq("AshWelcomeTourPrototype1"), _)));
  EXPECT_THAT(tutorial_descriptions_by_id,
              Contains(Pair(Eq("AshWelcomeTourPrototype2"), _)));
}

}  // namespace ash
