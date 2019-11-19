// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

// Check that AdvancedProtectionStatusManagerFactory returns the same object
// for both off-the-record profile and regular profile.
TEST(AdvancedProtectionStatusManagerFactoryTest, OffTheRecordUseSameService) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> testing_profile = builder.Build();

  // The regular profile and the off-the-record profile must be different.
  ASSERT_NE(testing_profile.get(), testing_profile->GetOffTheRecordProfile());

  EXPECT_EQ(AdvancedProtectionStatusManagerFactory::GetForProfile(
                testing_profile.get()),
            AdvancedProtectionStatusManagerFactory::GetForProfile(
                testing_profile->GetOffTheRecordProfile()));
}

}  // namespace safe_browsing
