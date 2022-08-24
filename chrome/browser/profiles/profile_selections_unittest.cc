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
  ProfileSelections selections = ProfileSelections::BuildForRegularProfile();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

TEST_F(ProfileSelectionsTest, RegularAndIncognito) {
  ProfileSelections selections =
      ProfileSelections::BuildForRegularAndIncognitoNonExperimental();

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), incognito_profile());

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

TEST_F(ProfileSelectionsTest, RedirectedInIncognito) {
  ProfileSelections selections =
      ProfileSelections::BuildRedirectedInIncognitoNonExperimental();

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
  ProfileSelections selections = ProfileSelections::BuildRedirectedToOriginal();

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
  ProfileSelections selections = ProfileSelections::BuildForAllProfiles();

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
  ProfileSelections selections = ProfileSelections::BuildNoProfilesSelected();

  TestProfileSelection(selections, regular_profile(), nullptr);
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), nullptr);
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileSelection(selections, system_profile(), nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

// Testing Experimental Builders.
// Params:
// - bool force_guest: used to bypass experiment and set a fixed value to the
// Guest ProfielSelection.
// - bool force_system: used to bypass experiment and set a fixed value to the
// System ProfielSelection.
// - bool system_experiment: used to activate/deactivate the
// `kSystemProfileSelectionDefaultNone` experiment.
class ProfileSelectionsTestWithParams
    : public ProfileSelectionsTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  void SetUp() override {
    ProfileSelectionsTest::SetUp();

    // TODO(rsult): move the below code to be in the
    // `ProfileSelectionsTestWithParams` constructor, once the System Profile
    // can be created with the experiment activated.
    bool activate_system_experiment = std::get<2>(GetParam());
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

TEST_P(ProfileSelectionsTestWithParams, BuildDefault) {
  bool force_guest = std::get<0>(GetParam());
  bool force_system = std::get<1>(GetParam());

  ProfileSelections selections =
      ProfileSelections::BuildDefault(force_guest, force_system);

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), nullptr);

  TestProfileSelection(selections, guest_profile(), guest_profile());
  TestProfileSelection(selections, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  bool system_experiment = IsSystemExperimentActive();
  TestProfileSelection(
      selections, system_profile(),
      force_system || !system_experiment ? system_profile() : nullptr);
  TestProfileSelection(selections, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

TEST_P(ProfileSelectionsTestWithParams, BuildRedirectedInIncognito) {
  bool force_guest = std::get<0>(GetParam());
  bool force_system = std::get<1>(GetParam());

  ProfileSelections selections =
      ProfileSelections::BuildRedirectedInIncognito(force_guest, force_system);

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), regular_profile());

  TestProfileSelection(selections, guest_profile(), guest_profile());
  TestProfileSelection(selections, guest_profile_otr(), guest_profile());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  bool system_experiment = IsSystemExperimentActive();
  TestProfileSelection(
      selections, system_profile(),
      force_system || !system_experiment ? system_profile() : nullptr);
  TestProfileSelection(
      selections, system_profile_otr(),
      force_system || !system_experiment ? system_profile() : nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

TEST_P(ProfileSelectionsTestWithParams, BuildForRegularAndIncognito) {
  bool force_guest = std::get<0>(GetParam());
  bool force_system = std::get<1>(GetParam());

  ProfileSelections selections =
      ProfileSelections::BuildForRegularAndIncognito(force_guest, force_system);

  TestProfileSelection(selections, regular_profile(), regular_profile());
  TestProfileSelection(selections, incognito_profile(), incognito_profile());

  TestProfileSelection(selections, guest_profile(), guest_profile());
  TestProfileSelection(selections, guest_profile_otr(), guest_profile_otr());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  bool system_experiment = IsSystemExperimentActive();
  TestProfileSelection(
      selections, system_profile(),
      force_system || !system_experiment ? system_profile() : nullptr);
  TestProfileSelection(
      selections, system_profile_otr(),
      force_system || !system_experiment ? system_profile_otr() : nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

INSTANTIATE_TEST_SUITE_P(ExperimentalBuilders,
                         ProfileSelectionsTestWithParams,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));
