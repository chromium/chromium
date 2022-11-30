// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

// Check that VerdictCacheManagerFactory returns different object
// for off-the-record profile and regular profile.
TEST(VerdictCacheManagerFactoryTest, OffTheRecordUseDifferentService) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> testing_profile = builder.Build();

  // There should be a not null object for off-the-record profile.
  EXPECT_NE(
      nullptr,
      VerdictCacheManagerFactory::GetForProfile(
          testing_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  EXPECT_NE(
      VerdictCacheManagerFactory::GetForProfile(testing_profile.get()),
      VerdictCacheManagerFactory::GetForProfile(
          testing_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  // Different objects for different off-the-record-profiles.
  EXPECT_NE(
      VerdictCacheManagerFactory::GetForProfile(
          testing_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)),
      VerdictCacheManagerFactory::GetForProfile(
          testing_profile->GetOffTheRecordProfile(
              Profile::OTRProfileID::CreateUniqueForTesting(),
              /*create_if_needed=*/true)));
}

}  // namespace safe_browsing
