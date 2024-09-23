// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/pref_change_recorder.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/autocorrect_enums.h"
#include "chrome/browser/ash/input_method/autocorrect_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

constexpr char kUsEnglish[] = "xkb:us::eng";
constexpr char kBrazilPortuguese[] = "xkb:br::por";
constexpr char kSpainSpanish[] = "xkb:es::spa";
constexpr char kFranceFrench[] = "xkb:fr::fra";

class FakeInputMethodOptions {
 public:
  FakeInputMethodOptions(PrefService* pref_service,
                         const std::string& engine_id)
      : pref_service_(pref_service), engine_id_(engine_id) {}

  void SetPkAutocorrectLevel(int autocorrect_level) {
    ScopedDictPrefUpdate(pref_service_,
                         prefs::kLanguageInputMethodSpecificSettings)
        ->SetByDottedPath(engine_id_ + ".physicalKeyboardAutoCorrectionLevel",
                          base::Value(autocorrect_level));
  }

  void SetVkAutocorrectLevel(int autocorrect_level) {
    ScopedDictPrefUpdate(pref_service_,
                         prefs::kLanguageInputMethodSpecificSettings)
        ->SetByDottedPath(engine_id_ + ".virtualKeyboardAutoCorrectionLevel",
                          base::Value(autocorrect_level));
  }

 private:
  raw_ptr<PrefService> pref_service_;
  const std::string engine_id_;
};

class PrefChangeRecorderTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
};

struct AutocorrectPrefChangeCase {
  std::string test_name;
  std::optional<int> autocorrect_level_from;
  int autocorrect_level_to;
  AutocorrectPrefStateTransition expected_metric;
};

class UserChangesAutocorrectPrefMetric
    : public PrefChangeRecorderTest,
      public testing::WithParamInterface<AutocorrectPrefChangeCase> {};

TEST_P(UserChangesAutocorrectPrefMetric,
       RecordsTheCorrectValueForPkAndEnglish) {
  const AutocorrectPrefChangeCase& test_case = GetParam();
  base::HistogramTester histograms_;
  FakeInputMethodOptions options(profile_.GetPrefs(), kUsEnglish);

  // Set the initial autocorrect level (simulating previous values set by user).
  if (test_case.autocorrect_level_from) {
    options.SetPkAutocorrectLevel(test_case.autocorrect_level_from.value());
  }
  // Start observing for changes.
  PrefChangeRecorder recorder(profile_.GetPrefs());
  options.SetPkAutocorrectLevel(test_case.autocorrect_level_to);

  // Remember that English is a subset of All, so we must record both a metric
  // for English and All.
  histograms_.ExpectTotalCount(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.English.PK", 1);
  histograms_.ExpectUniqueSample(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.English.PK",
      /*sample=*/test_case.expected_metric, /*expected_bucket_count=*/1);
  histograms_.ExpectTotalCount(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.PK", 1);
  histograms_.ExpectUniqueSample(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.PK",
      /*sample=*/test_case.expected_metric, /*expected_bucket_count=*/1);
}

TEST_P(UserChangesAutocorrectPrefMetric,
       RecordsTheCorrectValueForVkAndEnglish) {
  const AutocorrectPrefChangeCase& test_case = GetParam();
  base::HistogramTester histograms_;
  FakeInputMethodOptions options(profile_.GetPrefs(), kUsEnglish);

  // Set the initial autocorrect level (simulating previous values set by user).
  if (test_case.autocorrect_level_from) {
    options.SetVkAutocorrectLevel(test_case.autocorrect_level_from.value());
  }
  // Start observing for changes.
  PrefChangeRecorder recorder(profile_.GetPrefs());
  options.SetVkAutocorrectLevel(test_case.autocorrect_level_to);

  // Remember that English is a subset of All, so we must record both a metric
  // for English and All.
  histograms_.ExpectTotalCount(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.English.VK", 1);
  histograms_.ExpectUniqueSample(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.English.VK",
      /*sample=*/test_case.expected_metric, /*expected_bucket_count=*/1);
  histograms_.ExpectTotalCount(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.VK", 1);
  histograms_.ExpectUniqueSample(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.VK",
      /*sample=*/test_case.expected_metric, /*expected_bucket_count=*/1);
}

TEST_P(UserChangesAutocorrectPrefMetric,
       RecordsTheCorrectValueForPkAndBucketsToLangOtherThenEnglish) {
  const AutocorrectPrefChangeCase& test_case = GetParam();
  base::HistogramTester histograms_;
  FakeInputMethodOptions options(profile_.GetPrefs(), kBrazilPortuguese);

  // Set the initial autocorrect level (simulating previous values set by user).
  if (test_case.autocorrect_level_from) {
    options.SetPkAutocorrectLevel(test_case.autocorrect_level_from.value());
  }
  // Start observing for changes.
  PrefChangeRecorder recorder(profile_.GetPrefs());
  options.SetPkAutocorrectLevel(test_case.autocorrect_level_to);

  histograms_.ExpectTotalCount(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.PK", 1);
  histograms_.ExpectUniqueSample(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.PK",
      /*sample=*/test_case.expected_metric, /*expected_bucket_count=*/1);
}

TEST_P(UserChangesAutocorrectPrefMetric,
       RecordsTheCorrectValueForVkAndBucketsToLangOtherThenEnglish) {
  const AutocorrectPrefChangeCase& test_case = GetParam();
  base::HistogramTester histograms_;
  FakeInputMethodOptions options(profile_.GetPrefs(), kBrazilPortuguese);

  // Set the initial autocorrect level (simulating previous values set by user).
  if (test_case.autocorrect_level_from) {
    options.SetVkAutocorrectLevel(test_case.autocorrect_level_from.value());
  }
  // Start observing for changes.
  PrefChangeRecorder recorder(profile_.GetPrefs());
  options.SetVkAutocorrectLevel(test_case.autocorrect_level_to);

  histograms_.ExpectTotalCount(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.VK", 1);
  histograms_.ExpectUniqueSample(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.VK",
      /*sample=*/test_case.expected_metric, /*expected_bucket_count=*/1);
}

TEST_P(UserChangesAutocorrectPrefMetric,
       DoesNotRecordChangeForPKIfValueDoesntChange) {
  const AutocorrectPrefChangeCase& test_case = GetParam();
  base::HistogramTester histograms_;
  FakeInputMethodOptions options(profile_.GetPrefs(), kUsEnglish);

  // Set the initial autocorrect level (simulating previous values set by user).
  if (test_case.autocorrect_level_from) {
    options.SetPkAutocorrectLevel(test_case.autocorrect_level_from.value());
  }
  // Start observing for changes.
  PrefChangeRecorder recorder(profile_.GetPrefs());
  options.SetPkAutocorrectLevel(test_case.autocorrect_level_to);
  options.SetPkAutocorrectLevel(test_case.autocorrect_level_to);
  options.SetPkAutocorrectLevel(test_case.autocorrect_level_to);

  // Records the first change only.
  histograms_.ExpectTotalCount(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.English.PK", 1);
}

TEST_P(UserChangesAutocorrectPrefMetric,
       DoesNotRecordChangeForVKIfValueDoesntChange) {
  const AutocorrectPrefChangeCase& test_case = GetParam();
  base::HistogramTester histograms_;
  FakeInputMethodOptions options(profile_.GetPrefs(), kUsEnglish);

  // Set the initial autocorrect level (simulating previous values set by user).
  if (test_case.autocorrect_level_from) {
    options.SetVkAutocorrectLevel(test_case.autocorrect_level_from.value());
  }
  // Start observing for changes.
  PrefChangeRecorder recorder(profile_.GetPrefs());
  options.SetVkAutocorrectLevel(test_case.autocorrect_level_to);
  options.SetVkAutocorrectLevel(test_case.autocorrect_level_to);
  options.SetVkAutocorrectLevel(test_case.autocorrect_level_to);

  // Records the first change only.
  histograms_.ExpectTotalCount(
      "InputMethod.Assistive.AutocorrectV2.UserPrefChange.English.VK", 1);
}

INSTANTIATE_TEST_SUITE_P(
    PrefChangeRecorderTest,
    UserChangesAutocorrectPrefMetric,
    testing::ValuesIn<AutocorrectPrefChangeCase>({
        AutocorrectPrefChangeCase{
            "DefaultToEnabled",
            /*autocorrect_level_from=*/std::nullopt,
            /*autocorrect_level_to=*/1,
            /*expected_change=*/
            AutocorrectPrefStateTransition::kDefaultToEnabled},
        AutocorrectPrefChangeCase{
            "DefaultToAggressive",
            /*autocorrect_level_from=*/std::nullopt,
            /*autocorrect_level_to=*/2,
            /*expected_change=*/
            AutocorrectPrefStateTransition::kDefaultToEnabled},
        AutocorrectPrefChangeCase{
            "EnabledToDisabled",
            /*autocorrect_level_from=*/1,
            /*autocorrect_level_to=*/0,
            /*expected_change=*/
            AutocorrectPrefStateTransition::kEnabledToDisabled},
        AutocorrectPrefChangeCase{
            "AggressiveToDisabled",
            /*autocorrect_level_from=*/2,
            /*autocorrect_level_to=*/0,
            /*expected_change=*/
            AutocorrectPrefStateTransition::kEnabledToDisabled},
        AutocorrectPrefChangeCase{
            "DisabledToEnabled",
            /*autocorrect_level_from=*/0,
            /*autocorrect_level_to=*/1,
            /*expected_change=*/
            AutocorrectPrefStateTransition::kDisabledToEnabled},
        AutocorrectPrefChangeCase{
            "DisabledToAggressive",
            /*autocorrect_level_from=*/0,
            /*autocorrect_level_to=*/2,
            /*expected_change=*/
            AutocorrectPrefStateTransition::kDisabledToEnabled},
    }),
    [](const testing::TestParamInfo<AutocorrectPrefChangeCase>& info) {
      return info.param.test_name;
    });

struct EnabledByDefaultMetricCase {
  std::string test_variant;
  std::string engine_id;
  std::string metric_name;
};

class RecordsEnabledByDefaultTransitions
    : public PrefChangeRecorderTest,
      public testing::WithParamInterface<EnabledByDefaultMetricCase> {};

TEST_P(RecordsEnabledByDefaultTransitions, RecordsNothingIfTheFlagIsDisabled) {
  const EnabledByDefaultMetricCase& test_case = GetParam();
  base::HistogramTester histograms_;
  feature_list_.InitWithFeatures({}, {features::kAutocorrectByDefault});

  // Start observing changes ...
  PrefChangeRecorder recorder(profile_.GetPrefs());
  // Simulate the user being marked as active in the enabled by default group.
  SetPhysicalKeyboardAutocorrectAsEnabledByDefault(profile_.GetPrefs(),
                                                   test_case.engine_id);

  histograms_.ExpectTotalCount(test_case.metric_name, 0);
}

TEST_P(RecordsEnabledByDefaultTransitions, RecordsDefaultToEnabledByDefault) {
  const EnabledByDefaultMetricCase& test_case = GetParam();
  base::HistogramTester histograms_;
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault}, {});

  // Start observing changes ...
  PrefChangeRecorder recorder(profile_.GetPrefs());
  // Simulate the user being marked as active in the enabled by default group.
  SetPhysicalKeyboardAutocorrectAsEnabledByDefault(profile_.GetPrefs(),
                                                   test_case.engine_id);

  histograms_.ExpectTotalCount(test_case.metric_name, 1);
  histograms_.ExpectUniqueSample(
      test_case.metric_name,
      /*sample=*/AutocorrectPrefStateTransition::kDefaultToForceEnabled,
      /*expected_bucket_count=*/1);
}

TEST_P(RecordsEnabledByDefaultTransitions, RecordsEnabledByDefaultToDisabled) {
  const EnabledByDefaultMetricCase& test_case = GetParam();
  base::HistogramTester histograms_;
  FakeInputMethodOptions options(profile_.GetPrefs(), test_case.engine_id);
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault}, {});

  // User was previously marked as active in the enabled by default group.
  SetPhysicalKeyboardAutocorrectAsEnabledByDefault(profile_.GetPrefs(),
                                                   test_case.engine_id);
  // Start observing changes ...
  PrefChangeRecorder recorder(profile_.GetPrefs());
  options.SetPkAutocorrectLevel(0);  // set as disabled

  histograms_.ExpectTotalCount(test_case.metric_name, 1);
  histograms_.ExpectUniqueSample(
      test_case.metric_name,
      /*sample=*/AutocorrectPrefStateTransition::kForceEnabledToDisabled,
      /*expected_bucket_count=*/1);
}

TEST_P(RecordsEnabledByDefaultTransitions,
       RecordsEnabledByDefaultToEnabledWhenSetToModest) {
  const EnabledByDefaultMetricCase& test_case = GetParam();
  base::HistogramTester histograms_;
  FakeInputMethodOptions options(profile_.GetPrefs(), test_case.engine_id);
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault}, {});

  // User was previously marked as active in the enabled by default group.
  SetPhysicalKeyboardAutocorrectAsEnabledByDefault(profile_.GetPrefs(),
                                                   test_case.engine_id);
  // Start observing changes ...
  PrefChangeRecorder recorder(profile_.GetPrefs());
  options.SetPkAutocorrectLevel(1);  // set as enabled (modest)

  histograms_.ExpectTotalCount(test_case.metric_name, 1);
  histograms_.ExpectUniqueSample(
      test_case.metric_name,
      /*sample=*/AutocorrectPrefStateTransition::kForceEnabledToEnabled,
      /*expected_bucket_count=*/1);
}

TEST_P(RecordsEnabledByDefaultTransitions,
       RecordsEnabledByDefaultToEnabledWhenSetToAggressive) {
  const EnabledByDefaultMetricCase& test_case = GetParam();
  base::HistogramTester histograms_;
  FakeInputMethodOptions options(profile_.GetPrefs(), test_case.engine_id);
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault}, {});

  // User was previously marked as active in the enabled by default group.
  SetPhysicalKeyboardAutocorrectAsEnabledByDefault(profile_.GetPrefs(),
                                                   test_case.engine_id);
  // Start observing changes ...
  PrefChangeRecorder recorder(profile_.GetPrefs());
  options.SetPkAutocorrectLevel(2);  // set as enabled (aggressive)

  histograms_.ExpectTotalCount(test_case.metric_name, 1);
  histograms_.ExpectUniqueSample(
      test_case.metric_name,
      /*sample=*/AutocorrectPrefStateTransition::kForceEnabledToEnabled,
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    PrefChangeRecorderTest,
    RecordsEnabledByDefaultTransitions,
    testing::ValuesIn<EnabledByDefaultMetricCase>({
        EnabledByDefaultMetricCase{
            "UsEnglish",
            /*engine_id=*/kUsEnglish,
            /*metric_name=*/
            "InputMethod.Assistive.AutocorrectV2.UserPrefChange.English.PK"},
        EnabledByDefaultMetricCase{
            "BrazilianPortuguese",
            /*engine_id=*/kBrazilPortuguese,
            /*metric_name=*/
            "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.PK"},
        EnabledByDefaultMetricCase{
            "SpainSpanish",
            /*engine_id=*/kSpainSpanish,
            /*metric_name=*/
            "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.PK"},
        EnabledByDefaultMetricCase{
            "FranceFrench",
            /*engine_id=*/kFranceFrench,
            /*metric_name=*/
            "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.PK"},
    }),
    [](const testing::TestParamInfo<EnabledByDefaultMetricCase>& info) {
      return info.param.test_variant;
    });

}  // namespace
}  // namespace ash::input_method
