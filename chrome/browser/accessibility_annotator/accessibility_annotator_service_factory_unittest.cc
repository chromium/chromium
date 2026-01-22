// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_features.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorServiceFactoryTest : public testing::Test {
 public:
  AccessibilityAnnotatorServiceFactoryTest() = default;
  ~AccessibilityAnnotatorServiceFactoryTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
};

TEST_F(AccessibilityAnnotatorServiceFactoryTest,
       CreatesServiceWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      accessibility_annotator::kAccessibilityAnnotator);
  TestingProfile profile;
  EXPECT_NE(nullptr,
            AccessibilityAnnotatorServiceFactory::GetForProfile(&profile));
}

TEST_F(AccessibilityAnnotatorServiceFactoryTest, NoServiceWithFlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      accessibility_annotator::kAccessibilityAnnotator);
  TestingProfile profile;
  EXPECT_EQ(nullptr,
            AccessibilityAnnotatorServiceFactory::GetForProfile(&profile));
}

TEST_F(AccessibilityAnnotatorServiceFactoryTest,
       NoServiceForIncognitoWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      accessibility_annotator::kAccessibilityAnnotator);
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            AccessibilityAnnotatorServiceFactory::GetForProfile(otr_profile));
}

}  // namespace accessibility_annotator
