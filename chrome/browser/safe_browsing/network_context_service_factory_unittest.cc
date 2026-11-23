// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/network_context_service_factory.h"

#include "chrome/browser/safe_browsing/network_context_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class NetworkContextServiceFactoryTest : public testing::Test {
 protected:
  NetworkContextServiceFactoryTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(NetworkContextServiceFactoryTest,
       IncognitoCanRedirectOrUseIsolatedInstance) {
  TestingProfile* regular_profile =
      profile_manager_.CreateTestingProfile("regular_profile");
  TestingProfile* second_regular_profile =
      profile_manager_.CreateTestingProfile("second_regular_profile");
  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(regular_profile);
  TestingProfile* second_incognito_profile =
      TestingProfile::Builder().BuildIncognito(second_regular_profile);

  NetworkContextService* regular_service =
      NetworkContextServiceFactory::GetForBrowserContext(
          regular_profile, /*use_same_instance_in_off_the_record=*/true);
  ASSERT_NE(nullptr, regular_service);
  EXPECT_EQ(
      regular_service,
      NetworkContextServiceFactory::GetForBrowserContext(
          regular_profile, /*use_same_instance_in_off_the_record=*/false));

  // Unmigrated callers redirect Incognito to the original profile without
  // creating an isolated off-the-record instance.
  EXPECT_EQ(regular_service, NetworkContextServiceFactory::GetForBrowserContext(
                                 incognito_profile,
                                 /*use_same_instance_in_off_the_record=*/true));
  EXPECT_EQ(
      nullptr,
      NetworkContextServiceFactory::GetForBrowserContextIfExistsForTesting(
          incognito_profile, /*use_same_instance_in_off_the_record=*/false));

  // Migrated callers receive isolated instances per off-the-record profile.
  NetworkContextService* incognito_service =
      NetworkContextServiceFactory::GetForBrowserContext(
          incognito_profile, /*use_same_instance_in_off_the_record=*/false);
  NetworkContextService* second_incognito_service =
      NetworkContextServiceFactory::GetForBrowserContext(
          second_incognito_profile,
          /*use_same_instance_in_off_the_record=*/false);
  ASSERT_NE(nullptr, incognito_service);
  ASSERT_NE(nullptr, second_incognito_service);
  EXPECT_EQ(
      incognito_service,
      NetworkContextServiceFactory::GetForBrowserContextIfExistsForTesting(
          incognito_profile, /*use_same_instance_in_off_the_record=*/false));
  EXPECT_NE(regular_service, incognito_service);
  EXPECT_NE(incognito_service, second_incognito_service);
  EXPECT_NE(regular_service->GetURLLoaderFactory(),
            incognito_service->GetURLLoaderFactory());
  EXPECT_NE(incognito_service->GetURLLoaderFactory(),
            second_incognito_service->GetURLLoaderFactory());
}

TEST_F(NetworkContextServiceFactoryTest,
       GuestSessionCanRedirectOrUseIsolatedInstance) {
  TestingProfile* regular_profile =
      profile_manager_.CreateTestingProfile("regular_profile");
  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(regular_profile);
  TestingProfile* guest_parent_profile = profile_manager_.CreateGuestProfile();
  Profile* guest_otr_profile =
      guest_parent_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  NetworkContextService* guest_parent_service =
      NetworkContextServiceFactory::GetForBrowserContext(
          guest_parent_profile, /*use_same_instance_in_off_the_record=*/true);
  ASSERT_NE(nullptr, guest_parent_service);
  EXPECT_EQ(
      guest_parent_service,
      NetworkContextServiceFactory::GetForBrowserContext(
          guest_otr_profile, /*use_same_instance_in_off_the_record=*/true));
  EXPECT_EQ(
      nullptr,
      NetworkContextServiceFactory::GetForBrowserContextIfExistsForTesting(
          guest_otr_profile, /*use_same_instance_in_off_the_record=*/false));

  NetworkContextService* incognito_service =
      NetworkContextServiceFactory::GetForBrowserContext(
          incognito_profile, /*use_same_instance_in_off_the_record=*/false);
  NetworkContextService* guest_otr_service =
      NetworkContextServiceFactory::GetForBrowserContext(
          guest_otr_profile, /*use_same_instance_in_off_the_record=*/false);
  ASSERT_NE(nullptr, incognito_service);
  ASSERT_NE(nullptr, guest_otr_service);
  EXPECT_NE(guest_parent_service, guest_otr_service);
  EXPECT_NE(incognito_service, guest_otr_service);
  EXPECT_NE(incognito_service->GetURLLoaderFactory(),
            guest_otr_service->GetURLLoaderFactory());
}

}  // namespace safe_browsing
