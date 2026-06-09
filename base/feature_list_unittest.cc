// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_buildflags.h"
#include "base/feature_list_internal.h"
#include "base/feature_visitor.h"
#include "base/format_macros.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using ::base::internal::RuntimeMutabilityResult;

constexpr char kFeatureOnByDefaultName[] = "OnByDefault";
BASE_FEATURE(kFeatureOnByDefault,
             kFeatureOnByDefaultName,
             FEATURE_ENABLED_BY_DEFAULT);

constexpr char kFeatureOffByDefaultName[] = "OffByDefault";
BASE_FEATURE(kFeatureOffByDefault,
             kFeatureOffByDefaultName,
             FEATURE_DISABLED_BY_DEFAULT);

// For testing the 2-argument BASE_FEATURE macro.
BASE_FEATURE(kFeature2ArgsOn, FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeature2ArgsOff, FEATURE_DISABLED_BY_DEFAULT);

// For testing the 4-argument BASE_FEATURE_PARAM macro.
BASE_FEATURE_PARAM(int, kParamShortFormInt, &kFeature2ArgsOn, 42);
// For testing the 5-argument BASE_FEATURE_PARAM macro (original form).
BASE_FEATURE_PARAM(bool,
                   kParamLongFormBool,
                   &kFeature2ArgsOn,
                   "CustomParamName",
                   true);

// For testing the 5-argument BASE_FEATURE_ENUM_PARAM macro.
enum class TestEnum { kFirst, kSecond };
constexpr FeatureParam<TestEnum>::Option kTestEnumOptions[] = {
    {TestEnum::kFirst, "first"},
    {TestEnum::kSecond, "second"}};
BASE_FEATURE_ENUM_PARAM(TestEnum,
                        kEnumParamShortForm,
                        &kFeature2ArgsOn,
                        TestEnum::kFirst,
                        &kTestEnumOptions);
// For testing the 6-argument BASE_FEATURE_ENUM_PARAM macro (original form).
BASE_FEATURE_ENUM_PARAM(TestEnum,
                        kEnumParamLongForm,
                        &kFeature2ArgsOn,
                        "CustomEnumParamName",
                        TestEnum::kSecond,
                        &kTestEnumOptions);

// Features for the HistogramLogging test.
BASE_FEATURE(kEarlyFeature, FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kLateFeature, FEATURE_DISABLED_BY_DEFAULT);

// Features for testing runtime mutable features.
BASE_RUNTIME_MUTABLE_FEATURE(kRuntimeMutableFeature3Args,
                             "RuntimeMutableFeature3Args",
                             FEATURE_ENABLED_BY_DEFAULT);

BASE_RUNTIME_MUTABLE_FEATURE(kRuntimeMutableFeature,
                             FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kRuntimeMutableFeatureParam,
                   &kRuntimeMutableFeature,
                   12345);

constexpr std::string_view kRuntimeMutabilityResult =
    "Variations.RuntimeMutability.Result";

constexpr std::string_view kRuntimeMutabilityErrorFeatureName =
    "Variations.RuntimeMutability.Error.FeatureName";

std::string SortFeatureListString(const std::string& feature_list) {
  std::vector<std::string_view> features =
      FeatureList::SplitFeatureListString(feature_list);
  std::ranges::sort(features);
  return JoinString(features, ",");
}

}  // namespace

// A feature outside the anonymous namespace.
BASE_FEATURE(kFeatureOutsideAnonymousNamespace, FEATURE_DISABLED_BY_DEFAULT);

class FeatureListTest : public testing::Test {
 public:
  FeatureListTest() {
    // Provide an empty FeatureList to each test by default.
    scoped_feature_list_.InitWithFeatureList(std::make_unique<FeatureList>());
    FeatureList::ClearFeatureCachedValueForTesting(kRuntimeMutableFeature);
    FeatureList::ClearFeatureCachedValueForTesting(kRuntimeMutableFeature3Args);
  }
  FeatureListTest(const FeatureListTest&) = delete;
  FeatureListTest& operator=(const FeatureListTest&) = delete;
  ~FeatureListTest() override = default;

  HistogramTester histogram_tester;

  // Verify that the `kRuntimeMutabilityMask` set in the `Feature` struct is the
  // same as the one used by the `FeatureList` helpers. `Feature` has it's own
  // copy to minimize dependencies and exposed implementation details (the
  // constexpr constructor needs the value).
  static_assert(static_cast<uint32_t>(Feature::kRuntimeMutabilityMask) ==
                static_cast<uint32_t>(internal::kRuntimeMutabilityMask));

  // If any of these static asserts fail, that means the layout of the
  // `base::Feature` struct has been modified, and thus the Rust equivalent in
  // `base/feature.rs` (the `base::Feature` struct) must be updated to match.
  // LINT.IfChange(FeatureStruct)
  static_assert(sizeof(Feature) == (sizeof(void*) == 8 ? 16 : 12));
  static_assert(alignof(Feature) == alignof(void*));
  static_assert(offsetof(Feature, name) == 0);
  static_assert(offsetof(Feature, default_state) == sizeof(void*));
  static_assert(offsetof(Feature, cached_value) == sizeof(void*) + 4);
  // LINT.ThenChange(feature.rs:FeatureStruct)

 private:
  test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FeatureListTest, DefaultStates) {
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
}

// Testing the 2-argument BASE_FEATURE macro.
TEST_F(FeatureListTest, TwoArgMacro) {
  EXPECT_TRUE(FeatureList::IsEnabled(kFeature2ArgsOn));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeature2ArgsOff));
  EXPECT_STREQ("Feature2ArgsOn", kFeature2ArgsOn.name);
  EXPECT_STREQ("Feature2ArgsOff", kFeature2ArgsOff.name);
}

// Testing the 4-argument BASE_FEATURE_PARAM macro (auto-derived name).
TEST_F(FeatureListTest, FourArgFeatureParamMacro) {
  EXPECT_STREQ("ParamShortFormInt", kParamShortFormInt.name);
  EXPECT_EQ(42, kParamShortFormInt.default_value);
}

// Testing the 5-argument BASE_FEATURE_PARAM macro (explicit name).
TEST_F(FeatureListTest, FiveArgFeatureParamMacro) {
  EXPECT_STREQ("CustomParamName", kParamLongFormBool.name);
  EXPECT_EQ(true, kParamLongFormBool.default_value);
}

// Testing the 5-argument BASE_FEATURE_ENUM_PARAM macro (auto-derived name).
TEST_F(FeatureListTest, FiveArgFeatureEnumParamMacro) {
  EXPECT_STREQ("EnumParamShortForm", kEnumParamShortForm.name);
  EXPECT_EQ(TestEnum::kFirst, kEnumParamShortForm.default_value);
}

// Testing the 6-argument BASE_FEATURE_ENUM_PARAM macro (explicit name).
TEST_F(FeatureListTest, SixArgFeatureEnumParamMacro) {
  EXPECT_STREQ("CustomEnumParamName", kEnumParamLongForm.name);
  EXPECT_EQ(TestEnum::kSecond, kEnumParamLongForm.default_value);
}

TEST_F(FeatureListTest, OutsideAnonymousNamespace) {
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOutsideAnonymousNamespace));
  EXPECT_STREQ("FeatureOutsideAnonymousNamespace",
               kFeatureOutsideAnonymousNamespace.name);
}

TEST_F(FeatureListTest, InitFromCommandLine) {
  struct TestCases {
    const char* enable_features;
    const char* disable_features;
    bool expected_feature_on_state;
    bool expected_feature_off_state;
  };
  auto test_cases = std::to_array<TestCases>({
      {"", "", true, false},
      {"OffByDefault", "", true, true},
      {"OffByDefault", "OnByDefault", false, true},
      {"OnByDefault,OffByDefault", "", true, true},
      {"", "OnByDefault,OffByDefault", false, false},
      // In the case an entry is both, disable takes precedence.
      {"OnByDefault", "OnByDefault,OffByDefault", false, false},
  });

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: [%s] [%s]", i,
                                    test_case.enable_features,
                                    test_case.disable_features));

    auto feature_list = std::make_unique<FeatureList>();
    feature_list->InitFromCommandLine(test_case.enable_features,
                                      test_case.disable_features);
    test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_EQ(test_case.expected_feature_on_state,
              FeatureList::IsEnabled(kFeatureOnByDefault))
        << i;
    EXPECT_EQ(test_case.expected_feature_off_state,
              FeatureList::IsEnabled(kFeatureOffByDefault))
        << i;

    // Reading the state of each feature again will pull it from their
    // respective caches instead of performing the full lookup, which should
    // yield the same result.
    EXPECT_EQ(test_case.expected_feature_on_state,
              FeatureList::IsEnabled(kFeatureOnByDefault))
        << i;
    EXPECT_EQ(test_case.expected_feature_off_state,
              FeatureList::IsEnabled(kFeatureOffByDefault))
        << i;
  }
}

TEST_F(FeatureListTest, InitFromCommandLineWithFeatureParams) {
  struct {
    const std::string enable_features;
    const std::string expected_field_trial_created;
    const std::map<std::string, std::string> expected_feature_params;
  } test_cases[] = {
      {"Feature:x/100/y/test", "StudyFeature", {{"x", "100"}, {"y", "test"}}},
      {"Feature<Trial1:x/200/y/123", "Trial1", {{"x", "200"}, {"y", "123"}}},
      {"Feature<Trial2.Group2:x/test/y/uma/z/ukm",
       "Trial2",
       {{"x", "test"}, {"y", "uma"}, {"z", "ukm"}}},
  };

  // Clear global state so that repeated runs of this test don't flake.
  // When https://crrev.com/c/3694674 is submitted, we should be able to remove
  // this.
  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();

  static BASE_FEATURE(kFeature, "Feature", FEATURE_DISABLED_BY_DEFAULT);
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.enable_features);

    auto feature_list = std::make_unique<FeatureList>();
    feature_list->InitFromCommandLine(test_case.enable_features, "");
    test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_TRUE(FeatureList::IsEnabled(kFeature));
    EXPECT_TRUE(
        FieldTrialList::IsTrialActive(test_case.expected_field_trial_created));
    std::map<std::string, std::string> actual_params;
    EXPECT_TRUE(GetFieldTrialParamsByFeature(kFeature, &actual_params));
    EXPECT_EQ(test_case.expected_feature_params, actual_params);
  }
}

TEST_F(FeatureListTest, CheckFeatureIdentity) {
  // Tests that CheckFeatureIdentity() correctly detects when two different
  // structs with the same feature name are passed to it.

  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::make_unique<FeatureList>());
  FeatureList* feature_list = FeatureList::GetInstance();

  // Call it twice for each feature at the top of the file, since the first call
  // makes it remember the entry and the second call will verify it.
  EXPECT_TRUE(feature_list->CheckFeatureIdentity(kFeatureOnByDefault));
  EXPECT_TRUE(feature_list->CheckFeatureIdentity(kFeatureOnByDefault));
  EXPECT_TRUE(feature_list->CheckFeatureIdentity(kFeatureOffByDefault));
  EXPECT_TRUE(feature_list->CheckFeatureIdentity(kFeatureOffByDefault));

  // Now, call it with a distinct struct for |kFeatureOnByDefaultName|, which
  // should return false.
  static BASE_FEATURE(kFeatureOnByDefault2, kFeatureOnByDefaultName,
                      FEATURE_ENABLED_BY_DEFAULT);
  EXPECT_FALSE(feature_list->CheckFeatureIdentity(kFeatureOnByDefault2));
}

TEST_F(FeatureListTest, FieldTrialOverrides) {
  struct TestCases {
    FeatureList::OverrideState trial1_state;
    FeatureList::OverrideState trial2_state;
  };
  auto test_cases = std::to_array<TestCases>({
      {FeatureList::OVERRIDE_DISABLE_FEATURE,
       FeatureList::OVERRIDE_DISABLE_FEATURE},
      {FeatureList::OVERRIDE_DISABLE_FEATURE,
       FeatureList::OVERRIDE_ENABLE_FEATURE},
      {FeatureList::OVERRIDE_ENABLE_FEATURE,
       FeatureList::OVERRIDE_DISABLE_FEATURE},
      {FeatureList::OVERRIDE_ENABLE_FEATURE,
       FeatureList::OVERRIDE_ENABLE_FEATURE},
  });

  FieldTrial::ActiveGroup active_group;
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]", i));

    test::ScopedFeatureList outer_scope;
    outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

    auto feature_list = std::make_unique<FeatureList>();

    FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("TrialExample1", "A");
    FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("TrialExample2", "B");
    feature_list->RegisterFieldTrialOverride(kFeatureOnByDefaultName,
                                             test_case.trial1_state, trial1);
    feature_list->RegisterFieldTrialOverride(kFeatureOffByDefaultName,
                                             test_case.trial2_state, trial2);
    test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    // Initially, neither trial should be active.
    EXPECT_FALSE(FieldTrialList::IsTrialActive(trial1->trial_name()));
    EXPECT_FALSE(FieldTrialList::IsTrialActive(trial2->trial_name()));

    const bool expected_enabled_1 =
        (test_case.trial1_state == FeatureList::OVERRIDE_ENABLE_FEATURE);
    EXPECT_EQ(expected_enabled_1, FeatureList::IsEnabled(kFeatureOnByDefault));
    // The above should have activated |trial1|.
    EXPECT_TRUE(FieldTrialList::IsTrialActive(trial1->trial_name()));
    EXPECT_FALSE(FieldTrialList::IsTrialActive(trial2->trial_name()));

    const bool expected_enabled_2 =
        (test_case.trial2_state == FeatureList::OVERRIDE_ENABLE_FEATURE);
    EXPECT_EQ(expected_enabled_2, FeatureList::IsEnabled(kFeatureOffByDefault));
    // The above should have activated |trial2|.
    EXPECT_TRUE(FieldTrialList::IsTrialActive(trial1->trial_name()));
    EXPECT_TRUE(FieldTrialList::IsTrialActive(trial2->trial_name()));
  }
}

TEST_F(FeatureListTest, FieldTrialAssociateUseDefault) {
  auto feature_list = std::make_unique<FeatureList>();

  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("TrialExample1", "A");
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("TrialExample2", "B");
  feature_list->RegisterFieldTrialOverride(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial1);
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial2);
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // Initially, neither trial should be active.
  EXPECT_FALSE(FieldTrialList::IsTrialActive(trial1->trial_name()));
  EXPECT_FALSE(FieldTrialList::IsTrialActive(trial2->trial_name()));

  // Check the feature enabled state is its default.
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  // The above should have activated |trial1|.
  EXPECT_TRUE(FieldTrialList::IsTrialActive(trial1->trial_name()));
  EXPECT_FALSE(FieldTrialList::IsTrialActive(trial2->trial_name()));

  // Check the feature enabled state is its default.
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
  // The above should have activated |trial2|.
  EXPECT_TRUE(FieldTrialList::IsTrialActive(trial1->trial_name()));
  EXPECT_TRUE(FieldTrialList::IsTrialActive(trial2->trial_name()));
}

TEST_F(FeatureListTest, CommandLineEnableTakesPrecedenceOverFieldTrial) {
  auto feature_list = std::make_unique<FeatureList>();

  // The feature is explicitly enabled on the command-line.
  feature_list->InitFromCommandLine(kFeatureOffByDefaultName, "");

  // But the FieldTrial would set the feature to disabled.
  FieldTrial* trial = FieldTrialList::CreateFieldTrial("TrialExample2", "A");
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE, trial);
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_FALSE(FieldTrialList::IsTrialActive(trial->trial_name()));
  // Command-line should take precedence.
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOffByDefault));
  // Since the feature is on due to the command-line, and not as a result of the
  // field trial, the field trial should not be activated (since the Associate*
  // API wasn't used.)
  EXPECT_FALSE(FieldTrialList::IsTrialActive(trial->trial_name()));
}

TEST_F(FeatureListTest, CommandLineDisableTakesPrecedenceOverFieldTrial) {
  auto feature_list = std::make_unique<FeatureList>();

  // The feature is explicitly disabled on the command-line.
  feature_list->InitFromCommandLine("", kFeatureOffByDefaultName);

  // But the FieldTrial would set the feature to enabled.
  FieldTrial* trial = FieldTrialList::CreateFieldTrial("TrialExample2", "A");
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_FALSE(FieldTrialList::IsTrialActive(trial->trial_name()));
  // Command-line should take precedence.
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
  // Since the feature is on due to the command-line, and not as a result of the
  // field trial, the field trial should not be activated (since the Associate*
  // API wasn't used.)
  EXPECT_FALSE(FieldTrialList::IsTrialActive(trial->trial_name()));
}

TEST_F(FeatureListTest, IsFeatureOverriddenFromFieldTrial) {
  auto feature_list = std::make_unique<FeatureList>();

  // No features are overridden from the field trails yet.
  EXPECT_FALSE(feature_list->IsFeatureOverridden(kFeatureOnByDefaultName));
  EXPECT_FALSE(feature_list->IsFeatureOverridden(kFeatureOffByDefaultName));

  // Now, register field trials to override `kFeatureOnByDefaultName` state and
  // keeping `kFeatureOffByDefault` as the default. Check that both are
  // considered overridden.
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT,
      FieldTrialList::CreateFieldTrial("Trial1", "A"));
  feature_list->RegisterFieldTrialOverride(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE,
      FieldTrialList::CreateFieldTrial("Trial2", "A"));
  EXPECT_TRUE(feature_list->IsFeatureOverridden(kFeatureOnByDefaultName));
  EXPECT_TRUE(feature_list->IsFeatureOverridden(kFeatureOffByDefaultName));

  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));
  // Check the expected feature states for good measure.
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOnByDefault));
}

TEST_F(FeatureListTest, IsFeatureOverriddenFromCommandLine) {
  auto feature_list = std::make_unique<FeatureList>();

  // No features are overridden from the command line yet
  EXPECT_FALSE(feature_list->IsFeatureOverridden(kFeatureOnByDefaultName));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName));
  EXPECT_FALSE(feature_list->IsFeatureOverridden(kFeatureOffByDefaultName));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));

  // Now, enable |kFeatureOffByDefaultName| via the command-line.
  feature_list->InitFromCommandLine(kFeatureOffByDefaultName, "");

  // It should now be overridden for the enabled group.
  EXPECT_TRUE(feature_list->IsFeatureOverridden(kFeatureOffByDefaultName));
  EXPECT_TRUE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));
  EXPECT_TRUE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));

  // Register a field trial to associate with the feature and ensure that the
  // results are still the same.
  feature_list->AssociateReportingFieldTrial(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE,
      FieldTrialList::CreateFieldTrial("Trial1", "A"));
  EXPECT_TRUE(feature_list->IsFeatureOverridden(kFeatureOffByDefaultName));
  EXPECT_TRUE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));
  EXPECT_TRUE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));

  // Now, register a field trial to override |kFeatureOnByDefaultName| state
  // and check that the function still returns false for that feature.
  feature_list->RegisterFieldTrialOverride(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE,
      FieldTrialList::CreateFieldTrial("Trial2", "A"));
  EXPECT_TRUE(feature_list->IsFeatureOverridden(kFeatureOnByDefaultName));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // Check the expected feature states for good measure.
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOnByDefault));
}

TEST_F(FeatureListTest, AssociateReportingFieldTrial) {
  struct TestCases {
    const char* enable_features;
    const char* disable_features;
    bool expected_enable_trial_created;
    bool expected_disable_trial_created;
  };
  auto test_cases = std::to_array<TestCases>({
      // If no enable/disable flags are specified, no trials should be created.
      {"", "", false, false},
      // Enabling the feature should result in the enable trial created.
      {kFeatureOffByDefaultName, "", true, false},
      // Disabling the feature should result in the disable trial created.
      {"", kFeatureOffByDefaultName, false, true},
  });

  const char kTrialName[] = "ForcingTrial";
  const char kForcedOnGroupName[] = "ForcedOn";
  const char kForcedOffGroupName[] = "ForcedOff";

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: [%s] [%s]", i,
                                    test_case.enable_features,
                                    test_case.disable_features));

    test::ScopedFeatureList outer_scope;
    outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

    auto feature_list = std::make_unique<FeatureList>();
    feature_list->InitFromCommandLine(test_case.enable_features,
                                      test_case.disable_features);

    FieldTrial* enable_trial = nullptr;
    if (feature_list->IsFeatureOverriddenFromCommandLine(
            kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE)) {
      enable_trial = base::FieldTrialList::CreateFieldTrial(kTrialName,
                                                            kForcedOnGroupName);
      feature_list->AssociateReportingFieldTrial(
          kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE,
          enable_trial);
    }
    FieldTrial* disable_trial = nullptr;
    if (feature_list->IsFeatureOverriddenFromCommandLine(
            kFeatureOffByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE)) {
      disable_trial = base::FieldTrialList::CreateFieldTrial(
          kTrialName, kForcedOffGroupName);
      feature_list->AssociateReportingFieldTrial(
          kFeatureOffByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE,
          disable_trial);
    }
    EXPECT_EQ(test_case.expected_enable_trial_created, enable_trial != nullptr);
    EXPECT_EQ(test_case.expected_disable_trial_created,
              disable_trial != nullptr);
    test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_FALSE(FieldTrialList::IsTrialActive(kTrialName));
    if (disable_trial) {
      EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
      EXPECT_TRUE(FieldTrialList::IsTrialActive(kTrialName));
      EXPECT_EQ(kForcedOffGroupName, disable_trial->group_name());
    } else if (enable_trial) {
      EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOffByDefault));
      EXPECT_TRUE(FieldTrialList::IsTrialActive(kTrialName));
      EXPECT_EQ(kForcedOnGroupName, enable_trial->group_name());
    }
  }
}

TEST_F(FeatureListTest, RegisterExtraFeatureOverrides_ReplaceUseDefault) {
  auto feature_list = std::make_unique<FeatureList>();

  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("Trial1", "Group");
  feature_list->RegisterFieldTrialOverride(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial1);

  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("Trial2", "Group");
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial2);

  std::vector<FeatureList::FeatureOverrideInfo> overrides;
  overrides.emplace_back(std::cref(kFeatureOnByDefault),
                         FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE);
  overrides.emplace_back(std::cref(kFeatureOffByDefault),
                         FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE);
  feature_list->RegisterExtraFeatureOverrides(
      std::move(overrides), /*replace_use_default_overrides=*/true);
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // OVERRIDE_USE_DEFAULT entries should be overridden by AwFeatureOverrides.
  // Before querying the feature, the trials shouldn't be active, but should
  // be activated after querying.

  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("Trial1"));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("Trial1"));

  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("Trial2"));
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("Trial2"));
}

TEST_F(FeatureListTest, RegisterExtraFeatureOverrides) {
  auto feature_list = std::make_unique<FeatureList>();
  std::vector<FeatureList::FeatureOverrideInfo> overrides;
  overrides.emplace_back(std::cref(kFeatureOnByDefault),
                         FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE);
  overrides.emplace_back(std::cref(kFeatureOffByDefault),
                         FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE);
  feature_list->RegisterExtraFeatureOverrides(std::move(overrides));
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOffByDefault));
}

TEST_F(FeatureListTest, InitFromCommandLineThenRegisterExtraOverrides) {
  auto feature_list = std::make_unique<FeatureList>();
  feature_list->InitFromCommandLine(kFeatureOnByDefaultName,
                                    kFeatureOffByDefaultName);
  std::vector<FeatureList::FeatureOverrideInfo> overrides;
  overrides.emplace_back(std::cref(kFeatureOnByDefault),
                         FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE);
  overrides.emplace_back(std::cref(kFeatureOffByDefault),
                         FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE);
  feature_list->RegisterExtraFeatureOverrides(std::move(overrides));
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // The InitFromCommandLine supersedes the RegisterExtraFeatureOverrides
  // because it was called first.
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));

  std::string enable_features;
  std::string disable_features;
  FeatureList::GetInstance()->GetFeatureOverrides(&enable_features,
                                                  &disable_features);
  EXPECT_EQ(kFeatureOnByDefaultName, SortFeatureListString(enable_features));
  EXPECT_EQ(kFeatureOffByDefaultName, SortFeatureListString(disable_features));
}

TEST_F(FeatureListTest, GetFeatureOverrides) {
  auto feature_list = std::make_unique<FeatureList>();
  feature_list->InitFromCommandLine("A,X", "D");

  static BASE_FEATURE(feature_b, "B", FEATURE_ENABLED_BY_DEFAULT);
  static BASE_FEATURE(feature_c, "C", FEATURE_DISABLED_BY_DEFAULT);
  std::vector<FeatureList::FeatureOverrideInfo> overrides;
  overrides.emplace_back(std::cref(feature_b),
                         FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE);
  overrides.emplace_back(std::cref(feature_c),
                         FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE);
  feature_list->RegisterExtraFeatureOverrides(std::move(overrides));

  FieldTrial* trial = FieldTrialList::CreateFieldTrial("Trial", "Group");
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE, trial);

  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  std::string enable_features;
  std::string disable_features;
  FeatureList::GetInstance()->GetFeatureOverrides(&enable_features,
                                                  &disable_features);
  EXPECT_EQ("A,C,OffByDefault<Trial,X", SortFeatureListString(enable_features));
  EXPECT_EQ("B,D", SortFeatureListString(disable_features));

  FeatureList::GetInstance()->GetCommandLineFeatureOverrides(&enable_features,
                                                             &disable_features);
  EXPECT_EQ("A,C,X", SortFeatureListString(enable_features));
  EXPECT_EQ("B,D", SortFeatureListString(disable_features));
}

TEST_F(FeatureListTest, GetFeatureOverrides_UseDefault) {
  auto feature_list = std::make_unique<FeatureList>();
  feature_list->InitFromCommandLine("A,X", "D");

  FieldTrial* trial = FieldTrialList::CreateFieldTrial("Trial", "Group");
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial);

  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  std::string enable_features;
  std::string disable_features;
  FeatureList::GetInstance()->GetFeatureOverrides(&enable_features,
                                                  &disable_features);
  EXPECT_EQ("*OffByDefault<Trial,A,X", SortFeatureListString(enable_features));
  EXPECT_EQ("D", SortFeatureListString(disable_features));
}

TEST_F(FeatureListTest, GetFieldTrial) {
  FieldTrial* trial = FieldTrialList::CreateFieldTrial("Trial", "Group");
  auto feature_list = std::make_unique<FeatureList>();
  feature_list->RegisterFieldTrialOverride(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial);
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_EQ(trial, FeatureList::GetFieldTrial(kFeatureOnByDefault));
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kFeatureOffByDefault));
}

TEST_F(FeatureListTest, InitFromCommandLine_WithFieldTrials) {
  FieldTrialList::CreateFieldTrial("Trial", "Group");
  auto feature_list = std::make_unique<FeatureList>();
  feature_list->InitFromCommandLine("A,OffByDefault<Trial,X", "D");
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_FALSE(FieldTrialList::IsTrialActive("Trial"));
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_TRUE(FieldTrialList::IsTrialActive("Trial"));
}

TEST_F(FeatureListTest, InitFromCommandLine_UseDefault) {
  FieldTrialList::CreateFieldTrial("T1", "Group");
  FieldTrialList::CreateFieldTrial("T2", "Group");
  auto feature_list = std::make_unique<FeatureList>();
  feature_list->InitFromCommandLine("A,*OffByDefault<T1,*OnByDefault<T2,X",
                                    "D");
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_FALSE(FieldTrialList::IsTrialActive("T1"));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_TRUE(FieldTrialList::IsTrialActive("T1"));

  EXPECT_FALSE(FieldTrialList::IsTrialActive("T2"));
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_TRUE(FieldTrialList::IsTrialActive("T2"));
}

TEST_F(FeatureListTest, InitInstance) {
  auto feature_list = std::make_unique<base::FeatureList>();
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));

  // Initialize from command line if we haven't yet.
  FeatureList::InitInstance("", kFeatureOnByDefaultName);
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));

  // Do not initialize from commandline if we have already.
  FeatureList::InitInstance(kFeatureOffByDefaultName, "");
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
}

TEST_F(FeatureListTest, UninitializedInstance_IsEnabledReturnsFalse) {
  std::unique_ptr<FeatureList> original_feature_list =
      FeatureList::ClearInstanceForTesting();

  // This test case simulates the calling pattern found in code which does not
  // explicitly initialize the features list.
  // All IsEnabled() calls should return the default value in this scenario.
  EXPECT_EQ(nullptr, FeatureList::GetInstance());
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_EQ(nullptr, FeatureList::GetInstance());
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));

  if (original_feature_list) {
    FeatureList::RestoreInstanceForTesting(std::move(original_feature_list));
  }
}

TEST_F(FeatureListTest, StoreAndRetrieveFeaturesFromSharedMemory) {
  auto feature_list = std::make_unique<base::FeatureList>();

  // Create some overrides.
  feature_list->RegisterOverride(kFeatureOffByDefaultName,
                                 FeatureList::OVERRIDE_ENABLE_FEATURE, nullptr);
  feature_list->RegisterOverride(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE, nullptr);
  feature_list->FinalizeInitialization();

  // Create an allocator and store the overrides.
  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(4 << 10);
  WritableSharedPersistentMemoryAllocator allocator(std::move(shm.mapping), 1,
                                                    "");
  feature_list->AddFeaturesToAllocator(&allocator);

  auto feature_list2 = std::make_unique<base::FeatureList>();

  // Check that the new feature list is empty.
  EXPECT_FALSE(feature_list2->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));
  EXPECT_FALSE(feature_list2->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));

  feature_list2->InitFromSharedMemory(&allocator);
  // Check that the new feature list now has 2 overrides.
  EXPECT_TRUE(feature_list2->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));
  EXPECT_TRUE(feature_list2->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));
}

TEST_F(FeatureListTest, StoreAndRetrieveAssociatedFeaturesFromSharedMemory) {
  auto feature_list = std::make_unique<base::FeatureList>();

  // Create some overrides.
  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("TrialExample1", "A");
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("TrialExample2", "B");
  feature_list->RegisterFieldTrialOverride(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial1);
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial2);
  feature_list->FinalizeInitialization();

  // Create an allocator and store the overrides.
  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(4 << 10);
  WritableSharedPersistentMemoryAllocator allocator(std::move(shm.mapping), 1,
                                                    "");
  feature_list->AddFeaturesToAllocator(&allocator);

  auto feature_list2 = std::make_unique<base::FeatureList>();
  feature_list2->InitFromSharedMemory(&allocator);
  feature_list2->FinalizeInitialization();

  // Check that the field trials are still associated.
  FieldTrial* associated_trial1 =
      feature_list2->GetAssociatedFieldTrial(kFeatureOnByDefault);
  FieldTrial* associated_trial2 =
      feature_list2->GetAssociatedFieldTrial(kFeatureOffByDefault);
  EXPECT_EQ(associated_trial1, trial1);
  EXPECT_EQ(associated_trial2, trial2);
}

TEST_F(FeatureListTest, SetEarlyAccessInstance_AllowList) {
  test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  auto early_access_feature_list = std::make_unique<FeatureList>();
  early_access_feature_list->InitFromCommandLine("OffByDefault", "OnByDefault");
  FeatureList::SetEarlyAccessInstance(std::move(early_access_feature_list),
                                      {"DcheckIsFatal", "OnByDefault"});
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_EQ(&kFeatureOffByDefault,
            FeatureList::GetEarlyAccessedFeatureForTesting());
  FeatureList::ResetEarlyFeatureAccessTrackerForTesting();
}

TEST_F(FeatureListTest, SetEarlyAccessInstance_ReplaceByRealList) {
  test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  auto early_access_feature_list = std::make_unique<FeatureList>();
  early_access_feature_list->InitFromCommandLine("OffByDefault", "OnByDefault");
  FeatureList::SetEarlyAccessInstance(
      std::move(early_access_feature_list),
      {"DcheckIsFatal", "OffByDefault", "OnByDefault"});
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOffByDefault));

  auto feature_list = std::make_unique<FeatureList>();
  feature_list->InitFromCommandLine("", "");
  FeatureList::SetInstance(std::move(feature_list));
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
}

TEST_F(FeatureListTest, ParseFeatureString_WithIllegalFeatures) {
  // Normal feature format: Feature<Trial.Group:param=value.
  // Leading or trailing separators ('<', '.', ':') make the string invalid.
  const std::string enable_features = ":Feature,.Feature";
  for (const auto& enable_feature :
       FeatureList::SplitFeatureListString(enable_features)) {
    std::string feature_name;
    std::string study;
    std::string group;
    std::string feature_params;
    FeatureList::ParseEnableFeatureString(enable_feature, &feature_name, &study,
                                          &group, &feature_params);
  }
}

#if BUILDFLAG(ENABLE_BANNED_BASE_FEATURE_PREFIX) && \
    defined(GTEST_HAS_DEATH_TEST)
using FeatureListDeathTest = FeatureListTest;
TEST_F(FeatureListDeathTest, DiesWithBadFeatureName) {
  // TODO(dcheng): Add a nocompile version of this test. In general, people
  // should not be constructing features at runtime anyway but just in case...
  EXPECT_DEATH(
      Feature(
          StrCat({BUILDFLAG(BANNED_BASE_FEATURE_PREFIX), "MyFeature"}).c_str(),
          FEATURE_DISABLED_BY_DEFAULT,
          /*is_runtime_mutable=*/false,
          internal::FeatureMacroHandshake::kSecret),
      StrCat({"Invalid feature name ", BUILDFLAG(BANNED_BASE_FEATURE_PREFIX),
              "MyFeature"}));
}

TEST_F(FeatureListDeathTest, DiesWithBadMutableFeatureName) {
  // TODO(dcheng): Add a nocompile version of this test. In general, people
  // should not be constructing features at runtime anyway but just in case...
  EXPECT_DEATH(
      Feature(
          StrCat({BUILDFLAG(BANNED_BASE_FEATURE_PREFIX), "MyFeature"}).c_str(),
          FEATURE_DISABLED_BY_DEFAULT,
          /*is_runtime_mutable=*/true,
          internal::FeatureMacroHandshake::kSecret),
      StrCat({"Invalid feature name ", BUILDFLAG(BANNED_BASE_FEATURE_PREFIX),
              "MyFeature"}));
}
#endif  // BUILDFLAG(ENABLE_BANNED_BASE_FEATURE_PREFIX) &&
        // defined(GTEST_HAS_DEATH_TEST)

TEST(FeatureListAccessorTest, DefaultStates) {
  test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<FeatureList>();
  auto feature_list_accessor = feature_list->ConstructAccessor();
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_EQ(feature_list_accessor->GetOverrideStateByFeatureName(
                kFeatureOnByDefault.name),
            FeatureList::OVERRIDE_USE_DEFAULT);
  EXPECT_EQ(feature_list_accessor->GetOverrideStateByFeatureName(
                kFeatureOffByDefault.name),
            FeatureList::OVERRIDE_USE_DEFAULT);
}

TEST(FeatureListAccessorTest, InitFromCommandLine) {
  struct TestCases {
    const char* enable_features;
    const char* disable_features;
    FeatureList::OverrideState expected_feature_on_state;
    FeatureList::OverrideState expected_feature_off_state;
  };
  auto test_cases = std::to_array<TestCases>({
      {"", "", FeatureList::OVERRIDE_USE_DEFAULT,
       FeatureList::OVERRIDE_USE_DEFAULT},
      {"OffByDefault", "", FeatureList::OVERRIDE_USE_DEFAULT,
       FeatureList::OVERRIDE_ENABLE_FEATURE},
      {"OffByDefault", "OnByDefault", FeatureList::OVERRIDE_DISABLE_FEATURE,
       FeatureList::OVERRIDE_ENABLE_FEATURE},
      {"OnByDefault,OffByDefault", "", FeatureList::OVERRIDE_ENABLE_FEATURE,
       FeatureList::OVERRIDE_ENABLE_FEATURE},
      {"", "OnByDefault,OffByDefault", FeatureList::OVERRIDE_DISABLE_FEATURE,
       FeatureList::OVERRIDE_DISABLE_FEATURE},
      // In the case an entry is both, disable takes precedence.
      {"OnByDefault", "OnByDefault,OffByDefault",
       FeatureList::OVERRIDE_DISABLE_FEATURE,
       FeatureList::OVERRIDE_DISABLE_FEATURE},
  });

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: [%s] [%s]", i,
                                    test_case.enable_features,
                                    test_case.disable_features));

    test::ScopedFeatureList scoped_feature_list;
    auto feature_list = std::make_unique<FeatureList>();
    auto feature_list_accessor = feature_list->ConstructAccessor();
    feature_list->InitFromCommandLine(test_case.enable_features,
                                      test_case.disable_features);
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_EQ(test_case.expected_feature_on_state,
              feature_list_accessor->GetOverrideStateByFeatureName(
                  kFeatureOnByDefault.name))
        << i;
    EXPECT_EQ(test_case.expected_feature_off_state,
              feature_list_accessor->GetOverrideStateByFeatureName(
                  kFeatureOffByDefault.name))
        << i;
  }
}

TEST(FeatureListAccessorTest, InitFromCommandLineWithFeatureParams) {
  struct TestCases {
    const std::string enable_features;
    const std::map<std::string, std::string> expected_feature_params;
  };
  auto test_cases = std::to_array<TestCases>({
      {"Feature:x/100/y/test", {{"x", "100"}, {"y", "test"}}},
      {"Feature<Trial:asdf/ghjkl/y/123", {{"asdf", "ghjkl"}, {"y", "123"}}},
  });

  // Clear global state so that repeated runs of this test don't flake.
  // When https://crrev.com/c/3694674 is submitted, we should be able to remove
  // this.
  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(test_case.enable_features);

    test::ScopedFeatureList scoped_feature_list;
    auto feature_list = std::make_unique<FeatureList>();
    auto feature_list_accessor = feature_list->ConstructAccessor();
    feature_list->InitFromCommandLine(test_case.enable_features, "");
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_EQ(FeatureList::OVERRIDE_ENABLE_FEATURE,
              feature_list_accessor->GetOverrideStateByFeatureName("Feature"))
        << i;
    std::map<std::string, std::string> actual_params;
    EXPECT_TRUE(feature_list_accessor->GetParamsByFeatureName("Feature",
                                                              &actual_params))
        << i;
    EXPECT_EQ(test_case.expected_feature_params, actual_params) << i;
  }
}

// Test only class to verify correctness of
// FeatureList::VisitFeaturesAndParams().
class TestFeatureVisitor : public FeatureVisitor {
 public:
  TestFeatureVisitor() = default;

  TestFeatureVisitor(const TestFeatureVisitor&) = delete;
  TestFeatureVisitor& operator=(const TestFeatureVisitor&) = delete;

  ~TestFeatureVisitor() override = default;

  struct VisitedFeatureState {
    auto operator<=>(const VisitedFeatureState&) const = default;

    std::string feature_name;
    const base::FeatureList::OverrideState override_state;
    base::FieldTrialParams params;
    std::string trial_name;
    std::string group_name;
  };

  void Visit(const std::string& feature_name,
             FeatureList::OverrideState override_state,
             const FieldTrialParams& params,
             const std::string& trial_name,
             const std::string& group_name) override {
    feature_state_.insert(TestFeatureVisitor::VisitedFeatureState{
        feature_name, override_state, params, trial_name, group_name});
  }

  const std::multiset<TestFeatureVisitor::VisitedFeatureState>&
  feature_state() {
    return feature_state_;
  }

 private:
  std::multiset<VisitedFeatureState> feature_state_;
};

// Makes test output human readable.
std::ostream& operator<<(std::ostream& out,
                         const TestFeatureVisitor::VisitedFeatureState& state) {
  out << ".feature_name='" << state.feature_name
      << "', .override_state=" << state.override_state << ", .params={";

  for (const auto& param : state.params) {
    out << param.first << "=" << param.second << ", ";
  }

  out << "}, .trial_name='" << state.trial_name << "', .group_name='"
      << state.group_name << "'";
  return out;
}

TEST(TestFeatureVisitor, FeatureWithNoFieldTrial) {
  base::test::ScopedFeatureList outer_scope;
  outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(/*enabled_features=*/{kFeatureOffByDefault},
                                /*disabled_features=*/{kFeatureOnByDefault});

  TestFeatureVisitor visitor;
  base::FeatureList::VisitFeaturesAndParams(visitor);
  std::multiset<TestFeatureVisitor::VisitedFeatureState> actual_feature_state =
      visitor.feature_state();

  std::multiset<TestFeatureVisitor::VisitedFeatureState>
      expected_feature_state = {
          {"OnByDefault", FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE,
           FieldTrialParams{}, "", ""},
          {"OffByDefault", FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
           FieldTrialParams{}, "", ""},
      };

  EXPECT_EQ(actual_feature_state, expected_feature_state);
}

TEST(TestFeatureVisitor, FeatureOverrideUseDefault) {
  base::test::ScopedFeatureList outer_scope;
  outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

  auto feature_list = std::make_unique<base::FeatureList>();
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial("TrialExample", "A");
  feature_list->RegisterFieldTrialOverride(
      "TestFeature", base::FeatureList::OVERRIDE_USE_DEFAULT, trial);

  base::test::ScopedFeatureList initialized_feature_list;
  initialized_feature_list.InitWithFeatureList(std::move(feature_list));

  TestFeatureVisitor visitor;
  base::FeatureList::VisitFeaturesAndParams(visitor);
  std::multiset<TestFeatureVisitor::VisitedFeatureState> actual_feature_state =
      visitor.feature_state();

  std::multiset<TestFeatureVisitor::VisitedFeatureState>
      expected_feature_state = {
          {"TestFeature", FeatureList::OverrideState::OVERRIDE_USE_DEFAULT,
           FieldTrialParams{}, "TrialExample", "A"}};

  EXPECT_EQ(actual_feature_state, expected_feature_state);
}

TEST(TestFeatureVisitor, FeatureHasParams) {
  base::test::ScopedFeatureList outer_scope;
  outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

  base::test::ScopedFeatureList initialized_feature_list;

  initialized_feature_list.InitFromCommandLine(
      /*enable_features=*/"TestFeature<foo.bar:k1/v1/k2/v2",
      /*disable_features=*/"");

  const std::multiset<TestFeatureVisitor::VisitedFeatureState>
      expected_feature_state = {
          {"TestFeature", FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
           FieldTrialParams{{"k1", "v1"}, {"k2", "v2"}}, "foo", "bar"},
      };

  {  // Check cached params.
    TestFeatureVisitor visitor;
    base::FeatureList::VisitFeaturesAndParams(visitor);
    std::multiset<TestFeatureVisitor::VisitedFeatureState>
        actual_feature_state = visitor.feature_state();

    EXPECT_EQ(actual_feature_state, expected_feature_state);
  }

  {  // Check that we fetch params from shared memory.
    FieldTrialList::InstantiateFieldTrialAllocatorIfNeeded();
    FieldTrialParamAssociator::GetInstance()->ClearAllCachedParamsForTesting();

    TestFeatureVisitor visitor;
    base::FeatureList::VisitFeaturesAndParams(visitor);
    std::multiset<TestFeatureVisitor::VisitedFeatureState>
        actual_feature_state = visitor.feature_state();

    EXPECT_EQ(actual_feature_state, expected_feature_state);
  }
}

TEST(TestFeatureVisitor, FeatureWithPrefix) {
  base::test::ScopedFeatureList outer_scope;
  outer_scope.InitWithEmptyFeatureAndFieldTrialLists();

  base::test::ScopedFeatureList initialized_feature_list;

  initialized_feature_list.InitFromCommandLine(
      /*enable_features=*/
      "AFeature,AnotherFeature,TestFeature,TestFeature2,PrefixedFeature,"
      "PrefixedFeature2",
      /*disable_features=*/"");

  TestFeatureVisitor visitor;
  base::FeatureList::VisitFeaturesAndParams(visitor, "Prefixed");
  std::multiset<TestFeatureVisitor::VisitedFeatureState> actual_feature_state =
      visitor.feature_state();

  std::multiset<TestFeatureVisitor::VisitedFeatureState>
      expected_feature_state = {
          {"PrefixedFeature",
           FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
           FieldTrialParams{}, "", ""},
          {"PrefixedFeature2",
           FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
           FieldTrialParams{}, "", ""},
      };

  EXPECT_EQ(actual_feature_state, expected_feature_state);
}

TEST_F(FeatureListTest, HistogramLogging) {
  FeatureList::ClearInstanceForTesting();
  FeatureList::ResetEarlyFeatureAccessTrackerForTesting();
  FeatureList::ClearFeatureCachedValueForTesting(kEarlyFeature);
  FeatureList::ClearFeatureCachedValueForTesting(kLateFeature);

  FeatureList::IsEnabled(kEarlyFeature);
  histogram_tester.ExpectUniqueSample("Variations.FeatureAccess",
                                      HashFieldTrialName("EarlyFeature"), 1);
  histogram_tester.ExpectUniqueSample("Variations.FeatureAccessEarly",
                                      HashFieldTrialName("EarlyFeature"), 1);

  // Access again, should not log again.
  FeatureList::IsEnabled(kEarlyFeature);
  histogram_tester.ExpectUniqueSample("Variations.FeatureAccess",
                                      HashFieldTrialName("EarlyFeature"), 1);
  histogram_tester.ExpectUniqueSample("Variations.FeatureAccessEarly",
                                      HashFieldTrialName("EarlyFeature"), 1);

  // Initialize FeatureList.
  // We need to reset the tracker before setting the instance, otherwise
  // SetInstance will CHECK that no feature was accessed early.
  FeatureList::ResetEarlyFeatureAccessTrackerForTesting();
  auto feature_list = std::make_unique<FeatureList>();
  feature_list->InitFromCommandLine("", "");
  FeatureList::SetInstance(std::move(feature_list));

  // Access a new feature after initialization.
  FeatureList::IsEnabled(kLateFeature);

  histogram_tester.ExpectBucketCount("Variations.FeatureAccess",
                                     HashFieldTrialName("LateFeature"), 1);
  histogram_tester.ExpectTotalCount("Variations.FeatureAccessEarly", 1);

  // Access again, should not log again.
  FeatureList::IsEnabled(kLateFeature);
  histogram_tester.ExpectBucketCount("Variations.FeatureAccess",
                                     HashFieldTrialName("LateFeature"), 1);

  // Access the early feature again. Should not log again.
  FeatureList::IsEnabled(kEarlyFeature);
  histogram_tester.ExpectBucketCount("Variations.FeatureAccess",
                                     HashFieldTrialName("EarlyFeature"), 1);
}

TEST_F(FeatureListTest, RuntimeMutableFeatureDefine) {
  // Validate the 2-argument version of the macro.
  EXPECT_TRUE(kRuntimeMutableFeature.IsRuntimeMutable());
  EXPECT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityEnabled());
  EXPECT_STREQ("RuntimeMutableFeature", kRuntimeMutableFeature.name);

  // Validate the 3-argument version of the macro.
  EXPECT_TRUE(kRuntimeMutableFeature3Args.IsRuntimeMutable());
  EXPECT_FALSE(kRuntimeMutableFeature3Args.HasRuntimeMutabilityEnabled());
  EXPECT_STREQ("RuntimeMutableFeature3Args", kRuntimeMutableFeature3Args.name);
}

namespace {

// Data structure to hold the arguments passed to the callback.
struct RuntimeMutabilityCallbackData {
  std::string feature_name;
  std::string trial_name;
  std::string group_name;
  FeatureList::OverrideState state;
};

// Callback function to verify that the callback is invoked with the correct
// arguments and to record the arguments in the RuntimeMutabilityCallbackData
// struct.
void RuntimeMutabilityCallback(
    int* calls,
    RuntimeMutabilityCallbackData* data,
    std::reference_wrapper<const base::Feature> feature,
    std::string_view trial,
    std::string_view group,
    FeatureList::OverrideState state) {
  (*calls)++;
  data->feature_name = feature.get().name;
  data->trial_name = std::string(trial);
  data->group_name = std::string(group);
  data->state = state;
}

}  // namespace

TEST_F(FeatureListTest, EnableRuntimeMutability) {
  FeatureList::ClearInstanceForTesting();
  auto feature_list = std::make_unique<FeatureList>();

  EXPECT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityEnabled());

  int callback_calls = 0;
  RuntimeMutabilityCallbackData callback_data;
  feature_list->EnableRuntimeMutability(
      kRuntimeMutableFeature,
      base::BindRepeating(RuntimeMutabilityCallback,
                          base::Unretained(&callback_calls),
                          base::Unretained(&callback_data)));

  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_TRUE(kRuntimeMutableFeature.HasRuntimeMutabilityEnabled());

  EXPECT_EQ(0, callback_calls);
}

TEST_F(FeatureListTest, RuntimeMutability_CommandLineOverridePrecedence) {
  // Reset instance to initialize with command line option.
  FeatureList::ClearInstanceForTesting();
  auto feature_list = std::make_unique<FeatureList>();
  feature_list->InitFromCommandLine(kRuntimeMutableFeature.name, "");
  FeatureList* raw_list_ptr = feature_list.get();

  int callback_calls = 0;
  RuntimeMutabilityCallbackData callback_data;
  raw_list_ptr->EnableRuntimeMutability(
      kRuntimeMutableFeature,
      base::BindRepeating(RuntimeMutabilityCallback,
                          base::Unretained(&callback_calls),
                          base::Unretained(&callback_data)));

  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // Pre-conditions: It is enabled via command line.
  EXPECT_TRUE(FeatureList::IsEnabled(kRuntimeMutableFeature));

  // Updating the state dynamically should have no effect on command line
  // overridden features.
  raw_list_ptr->UpdateRuntimeMutableFeatureState(
      "TrialX", "GroupX", kRuntimeMutableFeature.name,
      FeatureList::OVERRIDE_DISABLE_FEATURE);

  // Verify that the state is unchanged and the callback is not invoked.
  EXPECT_EQ(0, callback_calls);
  EXPECT_TRUE(FeatureList::IsEnabled(kRuntimeMutableFeature));

  histogram_tester.ExpectUniqueSample(
      kRuntimeMutabilityResult,
      RuntimeMutabilityResult::kFailure_CommandLineOverride, 1);
  histogram_tester.ExpectUniqueSample(
      kRuntimeMutabilityErrorFeatureName,
      static_cast<int>(base::HashFieldTrialName(kRuntimeMutableFeature.name)),
      1);
}

TEST_F(FeatureListTest, RuntimeMutability_UpdateRuntimeMutableFeatureState) {
  FeatureList::ClearInstanceForTesting();

  int callback_calls = 0;
  RuntimeMutabilityCallbackData callback_data;
  test::ScopedFeatureList scoped_feature_list;
  {
    auto feature_list = std::make_unique<FeatureList>();
    feature_list->EnableRuntimeMutability(
        kRuntimeMutableFeature,
        base::BindRepeating(RuntimeMutabilityCallback,
                            base::Unretained(&callback_calls),
                            base::Unretained(&callback_data)));

    scoped_feature_list.InitWithFeatureList(std::move(feature_list));
  }
  // By default, it should be enabled (default state).
  EXPECT_TRUE(FeatureList::IsEnabled(kRuntimeMutableFeature));

  // Now update the state to disabled (the only supported scenario for V0)
  FeatureList::GetInstance()->UpdateRuntimeMutableFeatureState(
      "TrialA", "GroupA", kRuntimeMutableFeature.name,
      FeatureList::OVERRIDE_DISABLE_FEATURE);

  EXPECT_EQ(1, callback_calls);
  EXPECT_EQ(kRuntimeMutableFeature.name, callback_data.feature_name);
  EXPECT_EQ("TrialA", callback_data.trial_name);
  EXPECT_EQ("GroupA", callback_data.group_name);
  EXPECT_EQ(FeatureList::OVERRIDE_DISABLE_FEATURE, callback_data.state);

  // Verify that the dynamic check bypasses cache and reflects the updated
  // state!
  EXPECT_FALSE(FeatureList::IsEnabled(kRuntimeMutableFeature));

  // Attempting to re-enable it should have no effect.
  FeatureList::GetInstance()->UpdateRuntimeMutableFeatureState(
      "TrialB", "GroupB", kRuntimeMutableFeature.name,
      FeatureList::OVERRIDE_ENABLE_FEATURE);

  // The initial enabling of runtime mutability is logged as a success.
  histogram_tester.ExpectBucketCount(kRuntimeMutabilityResult,
                                     RuntimeMutabilityResult::kSuccess, 1);

  EXPECT_EQ(1, callback_calls);  // Callback should not be invoked.
  EXPECT_EQ(FeatureList::OVERRIDE_DISABLE_FEATURE, callback_data.state);
  EXPECT_FALSE(FeatureList::IsEnabled(kRuntimeMutableFeature));

  // The update attempting to enable the feature is logged as a failure, because
  // enabling features is not supported in V0.
  histogram_tester.ExpectBucketCount(
      kRuntimeMutabilityResult,
      RuntimeMutabilityResult::kFailure_StateNotSupported, 1);
}

#if defined(GTEST_HAS_DEATH_TEST)
TEST_F(FeatureListTest, RuntimeMutability_EnableRuntimeMutabilityAfterInit) {
  FeatureList::ClearInstanceForTesting();

  ASSERT_TRUE(kRuntimeMutableFeature.IsRuntimeMutable());
  ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityEnabled());
  ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityDisabled());

  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::make_unique<FeatureList>());

  // Attempting to enable runtime mutability once it has been disabled will
  // create the override entry (so that we can CHECK that it isn't registered
  // twice), but otherwise has no effect.
  int callback_calls = 0;
  RuntimeMutabilityCallbackData callback_data;
  EXPECT_DEATH(FeatureList::GetInstance()->EnableRuntimeMutability(
                   kRuntimeMutableFeature,
                   base::BindRepeating(RuntimeMutabilityCallback,
                                       base::Unretained(&callback_calls),
                                       base::Unretained(&callback_data))),
               "");  // CHECK messages are stripped from release builds.
}
#endif  // defined(GTEST_HAS_DEATH_TEST)

#if defined(GTEST_HAS_DEATH_TEST)
TEST_F(FeatureListTest, RuntimeMutability_MultipleEnableCalls) {
  FeatureList::ClearInstanceForTesting();

  ASSERT_TRUE(kRuntimeMutableFeature.IsRuntimeMutable());
  ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityEnabled());
  ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityDisabled());

  auto feature_list = std::make_unique<FeatureList>();

  // Attempting to enable runtime mutability once it has been disabled will
  // create the override entry (so that we can CHECK that it isn't registered
  // twice), but otherwise has no effect.
  int callback_calls = 0;
  RuntimeMutabilityCallbackData callback_data;
  feature_list->EnableRuntimeMutability(
      kRuntimeMutableFeature,
      base::BindRepeating(RuntimeMutabilityCallback,
                          base::Unretained(&callback_calls),
                          base::Unretained(&callback_data)));
  ASSERT_TRUE(kRuntimeMutableFeature.HasRuntimeMutabilityEnabled());
  ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityDisabled());

  // CHECK will fire if we attempt to enable runtime mutability more than once.
  EXPECT_DEATH(feature_list->EnableRuntimeMutability(
                   kRuntimeMutableFeature,
                   base::BindRepeating(RuntimeMutabilityCallback,
                                       base::Unretained(&callback_calls),
                                       base::Unretained(&callback_data))),
               "");  // CHECK messages are stripped from release builds.
}
#endif  // defined(GTEST_HAS_DEATH_TEST)

#if defined(GTEST_HAS_DEATH_TEST)
TEST_F(FeatureListTest, RuntimeMutability_MultipleEnableCalls_EarlyAccess) {
  FeatureList::ClearInstanceForTesting();

  ASSERT_TRUE(kRuntimeMutableFeature.IsRuntimeMutable());
  ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityEnabled());
  ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityDisabled());
  ASSERT_FALSE(kRuntimeMutableFeature.WasAccessedEarly());

  // Access the feature before enabling runtime mutability.
  ASSERT_TRUE(FeatureList::IsEnabled(kRuntimeMutableFeature));
  ASSERT_TRUE(kRuntimeMutableFeature.WasAccessedEarly());

  auto feature_list = std::make_unique<FeatureList>();

  // Attempting to enable runtime mutability for an early-accessed feature will
  // disable runtime mutability and log an error.
  int callback_calls = 0;
  RuntimeMutabilityCallbackData callback_data;
  feature_list->EnableRuntimeMutability(
      kRuntimeMutableFeature,
      base::BindRepeating(RuntimeMutabilityCallback,
                          base::Unretained(&callback_calls),
                          base::Unretained(&callback_data)));
  ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityEnabled());
  ASSERT_TRUE(kRuntimeMutableFeature.HasRuntimeMutabilityDisabled());

  // TODO: crbug.com/482451012 - Update this test to expect DEATH or some
  // metrics to be captured on early access.

  // CHECK will fire if we attempt to enable runtime mutability more than once.
  EXPECT_DEATH(feature_list->EnableRuntimeMutability(
                   kRuntimeMutableFeature,
                   base::BindRepeating(RuntimeMutabilityCallback,
                                       base::Unretained(&callback_calls),
                                       base::Unretained(&callback_data))),
               "");  // CHECK messages are stripped from release builds.
}
#endif  // defined(GTEST_HAS_DEATH_TEST)

TEST_F(FeatureListTest, RuntimeMutability_EarlyAccessDisablesMutability) {
  FeatureList::ClearInstanceForTesting();

  ASSERT_TRUE(kRuntimeMutableFeature.IsRuntimeMutable());
  ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityEnabled());
  ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityDisabled());
  ASSERT_FALSE(kRuntimeMutableFeature.WasAccessedEarly());

  int callback_calls = 0;
  RuntimeMutabilityCallbackData callback_data;
  test::ScopedFeatureList scoped_feature_list;
  {
    // Check that the state is enabled (the default state). Note that this check
    // is occurring before we enable runtime mutability for this feature. So,
    // the default state is returned, and the feature is marked as having been
    // accessed early.
    ASSERT_TRUE(FeatureList::IsEnabled(kRuntimeMutableFeature));
    ASSERT_TRUE(kRuntimeMutableFeature.WasAccessedEarly());

    // The runtime mutability state has not been updated yet. This is handled
    // internally by the FeatureList, which hasn't been implicated yet.
    ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityEnabled());
    ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityDisabled());

    // Attempting to enable runtime mutability on an early-accessed feature
    // will disable runtime mutability and log an error.
    auto feature_list = std::make_unique<FeatureList>();
    feature_list->EnableRuntimeMutability(
        kRuntimeMutableFeature,
        base::BindRepeating(RuntimeMutabilityCallback,
                            base::Unretained(&callback_calls),
                            base::Unretained(&callback_data)));
    ASSERT_FALSE(kRuntimeMutableFeature.HasRuntimeMutabilityEnabled());
    ASSERT_TRUE(kRuntimeMutableFeature.HasRuntimeMutabilityDisabled());

    // TODO: crbug.com/482451012 - Update this test to expect DEATH or some
    // metrics to be captured on early access.

    scoped_feature_list.InitWithFeatureList(std::move(feature_list));
  }

  // The feature remains marked as having runtime mutability disabled.
  EXPECT_TRUE(kRuntimeMutableFeature.HasRuntimeMutabilityDisabled());

  // Attempting to disable the feature has no effect.
  EXPECT_FALSE(FeatureList::GetInstance()->UpdateRuntimeMutableFeatureState(
      "TrialB", "GroupB", kRuntimeMutableFeature.name,
      FeatureList::OVERRIDE_DISABLE_FEATURE));
  EXPECT_EQ(0, callback_calls);  // Callback should not be invoked.
  EXPECT_TRUE(FeatureList::IsEnabled(kRuntimeMutableFeature));

  // TODO: crbug.com/482451012 - Update this test to expect DEATH or some
  // metrics to be captured on early access.
  histogram_tester.ExpectUniqueSample(kRuntimeMutabilityResult,
                                      RuntimeMutabilityResult::kFailure, 1);
  histogram_tester.ExpectUniqueSample(
      kRuntimeMutabilityErrorFeatureName,
      static_cast<int>(base::HashFieldTrialName(kRuntimeMutableFeature.name)),
      1);
}

TEST_F(FeatureListTest, RuntimeMutability_FeatureParamBypassCache) {
  constexpr char kTrialName[] = "TrialName";
  constexpr char kGroupName[] = "GroupName";

  // Create a new instance of FeatureList for this test.
  FeatureList::ClearInstanceForTesting();
  test::ScopedFeatureList scoped_feature_list;
  int callback_calls = 0;
  RuntimeMutabilityCallbackData callback_data;

  // The feature list is initialized within this scope.
  {
    auto feature_list = std::make_unique<FeatureList>();

    // Create a field trial that override-enables a feature and sets a
    // non-default feature param value.
    MockEntropyProvider entropy_provider;
    scoped_refptr<base::FieldTrial> trial(
        base::FieldTrialList::FactoryGetFieldTrial(
            kTrialName, 100, kGroupName, entropy_provider,
            /*randomization_seed=*/0, /*is_low_anonymity=*/false));
    trial->SetForced();
    ASSERT_TRUE(base::AssociateFieldTrialParams(
        kTrialName, kGroupName,
        FieldTrialParams{{kRuntimeMutableFeatureParam.name, "99999"}}));
    feature_list->RegisterFieldTrialOverride(
        kRuntimeMutableFeature.name, FeatureList::OVERRIDE_ENABLE_FEATURE,
        trial.get());
    trial->Activate();

    // Enable runtime mutability for the feature.
    feature_list->EnableRuntimeMutability(
        kRuntimeMutableFeature,
        base::BindRepeating(RuntimeMutabilityCallback,
                            base::Unretained(&callback_calls),
                            base::Unretained(&callback_data)));

    // Finalize the feature list.
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));
  }

  // The overridden enabled state and overridden param should be reflected.
  EXPECT_TRUE(FeatureList::IsEnabled(kRuntimeMutableFeature));
  EXPECT_EQ(99999, kRuntimeMutableFeatureParam.Get());

  // Update parameters/configuration.
  base::FeatureList::GetInstance()->UpdateRuntimeMutableFeatureState(
      kTrialName, kGroupName, kRuntimeMutableFeature.name,
      FeatureList::OVERRIDE_DISABLE_FEATURE);

  // The runtime overridden state (disabled) and default param value should be
  // reflected.
  EXPECT_FALSE(FeatureList::IsEnabled(kRuntimeMutableFeature));
  EXPECT_EQ(12345, kRuntimeMutableFeatureParam.Get());

  // The runtime mutability interactions should be logged.
  histogram_tester.ExpectUniqueSample(kRuntimeMutabilityResult,
                                      RuntimeMutabilityResult::kSuccess, 1);
  histogram_tester.ExpectTotalCount(kRuntimeMutabilityErrorFeatureName, 0);
}

}  // namespace base
