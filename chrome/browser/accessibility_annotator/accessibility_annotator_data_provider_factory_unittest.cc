// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_data_provider_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorDataProviderFactoryTest : public testing::Test {
 public:
  AccessibilityAnnotatorDataProviderFactoryTest() = default;
  ~AccessibilityAnnotatorDataProviderFactoryTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AccessibilityAnnotatorDataProviderFactoryTest,
       ServiceCreatedForRegularProfile) {
  TestingProfile profile;
  EXPECT_NE(nullptr,
            AccessibilityAnnotatorDataProviderFactory::GetForProfile(&profile));
}

TEST_F(AccessibilityAnnotatorDataProviderFactoryTest,
       ServiceRedirectedForIncognitoProfile) {
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_EQ(
      AccessibilityAnnotatorDataProviderFactory::GetForProfile(&profile),
      AccessibilityAnnotatorDataProviderFactory::GetForProfile(otr_profile));
}

}  // namespace accessibility_annotator
