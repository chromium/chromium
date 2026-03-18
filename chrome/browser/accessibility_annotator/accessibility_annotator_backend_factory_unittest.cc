// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorBackendFactoryTest : public testing::Test {
 public:
  AccessibilityAnnotatorBackendFactoryTest() = default;
  ~AccessibilityAnnotatorBackendFactoryTest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                              HistoryServiceFactory::GetDefaultFactory());
    profile_ = builder.Build();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      accessibility_annotator::kAccessibilityAnnotator};
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(AccessibilityAnnotatorBackendFactoryTest,
       ServiceCreatedForRegularProfile) {
  EXPECT_NE(nullptr, AccessibilityAnnotatorBackendFactory::GetForProfile(
                         profile_.get()));
}

TEST_F(AccessibilityAnnotatorBackendFactoryTest,
       ServiceRedirectedForIncognitoProfile) {
  Profile* otr_profile = profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);

  EXPECT_EQ(AccessibilityAnnotatorBackendFactory::GetForProfile(profile_.get()),
            AccessibilityAnnotatorBackendFactory::GetForProfile(otr_profile));
}

TEST_F(AccessibilityAnnotatorBackendFactoryTest, ServiceDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(kAccessibilityAnnotator);

  EXPECT_EQ(nullptr, AccessibilityAnnotatorBackendFactory::GetForProfile(
                         profile_.get()));
}

}  // namespace accessibility_annotator
