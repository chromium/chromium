// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace apps {

class AppPreloadServiceTest : public testing::Test {
 protected:
  AppPreloadServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppPreloadService);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppPreloadServiceTest, ServiceAccessPerProfile) {
  // We expect the App Preload Service to be available in a normal profile.
  TestingProfile::Builder profile_builder;
  auto profile = profile_builder.Build();
  auto* service = AppPreloadServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);

  // The service is unsupported in incognito.
  TestingProfile::Builder incognito_builder;
  auto* incognito_profile = incognito_builder.BuildIncognito(profile.get());
  EXPECT_EQ(nullptr,
            AppPreloadServiceFactory::GetForProfile(incognito_profile));

  // We expect the App Preload Service to be available in a guest profile.
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  auto guest_profile = guest_builder.Build();
  auto* guest_service =
      AppPreloadServiceFactory::GetForProfile(guest_profile.get());
  EXPECT_NE(nullptr, guest_service);

  // The service is not available for the OTR profile in guest mode.
  auto* guest_otr_profile =
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            AppPreloadServiceFactory::GetForProfile(guest_otr_profile));

  // We expect a different service in the Guest Session profile.
  EXPECT_NE(guest_service, service);
}

}  // namespace apps
