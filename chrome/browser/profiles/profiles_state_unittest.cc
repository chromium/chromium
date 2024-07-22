// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profiles_state.h"

#include <string>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/common/features.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "profiles_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if !BUILDFLAG(IS_ANDROID)
// Params for the parameterized test IsGuestModeRequestedTest.
struct IsGuestModeRequestedTestParams {
  bool has_switch;
  bool pref_enforced;
  bool pref_enabled;
  bool expected_guest_mode_requested;
};

// clang-format off
const IsGuestModeRequestedTestParams kIsGuestModeRequestedParams[] {
  // has_switch | pref_enforced | pref_enabled | expected_guest_mode_requested
  {  true,        true,           true,          true},
  {  true,        true,           false,         false},
  {  true,        false,          true,          true},
  {  true,        false,          false,         false},
  {  false,       true,           true,          true},
  {  false,       true,           false,         false},
  {  false,       false,          true,          false},
  {  false,       false,          false,         false},
};
// clang-format on
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
class IsGuestModeRequestedTest
    : public testing::TestWithParam<IsGuestModeRequestedTestParams> {};

TEST_P(IsGuestModeRequestedTest, Requested) {
  TestingPrefServiceSimple local_state;
  local_state.registry()->RegisterBooleanPref(prefs::kBrowserGuestModeEnforced,
                                              false);
  local_state.registry()->RegisterBooleanPref(prefs::kBrowserGuestModeEnabled,
                                              false);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  // Set parameters.
  IsGuestModeRequestedTestParams params = GetParam();
  local_state.SetBoolean(prefs::kBrowserGuestModeEnforced,
                         params.pref_enforced);
  local_state.SetBoolean(prefs::kBrowserGuestModeEnabled, params.pref_enabled);
  if (params.has_switch)
    command_line.AppendSwitch("guest");
  // Check expectation.
  EXPECT_EQ(params.expected_guest_mode_requested,
            profiles::IsGuestModeRequested(command_line, &local_state,
                                           /*show_warning=*/false));
}

INSTANTIATE_TEST_SUITE_P(ProfilesState,
                         IsGuestModeRequestedTest,
                         testing::ValuesIn(kIsGuestModeRequestedParams));

class IsGuestModeEnabledTest
    : public testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  IsGuestModeEnabledTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal(),
                         &testing_local_state_),
        testing_local_state_(TestingBrowserProcess::GetGlobal()) {
    testing_local_state_.Get()->SetBoolean(prefs::kBrowserGuestModeEnabled,
                                           BrowserGuestModePrefValue());
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    scoped_feature_list_.InitWithFeatureState(
        supervised_user::kHideGuestModeForSupervisedUsers,
        HideGuestModeForSupervisedUsersFeatureEnabled());
#endif
  }

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  bool BrowserGuestModePrefValue() { return get<0>(GetParam()); }

  bool HideGuestModeForSupervisedUsersFeatureEnabled() {
    return get<1>(GetParam());
  }

  Profile* CreateNormalProfile() {
    return CreateProfile(/*is_subject_to_parental_controls=*/false);
  }

  Profile* CreateSupervisedProfile() {
    return CreateProfile(/*is_subject_to_parental_controls=*/true);
  }

 private:
  Profile* CreateProfile(bool is_subject_to_parental_controls) {
    const std::string profile_name = base::StringPrintf(
        "Profile %zu",
        profile_manager_.profile_manager()->GetNumberOfProfiles());
    const std::string email = base::StringPrintf(
        "account%zu@gmail.com",
        profile_manager_.profile_manager()->GetNumberOfProfiles());

    Profile* profile = profile_manager_.CreateTestingProfile(
        profile_name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(profile_name),
        /*avatar_id=*/0, /*testing_factories=*/{},
        /*is_supervised_profile=*/is_subject_to_parental_controls,
        /*is_new_profile=*/std::nullopt,
        /*policy_service=*/std::nullopt, /*is_main_profile=*/false,
        /*shared_url_loader_factory=*/nullptr);

    return profile;
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  ScopedTestingLocalState testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(IsGuestModeEnabledTest, NoProfiles) {
  EXPECT_EQ(profiles::IsGuestModeEnabled(), BrowserGuestModePrefValue());
}

TEST_P(IsGuestModeEnabledTest, OneNormalProfile) {
  Profile* profile = CreateNormalProfile();

  EXPECT_EQ(profiles::IsGuestModeEnabled(), BrowserGuestModePrefValue());
  EXPECT_EQ(profiles::IsGuestModeEnabled(*profile),
            BrowserGuestModePrefValue());
}

TEST_P(IsGuestModeEnabledTest, OneSupervisedProfile) {
  Profile* profile = CreateSupervisedProfile();

  EXPECT_EQ(profiles::IsGuestModeEnabled(),
            BrowserGuestModePrefValue() &&
                !HideGuestModeForSupervisedUsersFeatureEnabled());
  EXPECT_EQ(profiles::IsGuestModeEnabled(*profile),
            BrowserGuestModePrefValue() &&
                !HideGuestModeForSupervisedUsersFeatureEnabled());
}

TEST_P(IsGuestModeEnabledTest, MixedProfiles) {
  Profile* normal_profile = CreateNormalProfile();
  Profile* supervised_profile = CreateSupervisedProfile();

  EXPECT_EQ(profiles::IsGuestModeEnabled(),
            BrowserGuestModePrefValue() &&
                !HideGuestModeForSupervisedUsersFeatureEnabled());
  EXPECT_EQ(profiles::IsGuestModeEnabled(*normal_profile),
            BrowserGuestModePrefValue());
  EXPECT_EQ(profiles::IsGuestModeEnabled(*supervised_profile),
            BrowserGuestModePrefValue() &&
                !HideGuestModeForSupervisedUsersFeatureEnabled());
}

INSTANTIATE_TEST_SUITE_P(
    ProfilesState,
    IsGuestModeEnabledTest,
    testing::Combine(
        /*BrowserGuestModePrefValue*/ testing::Bool(),
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
        /*HideGuestModeForSupervisedUsersFeatureEnabled*/ testing::Bool()));
#else
        /*HideGuestModeForSupervisedUsersFeatureEnabled*/ testing::Values(
            false)));
#endif

#endif  // !BUILDFLAG(IS_ANDROID)
