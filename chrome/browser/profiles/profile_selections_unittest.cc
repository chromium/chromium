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

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  TestProfileSelection(selections, signin_profile(), signin_profile());
  TestProfileSelection(selections, signin_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreen_profile(), lockscreen_profile());
  TestProfileSelection(selections, lockscreen_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreenapp_profile(),
                       lockscreenapp_profile());
  TestProfileSelection(selections, lockscreenapp_profile_otr(), nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

TEST_F(ProfileSelectionsTest, OnlyRegularProfile) {
  ProfileSelections selections = ProfileSelections::BuildForRegularProfile();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  TestProfileSelection(selections, signin_profile(), signin_profile());
  TestProfileSelection(selections, signin_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreen_profile(), lockscreen_profile());
  TestProfileSelection(selections, lockscreen_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreenapp_profile(),
                       lockscreenapp_profile());
  TestProfileSelection(selections, lockscreenapp_profile_otr(), nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

TEST_F(ProfileSelectionsTest, RegularAndIncognito) {
  ProfileSelections selections =
      ProfileSelections::BuildForRegularAndIncognito();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), incognito_profile());

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  TestProfileSelection(selections, signin_profile(), signin_profile());
  TestProfileSelection(selections, signin_profile_otr(), signin_profile_otr());

  TestProfileSelection(selections, lockscreen_profile(), lockscreen_profile());
  TestProfileSelection(selections, lockscreen_profile_otr(),
                       lockscreen_profile_otr());

  TestProfileSelection(selections, lockscreenapp_profile(),
                       lockscreenapp_profile());
  TestProfileSelection(selections, lockscreenapp_profile_otr(),
                       lockscreenapp_profile_otr());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

TEST_F(ProfileSelectionsTest, RedirectedInIncognito) {
  ProfileSelections selections =
      ProfileSelections::BuildRedirectedInIncognito();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), regular_profile());

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  TestProfileSelection(selections, signin_profile(), signin_profile());
  TestProfileSelection(selections, signin_profile_otr(), signin_profile());

  TestProfileSelection(selections, lockscreen_profile(), lockscreen_profile());
  TestProfileSelection(selections, lockscreen_profile_otr(),
                       lockscreen_profile());

  TestProfileSelection(selections, lockscreenapp_profile(),
                       lockscreenapp_profile());
  TestProfileSelection(selections, lockscreenapp_profile_otr(),
                       lockscreenapp_profile());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

TEST_F(ProfileSelectionsTest, RedirectedToOriginal) {
  ProfileSelections selections = ProfileSelections::BuildRedirectedToOriginal();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), regular_profile());

  TestProfileSelection(selections, guest_profile(), guest_profile());
  TestProfileSelection(selections, guest_profile_otr(), guest_profile());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), system_profile());
  TestProfileSelection(selections, system_profile_otr(), system_profile());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  TestProfileSelection(selections, signin_profile(), signin_profile());
  TestProfileSelection(selections, signin_profile_otr(), signin_profile());

  TestProfileSelection(selections, lockscreen_profile(), lockscreen_profile());
  TestProfileSelection(selections, lockscreen_profile_otr(),
                       lockscreen_profile());

  TestProfileSelection(selections, lockscreenapp_profile(),
                       lockscreenapp_profile());
  TestProfileSelection(selections, lockscreenapp_profile_otr(),
                       lockscreenapp_profile());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

TEST_F(ProfileSelectionsTest, ForAllProfiles) {
  ProfileSelections selections = ProfileSelections::BuildForAllProfiles();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), incognito_profile());

  TestProfileSelection(selections, guest_profile(), guest_profile());
  TestProfileSelection(selections, guest_profile_otr(), guest_profile_otr());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), system_profile());
  TestProfileSelection(selections, system_profile_otr(), system_profile_otr());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  TestProfileSelection(selections, signin_profile(), signin_profile());
  TestProfileSelection(selections, signin_profile_otr(), signin_profile_otr());

  TestProfileSelection(selections, lockscreen_profile(), lockscreen_profile());
  TestProfileSelection(selections, lockscreen_profile_otr(),
                       lockscreen_profile_otr());

  TestProfileSelection(selections, lockscreenapp_profile(),
                       lockscreenapp_profile());
  TestProfileSelection(selections, lockscreenapp_profile_otr(),
                       lockscreenapp_profile_otr());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

TEST_F(ProfileSelectionsTest, NoProfiles) {
  ProfileSelections selections = ProfileSelections::BuildNoProfilesSelected();

  TestProfileSelection(selections, regular_profile(), nullptr);
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  TestProfileSelection(selections, signin_profile(), nullptr);
  TestProfileSelection(selections, signin_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreen_profile(), nullptr);
  TestProfileSelection(selections, lockscreen_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreenapp_profile(), nullptr);
  TestProfileSelection(selections, lockscreenapp_profile_otr(), nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)s
}

// Testing default behavior with System Profile Experiment.
// Param:
// - bool system_experiment: used to activate/deactivate the
// `kSystemProfileSelectionDefaultNone` experiment.
class ProfileSelectionsTestWithParams
    : public ProfileSelectionsTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    ProfileSelectionsTest::SetUp();

    // TODO(rsult): move the below code to be in the
    // `ProfileSelectionsTestWithParams` constructor, once the System and Guest
    // Profiles can be created with the experiment activated.
    bool activate_system_experiment = GetParam();
    feature_list_.InitWithFeatureState(kSystemProfileSelectionDefaultNone,
                                       activate_system_experiment);
  }

 protected:
  bool IsSystemExperimentActive() const {
    return base::FeatureList::IsEnabled(kSystemProfileSelectionDefaultNone);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ProfileSelectionsTestWithParams, DefaultConstructor) {
  // This is equivalent to `ProfileSelections()` which is private and can only
  // be called this way in production.
  ProfileSelections selections = ProfileSelections::Builder().Build();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  bool system_experiment = IsSystemExperimentActive();
  TestProfileSelection(selections, system_profile(),
                       system_experiment ? nullptr : system_profile());
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  TestProfileSelection(selections, signin_profile(), signin_profile());
  TestProfileSelection(selections, signin_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreen_profile(), lockscreen_profile());
  TestProfileSelection(selections, lockscreen_profile_otr(), nullptr);

  TestProfileSelection(selections, lockscreenapp_profile(),
                       lockscreenapp_profile());
  TestProfileSelection(selections, lockscreenapp_profile_otr(), nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

INSTANTIATE_TEST_SUITE_P(DefaultBehaviorWithSystemProfileExperiment,
                         ProfileSelectionsTestWithParams,
                         ::testing::Bool());
