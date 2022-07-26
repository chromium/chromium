// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_selections.h"

#include "chrome/browser/profiles/profile_testing_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileSelectionsTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    profile_testing_helper_.SetUp();
  }

 protected:
  void TestProfileSelection(const ProfileSelections& selections,
                            Profile* given_profile,
                            Profile* expected_profile) {
    EXPECT_EQ(selections.ApplyProfileSelection(given_profile),
              expected_profile);
  }

  TestingProfile* regular_profile() {
    return profile_testing_helper_.regular_profile();
  }
  Profile* incognito_profile() {
    return profile_testing_helper_.incognito_profile();
  }

  TestingProfile* guest_profile() {
    return profile_testing_helper_.guest_profile();
  }
  Profile* guest_profile_otr() {
    return profile_testing_helper_.guest_profile_otr();
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestingProfile* system_profile() {
    return profile_testing_helper_.system_profile();
  }
  Profile* system_profile_otr() {
    return profile_testing_helper_.system_profile_otr();
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

 private:
  ProfileTestingHelper profile_testing_helper_;
};

TEST_F(ProfileSelectionsTest, DefaultImplementation) {
  ProfileSelections selections = ProfileSelections::BuildDefault();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), guest_profile());
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), system_profile());
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

TEST_F(ProfileSelectionsTest, CustomImplementation) {
  ProfileSelections selections =
      ProfileSelections::Builder()
          .WithRegular(ProfileSelection::kOwnInstance)
          .WithGuest(ProfileSelection::kOffTheRecordOnly)
          .WithSystem(ProfileSelection::kNone)
          .Build();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), incognito_profile());

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), guest_profile_otr());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

TEST_F(ProfileSelectionsTest, OnlyRegularProfile) {
  ProfileSelections selections =
      ProfileSelections::BuildServicesForRegularProfile();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

TEST_F(ProfileSelectionsTest, RedirectedInIncognito) {
  ProfileSelections selections =
      ProfileSelections::BuildServicesRedirectedInIncognito();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), regular_profile());

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

TEST_F(ProfileSelectionsTest, RedirectedToOriginal) {
  ProfileSelections selections =
      ProfileSelections::BuildServicesRedirectedToOriginal();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), regular_profile());

  TestProfileSelection(selections, guest_profile(), guest_profile());
  TestProfileSelection(selections, guest_profile_otr(), guest_profile());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), system_profile());
  TestProfileSelection(selections, system_profile_otr(), system_profile());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

TEST_F(ProfileSelectionsTest, ForAllProfiles) {
  ProfileSelections selections =
      ProfileSelections::BuildServicesForAllProfiles();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), incognito_profile());

  TestProfileSelection(selections, guest_profile(), guest_profile());
  TestProfileSelection(selections, guest_profile_otr(), guest_profile_otr());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), system_profile());
  TestProfileSelection(selections, system_profile_otr(), system_profile_otr());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

TEST_F(ProfileSelectionsTest, NoProfiles) {
  ProfileSelections selections =
      ProfileSelections::BuildNoServicesForAllProfiles();

  TestProfileSelection(selections, regular_profile(), nullptr);
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}
