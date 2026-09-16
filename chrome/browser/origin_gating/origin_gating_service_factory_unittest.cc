// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/origin_gating/origin_gating_service_factory.h"

#include <memory>

#include "chrome/test/base/testing_profile.h"
#include "components/origin_gating/core/origin_gating_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace origin_gating {
namespace {

class OriginGatingServiceFactoryTest : public ::testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(OriginGatingServiceFactoryTest, SameProfile) {
  TestingProfile profile;

  OriginGatingService* service1 =
      OriginGatingServiceFactory::GetForBrowserContext(&profile);
  EXPECT_NE(service1, nullptr);

  OriginGatingService* service2 =
      OriginGatingServiceFactory::GetForBrowserContext(&profile);
  EXPECT_EQ(service1, service2);
}

TEST_F(OriginGatingServiceFactoryTest, UnrelatedProfiles) {
  TestingProfile profile1;
  TestingProfile profile2;

  OriginGatingService* service1 =
      OriginGatingServiceFactory::GetForBrowserContext(&profile1);
  ASSERT_NE(service1, nullptr);

  OriginGatingService* service2 =
      OriginGatingServiceFactory::GetForBrowserContext(&profile2);
  ASSERT_NE(service2, nullptr);

  // Different/unrelated profiles get distinct service instances.
  EXPECT_NE(service1, service2);
}

TEST_F(OriginGatingServiceFactoryTest, OffTheRecordProfile) {
  TestingProfile profile;
  Profile* otr_profile =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);

  OriginGatingService* regular_service =
      OriginGatingServiceFactory::GetForBrowserContext(&profile);
  ASSERT_NE(regular_service, nullptr);

  OriginGatingService* otr_service =
      OriginGatingServiceFactory::GetForBrowserContext(otr_profile);
  ASSERT_NE(otr_service, nullptr);

  // Off-the-record profiles get their own distinct instance
  EXPECT_NE(regular_service, otr_service);

  // Calling again on the same OTR profile returns the same instance.
  EXPECT_EQ(otr_service,
            OriginGatingServiceFactory::GetForBrowserContext(otr_profile));
}

TEST_F(OriginGatingServiceFactoryTest, GuestProfile) {
  std::unique_ptr<TestingProfile> guest_profile =
      TestingProfile::Builder().SetGuestSession().Build();

  // Guest profiles browse, so navigations there are gateable even though
  // actor tasks cannot run in them.
  OriginGatingService* guest_service =
      OriginGatingServiceFactory::GetForBrowserContext(guest_profile.get());
  ASSERT_NE(guest_service, nullptr);

  // The user-visible Guest profile is off-the-record; it gets its own
  // instance rather than sharing the parent's.
  Profile* guest_otr_profile =
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  OriginGatingService* guest_otr_service =
      OriginGatingServiceFactory::GetForBrowserContext(guest_otr_profile);
  ASSERT_NE(guest_otr_service, nullptr);
  EXPECT_NE(guest_service, guest_otr_service);
}

}  // namespace
}  // namespace origin_gating
