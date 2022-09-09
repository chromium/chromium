// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_testing_helper.h"

#include "chrome/test/base/testing_browser_process.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "testing/gtest/include/gtest/gtest.h"

ProfileTestingHelper::ProfileTestingHelper()
    : manager_(TestingBrowserProcess::GetGlobal()) {}

ProfileTestingHelper::~ProfileTestingHelper() {
  manager_.DeleteAllTestingProfiles();
}

void ProfileTestingHelper::SetUp() {
  ASSERT_TRUE(manager_.SetUp());

  regular_profile_ = manager_.CreateTestingProfile("testing");
  ASSERT_TRUE(regular_profile_);
  ASSERT_FALSE(regular_profile_->IsOffTheRecord());
  ASSERT_TRUE(regular_profile_->IsRegularProfile());
  incognito_profile_ = regular_profile_->GetPrimaryOTRProfile(true);
  ASSERT_TRUE(incognito_profile_);
  ASSERT_TRUE(incognito_profile_->IsOffTheRecord());
  ASSERT_TRUE(incognito_profile_->IsIncognitoProfile());

  guest_profile_ = manager_.CreateGuestProfile();
  ASSERT_TRUE(guest_profile_);
  ASSERT_FALSE(guest_profile_->IsOffTheRecord());
  ASSERT_TRUE(guest_profile_->IsGuestSession());
  guest_profile_otr_ = guest_profile_->GetPrimaryOTRProfile(true);
  ASSERT_TRUE(guest_profile_otr_);
  ASSERT_TRUE(guest_profile_otr_->IsOffTheRecord());
  ASSERT_TRUE(guest_profile_otr_->IsGuestSession());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  system_profile_ = manager_.CreateSystemProfile();
  ASSERT_TRUE(system_profile_);
  ASSERT_FALSE(system_profile_->IsOffTheRecord());
  ASSERT_TRUE(system_profile_->IsSystemProfile());
  system_profile_otr_ = system_profile_->GetPrimaryOTRProfile(true);
  ASSERT_TRUE(system_profile_otr_);
  ASSERT_TRUE(system_profile_otr_->IsOffTheRecord());
  ASSERT_TRUE(system_profile_otr_->IsSystemProfile());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}
