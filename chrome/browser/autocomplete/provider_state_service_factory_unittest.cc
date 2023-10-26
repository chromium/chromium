// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/provider_state_service_factory.h"

#include "chrome/browser/profiles/profile_testing_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProviderStateServiceFactoryTest : public testing::Test {
 protected:
  ProfileTestingHelper profile_testing_helper_;
};

TEST_F(ProviderStateServiceFactoryTest, PrefEnabledReturnsValidService) {
  profile_testing_helper_.SetUp();

  EXPECT_TRUE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.regular_profile()));
  EXPECT_FALSE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.incognito_profile()));

  EXPECT_FALSE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.guest_profile()));
  EXPECT_FALSE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.guest_profile_otr()));

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.system_profile()));
  EXPECT_FALSE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.system_profile_otr()));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.signin_profile()));
  EXPECT_FALSE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.signin_profile_otr()));

  EXPECT_FALSE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.lockscreen_profile()));
  EXPECT_FALSE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.lockscreen_profile_otr()));

  EXPECT_FALSE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.lockscreenapp_profile()));
  EXPECT_FALSE(ProviderStateServiceFactory::GetForProfile(
      profile_testing_helper_.lockscreenapp_profile_otr()));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
