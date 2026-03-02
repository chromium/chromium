// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotation_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class AccessibilityAnnotationServiceFactoryTest : public testing::Test {
 public:
  AccessibilityAnnotationServiceFactoryTest() = default;
  ~AccessibilityAnnotationServiceFactoryTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AccessibilityAnnotationServiceFactoryTest,
       ServiceCreatedForRegularProfile) {
  base::test::ScopedFeatureList feature_list{kAccessibilityAnnotator};
  TestingProfile profile;
  EXPECT_NE(nullptr,
            AccessibilityAnnotationServiceFactory::GetForProfile(&profile));
}

TEST_F(AccessibilityAnnotationServiceFactoryTest,
       ServiceRedirectedForIncognitoProfile) {
  base::test::ScopedFeatureList feature_list{kAccessibilityAnnotator};
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_NE(nullptr,
            AccessibilityAnnotationServiceFactory::GetForProfile(&profile));
  EXPECT_EQ(AccessibilityAnnotationServiceFactory::GetForProfile(&profile),
            AccessibilityAnnotationServiceFactory::GetForProfile(otr_profile));
}

TEST_F(AccessibilityAnnotationServiceFactoryTest, ServiceDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kAccessibilityAnnotator);
  TestingProfile profile;
  EXPECT_EQ(nullptr,
            AccessibilityAnnotationServiceFactory::GetForProfile(&profile));
}

}  // namespace accessibility_annotator
