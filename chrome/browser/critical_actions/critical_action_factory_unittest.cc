// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/critical_action_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace critical_actions {

class CriticalActionFactoryTest : public testing::Test {
 public:
  CriticalActionFactoryTest() {
    feature_list_.InitAndEnableFeature(features::kCriticalActionHistory);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that GetForProfile returns a valid service for a regular
// profile and returns nullptr for an off-the-record profile.
TEST_F(CriticalActionFactoryTest, GetForProfile) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  CriticalActionService* service =
      CriticalActionFactory::GetForProfile(profile.get());
  EXPECT_NE(service, nullptr);

  // Check that the off-the-record profile gets a null service.
  Profile* otrProfile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  CriticalActionService* otr_service =
      CriticalActionFactory::GetForProfile(otrProfile);
  EXPECT_EQ(otr_service, nullptr);
}

// Verifies that calling GetForProfile multiple times with the same
// profile returns the same instance.
TEST_F(CriticalActionFactoryTest, ReturnsSameInstanceForSameProfile) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  CriticalActionService* service1 =
      CriticalActionFactory::GetForProfile(profile.get());
  EXPECT_NE(service1, nullptr);

  CriticalActionService* service2 =
      CriticalActionFactory::GetForProfile(profile.get());
  EXPECT_EQ(service1, service2);
}

// Verifies that calling GetForProfile multiple times with different
// profiles returns different instances.
TEST_F(CriticalActionFactoryTest, UniqueInstancesForDifferentProfiles) {
  std::unique_ptr<TestingProfile> profile1 = TestingProfile::Builder().Build();
  CriticalActionService* service1 =
      CriticalActionFactory::GetForProfile(profile1.get());
  EXPECT_NE(service1, nullptr);

  std::unique_ptr<TestingProfile> profile2 = TestingProfile::Builder().Build();
  CriticalActionService* service2 =
      CriticalActionFactory::GetForProfile(profile2.get());
  EXPECT_NE(service2, nullptr);
  EXPECT_NE(service1, service2);
}

}  // namespace critical_actions
