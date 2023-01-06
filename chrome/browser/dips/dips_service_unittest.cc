// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

// Enables or disables a base::Feature.
class ScopedInitFeature {
 public:
  explicit ScopedInitFeature(const base::Feature& feature, bool enable) {
    if (enable) {
      feature_list_.InitAndEnableFeature(feature);
    } else {
      feature_list_.InitAndDisableFeature(feature);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Enables/disables the DIPS Feature and updates DIPSServiceFactory's
// ProfileSelections to match.
class ScopedInitDIPSFeature {
 public:
  explicit ScopedInitDIPSFeature(bool enable)
      : init_feature_(dips::kFeature, enable),
        override_profile_selections_(
            DIPSServiceFactory::GetInstance(),
            DIPSServiceFactory::CreateProfileSelections()) {}

 private:
  ScopedInitFeature init_feature_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      override_profile_selections_;
};

class DIPSServiceTest : public ::testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DIPSServiceTest, CreateServiceIfFeatureEnabled) {
  ScopedInitDIPSFeature init_dips(true);

  TestingProfile profile;
  EXPECT_NE(DIPSService::Get(&profile), nullptr);
}

TEST_F(DIPSServiceTest, DontCreateServiceIfFeatureDisabled) {
  ScopedInitDIPSFeature init_dips(false);

  TestingProfile profile;
  EXPECT_EQ(DIPSService::Get(&profile), nullptr);
}
