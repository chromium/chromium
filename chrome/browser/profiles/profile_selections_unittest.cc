// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_selections.h"

#include "chrome/browser/profiles/profile_testing_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileSelectionsTest : public testing::Test,
                              public ProfileTestingHelper {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    ProfileTestingHelper::SetUp();
  }

 protected:
  void TestProfileSelection(const ProfileSelections& selections,
                            Profile* given_profile,
                            Profile* expected_profile) {
    EXPECT_EQ(selections.ApplyProfileSelection(given_profile),
              expected_profile);
  }
};

TEST_F(ProfileSelectionsTest, DefaultConstructor) {
  // This is equivalent to `ProfileSelections()` which is private and can only
  // be called this way in production.
  ProfileSelections selections = ProfileSelections::Builder().Build();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  TestProfileSelection(selections, signin_profile(), nullptr);
  TestProfileSelection(selections, signin_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreen_profile(), nullptr);
  TestProfileSelection(selections, lockscreen_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreenapp_profile(), nullptr);
  TestProfileSelection(selections, lockscreenapp_profile_otr(), nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(ProfileSelectionsTest, CustomImplementation) {
  ProfileSelections selections =
      ProfileSelections::Builder()
          .WithRegular(ProfileSelection::kOwnInstance)
          .WithGuest(ProfileSelection::kOffTheRecordOnly)
          .WithSystem(ProfileSelection::kNone)
          .WithAshInternals(ProfileSelection::kOriginalOnly)
          .Build();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), incognito_profile());

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), guest_profile_otr());

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  TestProfileSelection(selections, signin_profile(), signin_profile());
  TestProfileSelection(selections, signin_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreen_profile(), lockscreen_profile());
  TestProfileSelection(selections, lockscreen_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreenapp_profile(),
                       lockscreenapp_profile());
  TestProfileSelection(selections, lockscreenapp_profile_otr(), nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(ProfileSelectionsTest, OnlyRegularProfile) {
  ProfileSelections selections = ProfileSelections::BuildForRegularProfile();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  TestProfileSelection(selections, signin_profile(), nullptr);
  TestProfileSelection(selections, signin_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreen_profile(), nullptr);
  TestProfileSelection(selections, lockscreen_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreenapp_profile(), nullptr);
  TestProfileSelection(selections, lockscreenapp_profile_otr(), nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(ProfileSelectionsTest, RegularAndIncognito) {
  ProfileSelections selections =
      ProfileSelections::BuildForRegularAndIncognito();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), incognito_profile());

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  TestProfileSelection(selections, signin_profile(), nullptr);
  TestProfileSelection(selections, signin_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreen_profile(), nullptr);
  TestProfileSelection(selections, lockscreen_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreenapp_profile(), nullptr);
  TestProfileSelection(selections, lockscreenapp_profile_otr(), nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(ProfileSelectionsTest, RedirectedInIncognito) {
  ProfileSelections selections =
      ProfileSelections::BuildRedirectedInIncognito();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), regular_profile());

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  TestProfileSelection(selections, signin_profile(), nullptr);
  TestProfileSelection(selections, signin_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreen_profile(), nullptr);
  TestProfileSelection(selections, lockscreen_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreenapp_profile(), nullptr);
  TestProfileSelection(selections, lockscreenapp_profile_otr(), nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(ProfileSelectionsTest, NoProfiles) {
  ProfileSelections selections = ProfileSelections::BuildNoProfilesSelected();

  TestProfileSelection(selections, regular_profile(), nullptr);
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  TestProfileSelection(selections, signin_profile(), nullptr);
  TestProfileSelection(selections, signin_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreen_profile(), nullptr);
  TestProfileSelection(selections, lockscreen_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreenapp_profile(), nullptr);
  TestProfileSelection(selections, lockscreenapp_profile_otr(), nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS)s
}
