// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/autocorrect_prefs.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/ash/input_method/autocorrect_enums.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

constexpr char kUsEnglish[] = "xkb:us::eng";
constexpr char kBrazilPortuguese[] = "xkb:br::por";
constexpr char kLatinAmericaSpanish[] = "xkb:latam::spa";
constexpr char kFranceFrench[] = "xkb:fr::fra";

void SetManagedPkAutocorrectAllowed(Profile& profile, bool allowed) {
  profile.GetPrefs()->SetBoolean(
      prefs::kManagedPhysicalKeyboardAutocorrectAllowed, allowed);
}

void SetAutocorrectLevelTo(Profile& profile,
                           const std::string& pref_name,
                           const std::string& engine_id,
                           int autocorrect_level) {
  base::Value::Dict input_method_setting;
  input_method_setting.SetByDottedPath(
      base::StrCat({engine_id, ".", pref_name}), autocorrect_level);
  profile.GetPrefs()->Set(::prefs::kLanguageInputMethodSpecificSettings,
                          base::Value(std::move(input_method_setting)));
}

void SetPkAutocorrectLevelTo(Profile& profile,
                             const std::string& engine_id,
                             int autocorrect_level) {
  SetAutocorrectLevelTo(/*profile=*/profile,
                        /*pref_name=*/"physicalKeyboardAutoCorrectionLevel",
                        /*engine_id=*/engine_id,
                        /*autocorrect_level=*/autocorrect_level);
}

void SetVkAutocorrectLevelTo(Profile& profile,
                             const std::string& engine_id,
                             int autocorrect_level) {
  SetAutocorrectLevelTo(/*profile=*/profile,
                        /*pref_name=*/"virtualKeyboardAutoCorrectionLevel",
                        /*engine_id=*/engine_id,
                        /*autocorrect_level=*/autocorrect_level);
}

class AutocorrectPrefsTest : public ::testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
};

TEST_F(AutocorrectPrefsTest,
       FetchesTheCorrectPkValueWhenAutocorrectLevelIsNotSet) {
  EXPECT_EQ(
      GetPhysicalKeyboardAutocorrectPref(*(profile_.GetPrefs()), kUsEnglish),
      AutocorrectPreference::kDefault);
}

TEST_F(AutocorrectPrefsTest,
       FetchesTheCorrectVkValueWhenAutocorrectLevelIsNotSet) {
  EXPECT_EQ(
      GetVirtualKeyboardAutocorrectPref(*(profile_.GetPrefs()), kUsEnglish),
      AutocorrectPreference::kDefault);
}

struct AutocorrectPrefCase {
  std::string test_name;
  int autocorrect_level;
  AutocorrectPreference autocorrect_pref;
};

class FetchesAutocorrectPreference
    : public AutocorrectPrefsTest,
      public testing::WithParamInterface<AutocorrectPrefCase> {};

TEST_P(FetchesAutocorrectPreference, AndReturnsCorrectPkPref) {
  const AutocorrectPrefCase& test_case = GetParam();
  SetPkAutocorrectLevelTo(profile_,
                          /*engine_id=*/kUsEnglish,
                          /*autocorrect_level=*/test_case.autocorrect_level);

  EXPECT_EQ(
      GetPhysicalKeyboardAutocorrectPref(*(profile_.GetPrefs()), kUsEnglish),
      test_case.autocorrect_pref);
}

TEST_P(FetchesAutocorrectPreference, AndScopesReturnedPkPreferenceToEngineId) {
  const AutocorrectPrefCase& test_case = GetParam();
  SetPkAutocorrectLevelTo(profile_,
                          /*engine_id=*/kUsEnglish,
                          /*autocorrect_level=*/test_case.autocorrect_level);

  // The above preference is set for the US English engine only. Queries for
  // the autocorrect level on any other engine should return the default.
  EXPECT_EQ(GetPhysicalKeyboardAutocorrectPref(*(profile_.GetPrefs()),
                                               kBrazilPortuguese),
            AutocorrectPreference::kDefault);
}

TEST_P(FetchesAutocorrectPreference, AndReturnsCorrectVkPref) {
  const AutocorrectPrefCase& test_case = GetParam();
  SetVkAutocorrectLevelTo(profile_,
                          /*engine_id=*/kUsEnglish,
                          /*autocorrect_level=*/test_case.autocorrect_level);

  EXPECT_EQ(
      GetVirtualKeyboardAutocorrectPref(*(profile_.GetPrefs()), kUsEnglish),
      test_case.autocorrect_pref);
}

TEST_P(FetchesAutocorrectPreference, AndScopesReturnedVkPreferenceToEngineId) {
  const AutocorrectPrefCase& test_case = GetParam();
  SetVkAutocorrectLevelTo(profile_,
                          /*engine_id=*/kUsEnglish,
                          /*autocorrect_level=*/test_case.autocorrect_level);

  // The above preference is set for the US English engine only. Queries for
  // the autocorrect level on any other engine should return the default.
  EXPECT_EQ(GetVirtualKeyboardAutocorrectPref(*(profile_.GetPrefs()),
                                              kBrazilPortuguese),
            AutocorrectPreference::kDefault);
}

INSTANTIATE_TEST_SUITE_P(
    AutocorrectPrefsTest,
    FetchesAutocorrectPreference,
    testing::ValuesIn<AutocorrectPrefCase>({
        AutocorrectPrefCase{
            "ForAutocorrectLevelZero",
            /*autocorrect_level=*/0,
            /*autocorrect_pref=*/AutocorrectPreference::kDisabled},
        AutocorrectPrefCase{
            "ForAutocorrectLevelNegativeOne",
            /*autocorrect_level=*/-1,
            /*autocorrect_pref=*/AutocorrectPreference::kDisabled},
        AutocorrectPrefCase{
            "ForAutocorrectLevelOne",
            /*autocorrect_level=*/1,
            /*autocorrect_pref=*/AutocorrectPreference::kEnabled},
        AutocorrectPrefCase{
            "ForAutocorrectLevelTwo",
            /*autocorrect_level=*/1,
            /*autocorrect_pref=*/AutocorrectPreference::kEnabled},
    }),
    [](const testing::TestParamInfo<AutocorrectPrefCase> info) {
      return info.param.test_name;
    });

TEST_F(AutocorrectPrefsTest, MarksUsersPrefAsEnabledByDefault) {
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault}, {});

  SetPhysicalKeyboardAutocorrectAsEnabledByDefault(profile_.GetPrefs(),
                                                   kUsEnglish);

  EXPECT_EQ(
      GetPhysicalKeyboardAutocorrectPref(*(profile_.GetPrefs()), kUsEnglish),
      AutocorrectPreference::kEnabledByDefault);
}

TEST_F(AutocorrectPrefsTest, EnabledByDefaultIsScopedToSingleLanguage) {
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault}, {});

  SetPhysicalKeyboardAutocorrectAsEnabledByDefault(profile_.GetPrefs(),
                                                   kBrazilPortuguese);

  EXPECT_EQ(
      GetPhysicalKeyboardAutocorrectPref(*(profile_.GetPrefs()), kUsEnglish),
      AutocorrectPreference::kDefault);
  EXPECT_EQ(GetPhysicalKeyboardAutocorrectPref(*(profile_.GetPrefs()),
                                               kBrazilPortuguese),
            AutocorrectPreference::kEnabledByDefault);
}

class AutocorrectAdminPolicy : public AutocorrectPrefsTest,
                               public testing::WithParamInterface<std::string> {
};

TEST_P(AutocorrectAdminPolicy,
       WhenAdminPolicyDisallowsAutocorrectItIsAlwaysDisabled) {
  const std::string& engine_id = GetParam();

  SetPkAutocorrectLevelTo(profile_, engine_id, 1);
  SetManagedPkAutocorrectAllowed(profile_, false);

  EXPECT_EQ(
      GetPhysicalKeyboardAutocorrectPref(*(profile_.GetPrefs()), engine_id),
      AutocorrectPreference::kDisabled);
}

TEST_P(AutocorrectAdminPolicy, WhenAdminPolicyAllowsAutocorrectItCanBeEnabled) {
  const std::string& engine_id = GetParam();

  SetPkAutocorrectLevelTo(profile_, engine_id, 1);
  SetManagedPkAutocorrectAllowed(profile_, true);

  EXPECT_EQ(
      GetPhysicalKeyboardAutocorrectPref(*(profile_.GetPrefs()), engine_id),
      AutocorrectPreference::kEnabled);
}

TEST_P(AutocorrectAdminPolicy, WhenAdminPolicyIsNotSetAutocorrectCanBeEnabled) {
  const std::string& engine_id = GetParam();

  SetPkAutocorrectLevelTo(profile_, engine_id, 1);

  EXPECT_EQ(
      GetPhysicalKeyboardAutocorrectPref(*(profile_.GetPrefs()), engine_id),
      AutocorrectPreference::kEnabled);
}

INSTANTIATE_TEST_SUITE_P(AutocorrectPrefsTest,
                         AutocorrectAdminPolicy,
                         testing::ValuesIn<std::string>({
                             kBrazilPortuguese,
                             kFranceFrench,
                             kLatinAmericaSpanish,
                             kUsEnglish,
                         }));

}  // namespace
}  // namespace ash::input_method
