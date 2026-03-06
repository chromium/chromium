// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorBackendFactoryTest : public testing::Test {
 public:
  AccessibilityAnnotatorBackendFactoryTest() = default;
  ~AccessibilityAnnotatorBackendFactoryTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      accessibility_annotator::kContentAnnotator};
};

TEST_F(AccessibilityAnnotatorBackendFactoryTest,
       ServiceCreatedForRegularProfile) {
  TestingProfile profile;
  EXPECT_NE(nullptr,
            AccessibilityAnnotatorBackendFactory::GetForProfile(&profile));
}

TEST_F(AccessibilityAnnotatorBackendFactoryTest,
       ServiceRedirectedForIncognitoProfile) {
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_EQ(AccessibilityAnnotatorBackendFactory::GetForProfile(&profile),
            AccessibilityAnnotatorBackendFactory::GetForProfile(otr_profile));
}

}  // namespace accessibility_annotator
