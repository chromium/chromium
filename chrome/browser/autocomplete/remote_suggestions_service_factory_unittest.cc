// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/remote_suggestions_service_factory.h"

#include "chrome/browser/profiles/profile_testing_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

class RemoteSuggestionsServiceFactoryTest : public testing::Test,
                                            public ProfileTestingHelper {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    ProfileTestingHelper::SetUp();
  }
};

// Ensures that a service instance is created for the expected Profile types.
// See chrome/browser/profiles/profile_keyed_service_factory.md
TEST_F(RemoteSuggestionsServiceFactoryTest, ServiceInstance) {
  EXPECT_TRUE(RemoteSuggestionsServiceFactory::GetForProfile(
      regular_profile(), /*create_if_necessary=*/true));
  EXPECT_TRUE(RemoteSuggestionsServiceFactory::GetForProfile(
      incognito_profile(), /*create_if_necessary=*/true));

  EXPECT_TRUE(RemoteSuggestionsServiceFactory::GetForProfile(
      guest_profile(), /*create_if_necessary=*/true));
  EXPECT_TRUE(RemoteSuggestionsServiceFactory::GetForProfile(
      guest_profile_otr(), /*create_if_necessary=*/true));

// Service is NOT created for System Profiles. Also Android and Ash don't have
// System Profiles.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(RemoteSuggestionsServiceFactory::GetForProfile(
      system_profile(), /*create_if_necessary=*/true));
  EXPECT_FALSE(RemoteSuggestionsServiceFactory::GetForProfile(
      system_profile_otr(), /*create_if_necessary=*/true));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}
