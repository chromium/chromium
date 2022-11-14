// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_testing_helper.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  signin_profile_ = manager_.CreateTestingProfile(chrome::kInitialProfile);
  ASSERT_TRUE(signin_profile_);
  ASSERT_TRUE(ash::ProfileHelper::IsSigninProfile(signin_profile_));
  ASSERT_FALSE(ash::ProfileHelper::IsUserProfile(signin_profile_));
  ASSERT_FALSE(signin_profile_->IsOffTheRecord());
  signin_profile_otr_ = signin_profile_->GetPrimaryOTRProfile(true);
  ASSERT_TRUE(signin_profile_otr_);
  ASSERT_TRUE(ash::ProfileHelper::IsSigninProfile(signin_profile_otr_));
  ASSERT_FALSE(ash::ProfileHelper::IsUserProfile(signin_profile_otr_));
  ASSERT_TRUE(signin_profile_otr_->IsOffTheRecord());

  lockscreen_profile_ =
      manager_.CreateTestingProfile(chrome::kLockScreenProfile);
  ASSERT_TRUE(lockscreen_profile_);
  ASSERT_TRUE(ash::ProfileHelper::IsLockScreenProfile(lockscreen_profile_));
  ASSERT_FALSE(ash::ProfileHelper::IsUserProfile(lockscreen_profile_));
  ASSERT_FALSE(lockscreen_profile_->IsOffTheRecord());
  lockscreen_profile_otr_ = lockscreen_profile_->GetPrimaryOTRProfile(true);
  ASSERT_TRUE(lockscreen_profile_otr_);
  ASSERT_TRUE(ash::ProfileHelper::IsLockScreenProfile(lockscreen_profile_otr_));
  ASSERT_FALSE(ash::ProfileHelper::IsUserProfile(lockscreen_profile_otr_));
  ASSERT_TRUE(lockscreen_profile_otr_->IsOffTheRecord());

  lockscreenapp_profile_ =
      manager_.CreateTestingProfile(chrome::kLockScreenAppProfile);
  ASSERT_TRUE(lockscreenapp_profile_);
  ASSERT_TRUE(
      ash::ProfileHelper::IsLockScreenAppProfile(lockscreenapp_profile_));
  ASSERT_FALSE(ash::ProfileHelper::IsUserProfile(lockscreenapp_profile_));
  ASSERT_FALSE(lockscreenapp_profile_->IsOffTheRecord());
  lockscreenapp_profile_otr_ =
      lockscreenapp_profile_->GetPrimaryOTRProfile(true);
  ASSERT_TRUE(lockscreenapp_profile_otr_);
  ASSERT_TRUE(
      ash::ProfileHelper::IsLockScreenAppProfile(lockscreenapp_profile_otr_));
  ASSERT_FALSE(ash::ProfileHelper::IsUserProfile(lockscreenapp_profile_otr_));
  ASSERT_TRUE(lockscreenapp_profile_otr_->IsOffTheRecord());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
