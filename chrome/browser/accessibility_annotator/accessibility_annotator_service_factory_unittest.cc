// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/accessibility_annotator_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorServiceFactoryTest : public testing::Test {
 public:
  AccessibilityAnnotatorServiceFactoryTest() = default;
  ~AccessibilityAnnotatorServiceFactoryTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AccessibilityAnnotatorServiceFactoryTest,
       ServiceCreatedForRegularProfile) {
  base::test::ScopedFeatureList feature_list{features::kAccessibilityAnnotator};
  TestingProfile profile;
  EXPECT_NE(nullptr,
            AccessibilityAnnotatorServiceFactory::GetForProfile(&profile));
}

TEST_F(AccessibilityAnnotatorServiceFactoryTest,
       ServiceRedirectedForIncognitoProfile) {
  base::test::ScopedFeatureList feature_list{features::kAccessibilityAnnotator};
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_NE(nullptr,
            AccessibilityAnnotatorServiceFactory::GetForProfile(&profile));
  EXPECT_EQ(AccessibilityAnnotatorServiceFactory::GetForProfile(&profile),
            AccessibilityAnnotatorServiceFactory::GetForProfile(otr_profile));
}

TEST_F(AccessibilityAnnotatorServiceFactoryTest, ServiceDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAccessibilityAnnotator);
  TestingProfile profile;
  EXPECT_EQ(nullptr,
            AccessibilityAnnotatorServiceFactory::GetForProfile(&profile));
}

TEST_F(AccessibilityAnnotatorServiceFactoryTest,
       EntityDataProviderCreatedWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kAccessibilityAnnotator,
                                 features::kAccessibilityAnnotatorGetEntities},
                                {});
  TestingProfile profile;
  AccessibilityAnnotatorService* service =
      AccessibilityAnnotatorServiceFactory::GetForProfile(&profile);
  ASSERT_NE(nullptr, service);
  EXPECT_NE(nullptr, service->GetEntityDataProvider());
}

TEST_F(AccessibilityAnnotatorServiceFactoryTest,
       EntityDataProviderNullWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kAccessibilityAnnotator},
                                {features::kAccessibilityAnnotatorGetEntities});
  TestingProfile profile;
  AccessibilityAnnotatorService* service =
      AccessibilityAnnotatorServiceFactory::GetForProfile(&profile);
  ASSERT_NE(nullptr, service);
  EXPECT_EQ(nullptr, service->GetEntityDataProvider());
}

}  // namespace accessibility_annotator
