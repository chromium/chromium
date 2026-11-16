// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profiles_state.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/shared_preferences/shared_preferences_manager.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/preferences/android/chrome_shared_preferences.h"
#include "chrome/browser/startup_data.h"
#endif

namespace {

constexpr char kProfile1[] = "Profile 1";
#if BUILDFLAG(IS_ANDROID)
constexpr char kExistingProfile[] = "Profile 2";
#endif

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

class IsGuestModeEnabledTest : public testing::TestWithParam<bool> {
 public:
  IsGuestModeEnabledTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
        prefs::kBrowserGuestModeEnabled, BrowserGuestModePrefValue());
  }

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  bool BrowserGuestModePrefValue() { return GetParam(); }

  bool HideGuestModeForSupervisedUsersFeatureEnabled() {
    return BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN);
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

    Profile* profile = profile_manager_.CreateTestingProfile(
        profile_name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(profile_name),
        /*avatar_id=*/0, /*testing_factories=*/{},
        /*is_supervised_profile=*/is_subject_to_parental_controls,
        /*is_new_profile=*/std::nullopt,
        /*policy_service=*/std::nullopt,
        /*shared_url_loader_factory=*/nullptr);

    return profile;
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
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

INSTANTIATE_TEST_SUITE_P(ProfilesState,
                         IsGuestModeEnabledTest,
                         testing::Bool());

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// On Android, multiple profiles are only enabled with kAndroidMultiprofile.
TEST(ProfilesStateTest, MultipleProfilesFollowAndroidMultiprofileFeature) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chrome::android::kAndroidMultiprofile);
    EXPECT_TRUE(profiles::IsMultipleProfilesEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(chrome::android::kAndroidMultiprofile);
    EXPECT_FALSE(profiles::IsMultipleProfilesEnabled());
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

class GetLastUsedProfileBaseNameTest : public testing::Test {
 public:
  GetLastUsedProfileBaseNameTest() {
    local_state_.registry()->RegisterStringPref(prefs::kProfileLastUsed,
                                                std::string());
    local_state_.registry()->RegisterDictionaryPref(prefs::kProfileAttributes);
#if BUILDFLAG(IS_ANDROID)
    feature_list_.InitAndEnableFeature(
        features::kAndroidMultiprofileStartupData);
#endif
  }

 protected:
  void SetLastUsedProfile(const std::string& base_name) {
    local_state_.SetString(prefs::kProfileLastUsed, base_name);
  }

  void AddKnownProfile(const std::string& base_name) {
    ScopedDictPrefUpdate(&local_state_, prefs::kProfileAttributes)
        ->Set(base_name, base::DictValue());
  }

  base::FilePath GetLastUsedProfileBaseName() {
    return profiles::GetLastUsedProfileBaseName(local_state_);
  }

#if BUILDFLAG(IS_ANDROID)
  void TestProfileBaseNameForStartupImpl(
      bool startup_data_enabled,
      const std::string& command_line_profile,
      const std::string& expected_profile);
#endif

  TestingPrefServiceSimple local_state_;
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list_;
#endif
};

TEST_F(GetLastUsedProfileBaseNameTest, ReturnsInitialProfileWhenUnset) {
  EXPECT_EQ(base::FilePath::FromASCII(chrome::kInitialProfile),
            GetLastUsedProfileBaseName());
}

TEST_F(GetLastUsedProfileBaseNameTest, IgnoresSystemProfile) {
  const std::string system_profile =
      base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe();
  SetLastUsedProfile(system_profile);
  AddKnownProfile(system_profile);

  EXPECT_EQ(base::FilePath::FromASCII(chrome::kInitialProfile),
            GetLastUsedProfileBaseName());
}

TEST_F(GetLastUsedProfileBaseNameTest, ReturnsKnownLastUsedProfile) {
  SetLastUsedProfile(kProfile1);
  AddKnownProfile(kProfile1);

  EXPECT_EQ(base::FilePath::FromASCII(kProfile1), GetLastUsedProfileBaseName());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(GetLastUsedProfileBaseNameTest, AndroidIgnoresUnknownProfile) {
  SetLastUsedProfile(kProfile1);
  EXPECT_EQ(base::FilePath::FromASCII(chrome::kInitialProfile),
            GetLastUsedProfileBaseName());

  SetLastUsedProfile("..");
  EXPECT_EQ(base::FilePath::FromASCII(chrome::kInitialProfile),
            GetLastUsedProfileBaseName());
}

TEST_F(GetLastUsedProfileBaseNameTest,
       AndroidReturnsUnknownLastUsedProfileWhenDisabled) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(features::kAndroidMultiprofileStartupData);
  SetLastUsedProfile(kProfile1);

  EXPECT_EQ(base::FilePath::FromASCII(kProfile1), GetLastUsedProfileBaseName());
}

TEST_F(GetLastUsedProfileBaseNameTest, AndroidStartsWithLastUsedProfile) {
  TestProfileBaseNameForStartupImpl(/*startup_data_enabled=*/true,
                                    /*command_line_profile=*/std::string(),
                                    /*expected_profile=*/kProfile1);
}

TEST_F(GetLastUsedProfileBaseNameTest,
       AndroidStartsWithInitialProfileWhenDisabled) {
  TestProfileBaseNameForStartupImpl(
      /*startup_data_enabled=*/false,
      /*command_line_profile=*/kExistingProfile,
      /*expected_profile=*/chrome::kInitialProfile);
}

TEST_F(GetLastUsedProfileBaseNameTest, AndroidStartsWithCommandLineProfile) {
  TestProfileBaseNameForStartupImpl(/*startup_data_enabled=*/true,
                                    /*command_line_profile=*/kExistingProfile,
                                    /*expected_profile=*/kExistingProfile);
}

TEST_F(GetLastUsedProfileBaseNameTest,
       AndroidIgnoresNonexistentCommandLineProfile) {
  TestProfileBaseNameForStartupImpl(/*startup_data_enabled=*/true,
                                    /*command_line_profile=*/"Profile 3",
                                    /*expected_profile=*/kProfile1);
}

void GetLastUsedProfileBaseNameTest::TestProfileBaseNameForStartupImpl(
    bool startup_data_enabled,
    const std::string& command_line_profile,
    const std::string& expected_profile) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates(
      {{chrome::android::kAndroidMultiprofile, true},
       {features::kAndroidMultiprofileStartupData, startup_data_enabled}});
  SetLastUsedProfile(kProfile1);
  AddKnownProfile(kProfile1);
  AddKnownProfile(kExistingProfile);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  if (!command_line_profile.empty()) {
    command_line.AppendSwitchASCII(switches::kProfileDirectory,
                                   command_line_profile);
  }

  EXPECT_EQ(base::FilePath::FromASCII(expected_profile),
            StartupData::GetProfileBaseNameForStartup(
                local_state_, command_line, base::FilePath()));
}

class LastUsedProfileBaseNameTest : public testing::Test {
 protected:
  void SetUp() override {
    GetSharedPrefs().RemoveKey(
        profiles::kLastUsedProfileBaseNameSharedPreferenceKey);
  }

  void TearDown() override {
    GetSharedPrefs().RemoveKey(
        profiles::kLastUsedProfileBaseNameSharedPreferenceKey);
  }

  // Returns a new instance each time, since it holds a local JNI reference.
  base::android::SharedPreferencesManager GetSharedPrefs() {
    return android::shared_preferences::GetChromeSharedPreferences();
  }

  void TestSetLastUsedProfileWritesBaseNameImpl(bool profile_exists);
};

TEST_F(LastUsedProfileBaseNameTest, OverwritesStaleBaseName) {
  GetSharedPrefs().WriteString(
      profiles::kLastUsedProfileBaseNameSharedPreferenceKey, "Profile 2");

  profiles::WriteLastUsedProfileBaseName(base::FilePath::FromASCII(kProfile1));

  EXPECT_EQ(kProfile1,
            GetSharedPrefs().ReadString(
                profiles::kLastUsedProfileBaseNameSharedPreferenceKey,
                std::string()));
}

TEST_F(LastUsedProfileBaseNameTest, SetLastUsedProfileWritesBaseName) {
  TestSetLastUsedProfileWritesBaseNameImpl(/*profile_exists=*/true);
}

TEST_F(LastUsedProfileBaseNameTest,
       SetLastUsedProfileWritesInitialProfileForUnknownProfile) {
  TestSetLastUsedProfileWritesBaseNameImpl(/*profile_exists=*/false);
}

void LastUsedProfileBaseNameTest::TestSetLastUsedProfileWritesBaseNameImpl(
    bool profile_exists) {
  base::test::ScopedFeatureList feature_list(
      features::kAndroidMultiprofileStartupData);
  if (profile_exists) {
    ScopedDictPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                         prefs::kProfileAttributes)
        ->Set(kProfile1, base::DictValue());
  }

  profiles::SetLastUsedProfile(base::FilePath::FromASCII(kProfile1));

  EXPECT_EQ(profile_exists ? kProfile1 : chrome::kInitialProfile,
            GetSharedPrefs().ReadString(
                profiles::kLastUsedProfileBaseNameSharedPreferenceKey,
                std::string()));
}
#else
TEST_F(GetLastUsedProfileBaseNameTest, ReturnsUnknownLastUsedProfile) {
  SetLastUsedProfile(kProfile1);

  EXPECT_EQ(base::FilePath::FromASCII(kProfile1), GetLastUsedProfileBaseName());
}
#endif  // BUILDFLAG(IS_ANDROID)

class GetProfileDirFromCommandLineTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    local_state_.registry()->RegisterDictionaryPref(prefs::kProfileAttributes);
  }

  base::FilePath GetProfileDir() {
    return profiles::GetProfileDirFromCommandLine(
        command_line_, user_data_dir_.GetPath(), local_state_);
  }

  void TestIgnoreIfNotExistsImpl(bool directory_exists);
#if BUILDFLAG(IS_ANDROID)
  void TestStartupDataRequiresExistingProfileImpl(bool profile_exists);
#endif

  base::ScopedTempDir user_data_dir_;
  base::CommandLine command_line_{base::CommandLine::NO_PROGRAM};
  TestingPrefServiceSimple local_state_;
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list_{
      chrome::android::kAndroidMultiprofile};
#endif
};

TEST_F(GetProfileDirFromCommandLineTest, ReturnsEmptyPathWithoutSwitch) {
  EXPECT_TRUE(GetProfileDir().empty());
}

TEST_F(GetProfileDirFromCommandLineTest, ReturnsProfileDirectory) {
  command_line_.AppendSwitchASCII(switches::kProfileDirectory, kProfile1);

  EXPECT_EQ(base::FilePath::FromASCII(kProfile1), GetProfileDir());
}

TEST_F(GetProfileDirFromCommandLineTest,
       ReturnsExistingDirectoryWithIgnoreIfNotExists) {
  TestIgnoreIfNotExistsImpl(/*directory_exists=*/true);
}

TEST_F(GetProfileDirFromCommandLineTest,
       IgnoresMissingDirectoryWithIgnoreIfNotExists) {
  TestIgnoreIfNotExistsImpl(/*directory_exists=*/false);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(GetProfileDirFromCommandLineTest,
       AndroidReturnsEmptyPathWithoutMultipleProfiles) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(chrome::android::kAndroidMultiprofile);
  command_line_.AppendSwitchASCII(switches::kProfileDirectory, kProfile1);

  EXPECT_TRUE(GetProfileDir().empty());
}

TEST_F(GetProfileDirFromCommandLineTest,
       AndroidReturnsExistingProfileWithStartupData) {
  TestStartupDataRequiresExistingProfileImpl(/*profile_exists=*/true);
}

TEST_F(GetProfileDirFromCommandLineTest,
       AndroidIgnoresNonexistentProfileWithStartupData) {
  TestStartupDataRequiresExistingProfileImpl(/*profile_exists=*/false);
}

TEST_F(GetProfileDirFromCommandLineTest,
       AndroidRejectsAbsolutePathWithStartupData) {
  base::test::ScopedFeatureList feature_list(
      features::kAndroidMultiprofileStartupData);
  command_line_.AppendSwitchPath(switches::kProfileDirectory,
                                 user_data_dir_.GetPath());

  EXPECT_TRUE(GetProfileDir().empty());
}
#endif  // BUILDFLAG(IS_ANDROID)

void GetProfileDirFromCommandLineTest::TestIgnoreIfNotExistsImpl(
    bool directory_exists) {
  if (directory_exists) {
    ASSERT_TRUE(
        base::CreateDirectory(user_data_dir_.GetPath().AppendASCII(kProfile1)));
  }
  command_line_.AppendSwitchASCII(switches::kProfileDirectory, kProfile1);
  command_line_.AppendSwitch(switches::kIgnoreProfileDirectoryIfNotExists);

  EXPECT_EQ(directory_exists ? base::FilePath::FromASCII(kProfile1)
                             : base::FilePath(),
            GetProfileDir());
}

#if BUILDFLAG(IS_ANDROID)
void GetProfileDirFromCommandLineTest::
    TestStartupDataRequiresExistingProfileImpl(bool profile_exists) {
  base::test::ScopedFeatureList feature_list(
      features::kAndroidMultiprofileStartupData);
  if (profile_exists) {
    ScopedDictPrefUpdate(&local_state_, prefs::kProfileAttributes)
        ->Set(kProfile1, base::DictValue());
  }
  command_line_.AppendSwitchASCII(switches::kProfileDirectory, kProfile1);

  EXPECT_EQ(
      profile_exists ? base::FilePath::FromASCII(kProfile1) : base::FilePath(),
      GetProfileDir());
}
#endif  // BUILDFLAG(IS_ANDROID)
