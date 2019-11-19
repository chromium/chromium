// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"

#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_field_trial_list_resetter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

constexpr char kFeatureOnByDefaultName[] = "OnByDefault";
struct Feature kFeatureOnByDefault {
  kFeatureOnByDefaultName, FEATURE_ENABLED_BY_DEFAULT
};

constexpr char kFeatureOffByDefaultName[] = "OffByDefault";
struct Feature kFeatureOffByDefault {
  kFeatureOffByDefaultName, FEATURE_DISABLED_BY_DEFAULT
};

std::string SortFeatureListString(const std::string& feature_list) {
  std::vector<base::StringPiece> features =
      FeatureList::SplitFeatureListString(feature_list);
  std::sort(features.begin(), features.end());
  return JoinString(features, ",");
}

}  // namespace

class FeatureListTest : public testing::Test {
 public:
  FeatureListTest() {
    // Provide an empty FeatureList to each test by default.
    scoped_feature_list_.InitWithFeatureList(std::make_unique<FeatureList>());
  }
  ~FeatureListTest() override = default;

 private:
  test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FeatureListTest);
};

TEST_F(FeatureListTest, DefaultStates) {
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
}

TEST_F(FeatureListTest, InitializeFromCommandLine) {
  struct {
    const char* enable_features;
    const char* disable_features;
    bool expected_feature_on_state;
    bool expected_feature_off_state;
  } test_cases[] = {
      {"", "", true, false},
      {"OffByDefault", "", true, true},
      {"OffByDefault", "OnByDefault", false, true},
      {"OnByDefault,OffByDefault", "", true, true},
      {"", "OnByDefault,OffByDefault", false, false},
      // In the case an entry is both, disable takes precedence.
      {"OnByDefault", "OnByDefault,OffByDefault", false, false},
  };

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: [%s] [%s]", i,
                                    test_case.enable_features,
                                    test_case.disable_features));

    auto feature_list = std::make_unique<FeatureList>();
    feature_list->InitializeFromCommandLine(test_case.enable_features,
                                            test_case.disable_features);
    test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_EQ(test_case.expected_feature_on_state,
              FeatureList::IsEnabled(kFeatureOnByDefault))
        << i;
    EXPECT_EQ(test_case.expected_feature_off_state,
              FeatureList::IsEnabled(kFeatureOffByDefault))
        << i;
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
  struct Feature kFeatureOnByDefault2 {
    kFeatureOnByDefaultName, FEATURE_ENABLED_BY_DEFAULT
  };
  EXPECT_FALSE(feature_list->CheckFeatureIdentity(kFeatureOnByDefault2));
}

TEST_F(FeatureListTest, FieldTrialOverrides) {
  struct {
    FeatureList::OverrideState trial1_state;
    FeatureList::OverrideState trial2_state;
  } test_cases[] = {
      {FeatureList::OVERRIDE_DISABLE_FEATURE,
       FeatureList::OVERRIDE_DISABLE_FEATURE},
      {FeatureList::OVERRIDE_DISABLE_FEATURE,
       FeatureList::OVERRIDE_ENABLE_FEATURE},
      {FeatureList::OVERRIDE_ENABLE_FEATURE,
       FeatureList::OVERRIDE_DISABLE_FEATURE},
      {FeatureList::OVERRIDE_ENABLE_FEATURE,
       FeatureList::OVERRIDE_ENABLE_FEATURE},
  };

  FieldTrial::ActiveGroup active_group;
  for (size_t i = 0; i < base::size(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]", i));

    test::ScopedFieldTrialListResetter resetter;
    FieldTrialList field_trial_list(nullptr);
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
  feature_list->InitializeFromCommandLine(kFeatureOffByDefaultName, "");

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
  feature_list->InitializeFromCommandLine("", kFeatureOffByDefaultName);

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

TEST_F(FeatureListTest, IsFeatureOverriddenFromCommandLine) {
  auto feature_list = std::make_unique<FeatureList>();

  // No features are overridden from the command line yet
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));

  // Now, enable |kFeatureOffByDefaultName| via the command-line.
  feature_list->InitializeFromCommandLine(kFeatureOffByDefaultName, "");

  // It should now be overridden for the enabled group.
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));
  EXPECT_TRUE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));

  // Register a field trial to associate with the feature and ensure that the
  // results are still the same.
  feature_list->AssociateReportingFieldTrial(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE,
      FieldTrialList::CreateFieldTrial("Trial1", "A"));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));
  EXPECT_TRUE(feature_list->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));

  // Now, register a field trial to override |kFeatureOnByDefaultName| state
  // and check that the function still returns false for that feature.
  feature_list->RegisterFieldTrialOverride(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE,
      FieldTrialList::CreateFieldTrial("Trial2", "A"));
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
  struct {
    const char* enable_features;
    const char* disable_features;
    bool expected_enable_trial_created;
    bool expected_disable_trial_created;
  } test_cases[] = {
      // If no enable/disable flags are specified, no trials should be created.
      {"", "", false, false},
      // Enabling the feature should result in the enable trial created.
      {kFeatureOffByDefaultName, "", true, false},
      // Disabling the feature should result in the disable trial created.
      {"", kFeatureOffByDefaultName, false, true},
  };

  const char kTrialName[] = "ForcingTrial";
  const char kForcedOnGroupName[] = "ForcedOn";
  const char kForcedOffGroupName[] = "ForcedOff";

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: [%s] [%s]", i,
                                    test_case.enable_features,
                                    test_case.disable_features));

    test::ScopedFieldTrialListResetter resetter;
    FieldTrialList field_trial_list(nullptr);
    auto feature_list = std::make_unique<FeatureList>();
    feature_list->InitializeFromCommandLine(test_case.enable_features,
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

TEST_F(FeatureListTest, RegisterExtraFeatureOverrides) {
  auto feature_list = std::make_unique<FeatureList>();
  std::vector<FeatureList::FeatureOverrideInfo> overrides;
  overrides.push_back({std::cref(kFeatureOnByDefault),
                       FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE});
  overrides.push_back({std::cref(kFeatureOffByDefault),
                       FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE});
  feature_list->RegisterExtraFeatureOverrides(std::move(overrides));
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOffByDefault));
}

TEST_F(FeatureListTest, InitializeFromCommandLineThenRegisterExtraOverrides) {
  auto feature_list = std::make_unique<FeatureList>();
  feature_list->InitializeFromCommandLine(kFeatureOnByDefaultName,
                                          kFeatureOffByDefaultName);
  std::vector<FeatureList::FeatureOverrideInfo> overrides;
  overrides.push_back({std::cref(kFeatureOnByDefault),
                       FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE});
  overrides.push_back({std::cref(kFeatureOffByDefault),
                       FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE});
  feature_list->RegisterExtraFeatureOverrides(std::move(overrides));
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // The InitializeFromCommandLine supersedes the RegisterExtraFeatureOverrides
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
  feature_list->InitializeFromCommandLine("A,X", "D");

  Feature feature_b = {"B", FEATURE_ENABLED_BY_DEFAULT};
  Feature feature_c = {"C", FEATURE_DISABLED_BY_DEFAULT};
  std::vector<FeatureList::FeatureOverrideInfo> overrides;
  overrides.push_back({std::cref(feature_b),
                       FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE});
  overrides.push_back({std::cref(feature_c),
                       FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE});
  feature_list->RegisterExtraFeatureOverrides(std::move(overrides));

  FieldTrial* trial = FieldTrialList::CreateFieldTrial("Trial", "Group");
  feature_list->RegisterFieldTrialOverride(kFeatureOffByDefaultName,
                                           FeatureList::OVERRIDE_ENABLE_FEATURE,
                                           trial);

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
  feature_list->InitializeFromCommandLine("A,X", "D");

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

TEST_F(FeatureListTest, InitializeFromCommandLine_WithFieldTrials) {
  FieldTrialList::CreateFieldTrial("Trial", "Group");
  auto feature_list = std::make_unique<FeatureList>();
  feature_list->InitializeFromCommandLine("A,OffByDefault<Trial,X", "D");
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_FALSE(FieldTrialList::IsTrialActive("Trial"));
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_TRUE(FieldTrialList::IsTrialActive("Trial"));
}

TEST_F(FeatureListTest, InitializeFromCommandLine_UseDefault) {
  FieldTrialList::CreateFieldTrial("T1", "Group");
  FieldTrialList::CreateFieldTrial("T2", "Group");
  auto feature_list = std::make_unique<FeatureList>();
  feature_list->InitializeFromCommandLine(
      "A,*OffByDefault<T1,*OnByDefault<T2,X", "D");
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_FALSE(FieldTrialList::IsTrialActive("T1"));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_TRUE(FieldTrialList::IsTrialActive("T1"));

  EXPECT_FALSE(FieldTrialList::IsTrialActive("T2"));
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_TRUE(FieldTrialList::IsTrialActive("T2"));
}

TEST_F(FeatureListTest, InitializeInstance) {
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));

  // Initialize from command line if we haven't yet.
  FeatureList::InitializeInstance("", kFeatureOnByDefaultName);
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));

  // Do not initialize from commandline if we have already.
  FeatureList::InitializeInstance(kFeatureOffByDefaultName, "");
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

  if (original_feature_list)
    FeatureList::RestoreInstanceForTesting(std::move(original_feature_list));
}

TEST_F(FeatureListTest, StoreAndRetrieveFeaturesFromSharedMemory) {
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);

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

  std::unique_ptr<base::FeatureList> feature_list2(new base::FeatureList);

  // Check that the new feature list is empty.
  EXPECT_FALSE(feature_list2->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));
  EXPECT_FALSE(feature_list2->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));

  feature_list2->InitializeFromSharedMemory(&allocator);
  // Check that the new feature list now has 2 overrides.
  EXPECT_TRUE(feature_list2->IsFeatureOverriddenFromCommandLine(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_ENABLE_FEATURE));
  EXPECT_TRUE(feature_list2->IsFeatureOverriddenFromCommandLine(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE));
}

TEST_F(FeatureListTest, StoreAndRetrieveAssociatedFeaturesFromSharedMemory) {
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);

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

  std::unique_ptr<base::FeatureList> feature_list2(new base::FeatureList);
  feature_list2->InitializeFromSharedMemory(&allocator);
  feature_list2->FinalizeInitialization();

  // Check that the field trials are still associated.
  FieldTrial* associated_trial1 =
      feature_list2->GetAssociatedFieldTrial(kFeatureOnByDefault);
  FieldTrial* associated_trial2 =
      feature_list2->GetAssociatedFieldTrial(kFeatureOffByDefault);
  EXPECT_EQ(associated_trial1, trial1);
  EXPECT_EQ(associated_trial2, trial2);
}

}  // namespace base
