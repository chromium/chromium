// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class AppDeduplicationServiceTest : public testing::Test {
 protected:
  AppDeduplicationServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAppDeduplicationService);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppDeduplicationServiceTest, ServiceAccessPerProfile) {
  TestingProfile::Builder profile_builder;

  // We expect an App Deduplication Service in a regular profile.
  auto profile = profile_builder.Build();
  EXPECT_TRUE(apps::AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(profile.get()));
  auto* service =
      apps::AppDeduplicationServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);

  // We expect App Deduplication Service to be unsupported in incognito.
  TestingProfile::Builder incognito_builder;
  auto* incognito_profile = incognito_builder.BuildIncognito(profile.get());
  EXPECT_FALSE(
      apps::AppDeduplicationServiceFactory::
          IsAppDeduplicationServiceAvailableForProfile(incognito_profile));
  EXPECT_EQ(nullptr, apps::AppDeduplicationServiceFactory::GetForProfile(
                         incognito_profile));

  // We expect a different App Deduplication Service in the Guest Session
  // profile.
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  auto guest_profile = guest_builder.Build();

  // App Deduplication Service is not available for original profile.
  EXPECT_FALSE(
      apps::AppDeduplicationServiceFactory::
          IsAppDeduplicationServiceAvailableForProfile(guest_profile.get()));
  EXPECT_EQ(nullptr, apps::AppDeduplicationServiceFactory::GetForProfile(
                         guest_profile.get()));

  // App Deduplication Service is available for OTR profile in Guest mode.
  auto* guest_otr_profile =
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_TRUE(
      apps::AppDeduplicationServiceFactory::
          IsAppDeduplicationServiceAvailableForProfile(guest_otr_profile));
  auto* guest_otr_service =
      apps::AppDeduplicationServiceFactory::GetForProfile(guest_otr_profile);
  EXPECT_NE(nullptr, guest_otr_service);
  EXPECT_NE(guest_otr_service, service);
}

}  // namespace apps
