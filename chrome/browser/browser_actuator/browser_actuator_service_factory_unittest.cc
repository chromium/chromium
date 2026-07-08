// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {

class BrowserActuatorServiceFactoryTest : public testing::Test {
 protected:
  BrowserActuatorServiceFactoryTest() = default;
  ~BrowserActuatorServiceFactoryTest() override = default;

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserActuatorServiceFactoryTest, UsesRealService) {
  feature_list_.InitAndEnableFeature(kBrowserActuator);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);
  EXPECT_TRUE(service->IsInitialized());
}

TEST_F(BrowserActuatorServiceFactoryTest, ReturnsNullIfFeatureDisabled) {
  feature_list_.InitAndDisableFeature(kBrowserActuator);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(profile.get());
  EXPECT_EQ(nullptr, service);
}

TEST_F(BrowserActuatorServiceFactoryTest, ReturnsNullInIncognito) {
  feature_list_.InitAndEnableFeature(kBrowserActuator);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  Profile* otr_profile = profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(otr_profile);
  EXPECT_EQ(nullptr, service);
}

TEST_F(BrowserActuatorServiceFactoryTest, ReturnsNullInGuest) {
  feature_list_.InitAndEnableFeature(kBrowserActuator);
  std::unique_ptr<TestingProfile> profile =
      TestingProfile::Builder().SetGuestSession().Build();

  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(profile.get());
  EXPECT_EQ(nullptr, service);
}

}  // namespace browser_actuator
