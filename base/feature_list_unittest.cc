// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
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
  FeatureListTest() : feature_list_(nullptr) {
    RegisterFeatureListInstance(std::make_unique<FeatureList>());
  }
  ~FeatureListTest() override { ClearFeatureListInstance(); }

  void RegisterFeatureListInstance(std::unique_ptr<FeatureList> feature_list) {
    FeatureList::ClearInstanceForTesting();
    feature_list_ = feature_list.get();
    FeatureList::SetInstance(std::move(feature_list));
  }
  void ClearFeatureListInstance() {
    FeatureList::ClearInstanceForTesting();
    feature_list_ = nullptr;
  }

  FeatureList* feature_list() { return feature_list_; }

 private:
  // Weak. Owned by the FeatureList::SetInstance().
  FeatureList* feature_list_;

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

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: [%s] [%s]", i,
                                    test_case.enable_features,
                                    test_case.disable_features));

    ClearFeatureListInstance();
    std::unique_ptr<FeatureList> feature_list(new FeatureList);
    feature_list->InitializeFromCommandLine(test_case.enable_features,
                                            test_case.disable_features);
    RegisterFeatureListInstance(std::move(feature_list));

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

  // Call it twice for each feature at the top of the file, since the first call
  // makes it remember the entry and the second call will verify it.
  EXPECT_TRUE(feature_list()->CheckFeatureIdentity(kFeatureOnByDefault));
  EXPECT_TRUE(feature_list()->CheckFeatureIdentity(kFeatureOnByDefault));
  EXPECT_TRUE(feature_list()->CheckFeatureIdentity(kFeatureOffByDefault));
  EXPECT_TRUE(feature_list()->CheckFeatureIdentity(kFeatureOffByDefault));

  // Now, call it with a distinct struct for |kFeatureOnByDefaultName|, which
  // should return false.
  struct Feature kFeatureOnByDefault2 {
    kFeatureOnByDefaultName, FEATURE_ENABLED_BY_DEFAULT
  };
  EXPECT_FALSE(feature_list()->CheckFeatureIdentity(kFeatureOnByDefault2));
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
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]", i));

    ClearFeatureListInstance();

    FieldTrialList field_trial_list(nullptr);
    std::unique_ptr<FeatureList> feature_list(new FeatureList);

    FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("TrialExample1", "A");
    FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("TrialExample2", "B");
    feature_list->RegisterFieldTrialOverride(kFeatureOnByDefaultName,
                                             test_case.trial1_state, trial1);
    feature_list->RegisterFieldTrialOverride(kFeatureOffByDefaultName,
                                             test_case.trial2_state, trial2);
    RegisterFeatureListInstance(std::move(feature_list));

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
  FieldTrialList field_trial_list(nullptr);
  std::unique_ptr<FeatureList> feature_list(new FeatureList);

  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("TrialExample1", "A");
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("TrialExample2", "B");
  feature_list->RegisterFieldTrialOverride(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial1);
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial2);
  RegisterFeatureListInstance(std::move(feature_list));

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

TEST_F(FeatureListTest, CommandLineTakesPrecedenceOverFieldTrial) {
  ClearFeatureListInstance();

  FieldTrialList field_trial_list(nullptr);
  std::unique_ptr<FeatureList> feature_list(new FeatureList);

  // The feature is explicitly enabled on the command-line.
  feature_list->InitializeFromCommandLine(kFeatureOffByDefaultName, "");

  // But the FieldTrial would set the feature to disabled.
  FieldTrial* trial = FieldTrialList::CreateFieldTrial("TrialExample2", "A");
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_DISABLE_FEATURE, trial);
  RegisterFeatureListInstance(std::move(feature_list));

  EXPECT_FALSE(FieldTrialList::IsTrialActive(trial->trial_name()));
  // Command-line should take precedence.
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOffByDefault));
  // Since the feature is on due to the command-line, and not as a result of the
  // field trial, the field trial should not be activated (since the Associate*
  // API wasn't used.)
  EXPECT_FALSE(FieldTrialList::IsTrialActive(trial->trial_name()));
}

TEST_F(FeatureListTest, IsFeatureOverriddenFromCommandLine) {
  ClearFeatureListInstance();

  FieldTrialList field_trial_list(nullptr);
  std::unique_ptr<FeatureList> feature_list(new FeatureList);

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
  RegisterFeatureListInstance(std::move(feature_list));

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

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: [%s] [%s]", i,
                                    test_case.enable_features,
                                    test_case.disable_features));

    ClearFeatureListInstance();

    FieldTrialList field_trial_list(nullptr);
    std::unique_ptr<FeatureList> feature_list(new FeatureList);
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
    RegisterFeatureListInstance(std::move(feature_list));

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

TEST_F(FeatureListTest, GetFeatureOverrides) {
  ClearFeatureListInstance();
  FieldTrialList field_trial_list(nullptr);
  std::unique_ptr<FeatureList> feature_list(new FeatureList);
  feature_list->InitializeFromCommandLine("A,X", "D");

  FieldTrial* trial = FieldTrialList::CreateFieldTrial("Trial", "Group");
  feature_list->RegisterFieldTrialOverride(kFeatureOffByDefaultName,
                                           FeatureList::OVERRIDE_ENABLE_FEATURE,
                                           trial);

  RegisterFeatureListInstance(std::move(feature_list));

  std::string enable_features;
  std::string disable_features;
  FeatureList::GetInstance()->GetFeatureOverrides(&enable_features,
                                                  &disable_features);
  EXPECT_EQ("A,OffByDefault<Trial,X", SortFeatureListString(enable_features));
  EXPECT_EQ("D", SortFeatureListString(disable_features));

  FeatureList::GetInstance()->GetCommandLineFeatureOverrides(&enable_features,
                                                             &disable_features);
  EXPECT_EQ("A,X", SortFeatureListString(enable_features));
  EXPECT_EQ("D", SortFeatureListString(disable_features));
}

TEST_F(FeatureListTest, GetFeatureOverrides_UseDefault) {
  ClearFeatureListInstance();
  FieldTrialList field_trial_list(nullptr);
  std::unique_ptr<FeatureList> feature_list(new FeatureList);
  feature_list->InitializeFromCommandLine("A,X", "D");

  FieldTrial* trial = FieldTrialList::CreateFieldTrial("Trial", "Group");
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial);

  RegisterFeatureListInstance(std::move(feature_list));

  std::string enable_features;
  std::string disable_features;
  FeatureList::GetInstance()->GetFeatureOverrides(&enable_features,
                                                  &disable_features);
  EXPECT_EQ("*OffByDefault<Trial,A,X", SortFeatureListString(enable_features));
  EXPECT_EQ("D", SortFeatureListString(disable_features));
}

TEST_F(FeatureListTest, GetFieldTrial) {
  ClearFeatureListInstance();
  FieldTrialList field_trial_list(nullptr);
  FieldTrial* trial = FieldTrialList::CreateFieldTrial("Trial", "Group");
  std::unique_ptr<FeatureList> feature_list(new FeatureList);
  feature_list->RegisterFieldTrialOverride(
      kFeatureOnByDefaultName, FeatureList::OVERRIDE_USE_DEFAULT, trial);
  RegisterFeatureListInstance(std::move(feature_list));

  EXPECT_EQ(trial, FeatureList::GetFieldTrial(kFeatureOnByDefault));
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kFeatureOffByDefault));
}

TEST_F(FeatureListTest, InitializeFromCommandLine_WithFieldTrials) {
  ClearFeatureListInstance();
  FieldTrialList field_trial_list(nullptr);
  FieldTrialList::CreateFieldTrial("Trial", "Group");
  std::unique_ptr<FeatureList> feature_list(new FeatureList);
  feature_list->InitializeFromCommandLine("A,OffByDefault<Trial,X", "D");
  RegisterFeatureListInstance(std::move(feature_list));

  EXPECT_FALSE(FieldTrialList::IsTrialActive("Trial"));
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_TRUE(FieldTrialList::IsTrialActive("Trial"));
}

TEST_F(FeatureListTest, InitializeFromCommandLine_UseDefault) {
  ClearFeatureListInstance();
  FieldTrialList field_trial_list(nullptr);
  FieldTrialList::CreateFieldTrial("T1", "Group");
  FieldTrialList::CreateFieldTrial("T2", "Group");
  std::unique_ptr<FeatureList> feature_list(new FeatureList);
  feature_list->InitializeFromCommandLine(
      "A,*OffByDefault<T1,*OnByDefault<T2,X", "D");
  RegisterFeatureListInstance(std::move(feature_list));

  EXPECT_FALSE(FieldTrialList::IsTrialActive("T1"));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_TRUE(FieldTrialList::IsTrialActive("T1"));

  EXPECT_FALSE(FieldTrialList::IsTrialActive("T2"));
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_TRUE(FieldTrialList::IsTrialActive("T2"));
}

TEST_F(FeatureListTest, InitializeInstance) {
  ClearFeatureListInstance();

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  FeatureList::SetInstance(std::move(feature_list));
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
  ClearFeatureListInstance();
  // This test case simulates the calling pattern found in code which does not
  // explicitly initialize the features list.
  // All IsEnabled() calls should return the default value in this scenario.
  EXPECT_EQ(nullptr, FeatureList::GetInstance());
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_EQ(nullptr, FeatureList::GetInstance());
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
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
  std::unique_ptr<SharedMemory> shm(new SharedMemory());
  shm->CreateAndMapAnonymous(4 << 10);
  SharedPersistentMemoryAllocator allocator(std::move(shm), 1, "", false);
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
  FieldTrialList field_trial_list(nullptr);
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
  std::unique_ptr<SharedMemory> shm(new SharedMemory());
  shm->CreateAndMapAnonymous(4 << 10);
  SharedPersistentMemoryAllocator allocator(std::move(shm), 1, "", false);
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
