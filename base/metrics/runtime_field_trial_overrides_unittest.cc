// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/runtime_field_trial_overrides.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/types/pass_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
class VariationsService {
 public:
  static base::PassKey<VariationsService> CreatePassKeyForTesting() {
    return base::PassKey<VariationsService>();
  }
};
}  // namespace variations

namespace base {

namespace {

std::optional<RuntimeFieldTrialOverrides::RuntimeOverrideInfo> FindOverride(
    const flat_map<std::string,
                   RuntimeFieldTrialOverrides::RuntimeOverrideInfo>& overrides,
    std::string_view trial_name) {
  auto it = overrides.find(trial_name);
  return it == overrides.end() ? std::nullopt : std::make_optional(it->second);
}

}  // namespace

class RuntimeFieldTrialOverridesTest : public ::testing::Test {
 public:
  class MockObserver : public RuntimeFieldTrialOverrides::Observer {
   public:
    void OnRuntimeFieldTrialOverride(
        const RuntimeFieldTrialOverrides::RuntimeOverrideInfo& override_info,
        std::string_view previous_override_trial_name) override {
      last_trial_name = override_info.trial_name;
      last_group_name = override_info.group_name;
      last_overridden_trial = override_info.overridden_trial;
      last_previous_override_trial_name =
          std::string(previous_override_trial_name);
      call_count++;
    }

    std::string last_trial_name;
    std::string last_group_name;
    raw_ptr<const FieldTrial> last_overridden_trial = nullptr;
    std::string last_previous_override_trial_name;
    int call_count = 0;
  };

  void TearDown() override {
    RuntimeFieldTrialOverrides::GetInstance()->ResetForTesting();
  }
};

TEST_F(RuntimeFieldTrialOverridesTest, ApplyAndGetOverrides) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(pass_key, "Trial", "Group",
                                              /*overridden_trial=*/nullptr));

  auto override_info = FindOverride(overrides->GetRuntimeOverrides(), "Trial");
  ASSERT_TRUE(override_info.has_value());
  EXPECT_EQ(override_info->trial_name, "Trial");
  EXPECT_EQ(override_info->group_name, "Group");
  EXPECT_EQ(override_info->overridden_trial, nullptr);
}

TEST_F(RuntimeFieldTrialOverridesTest, ApplyWithPreviousOverride) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  FieldTrial* trial = FieldTrialList::CreateFieldTrial("Trial", "Group");

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(pass_key, "Killswitch50Pct",
                                              "Disabled50",
                                              /*overridden_trial=*/trial));
  auto override_info =
      FindOverride(overrides->GetRuntimeOverrides(), "Killswitch50Pct");
  ASSERT_TRUE(override_info.has_value());
  EXPECT_EQ(override_info->trial_name, "Killswitch50Pct");
  EXPECT_EQ(override_info->group_name, "Disabled50");
  EXPECT_EQ(override_info->overridden_trial, trial);

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key, "Killswitch100Pct", "Disabled100", /*overridden_trial=*/trial,
      /*previous_override_trial_name=*/"Killswitch50Pct"));

  // The previous override should be removed.
  EXPECT_FALSE(FindOverride(overrides->GetRuntimeOverrides(), "Killswitch50Pct")
                   .has_value());
  EXPECT_EQ(overrides->GetRuntimeOverrides().size(), 1);

  override_info =
      FindOverride(overrides->GetRuntimeOverrides(), "Killswitch100Pct");
  ASSERT_TRUE(override_info.has_value());
  EXPECT_EQ(override_info->trial_name, "Killswitch100Pct");
  EXPECT_EQ(override_info->group_name, "Disabled100");
  EXPECT_EQ(override_info->overridden_trial, trial);
}

TEST_F(RuntimeFieldTrialOverridesTest,
       ApplyWithSameTrialNameAsPreviousOverride) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(pass_key, "Killswitch",
                                              "Disabled50Pct",
                                              /*overridden_trial=*/nullptr));
  EXPECT_TRUE(
      FindOverride(overrides->GetRuntimeOverrides(), "Killswitch").has_value());

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key, "Killswitch", "Disabled100Pct",
      /*overridden_trial=*/nullptr,
      /*previous_override_trial_name=*/"Killswitch"));

  auto override_info =
      FindOverride(overrides->GetRuntimeOverrides(), "Killswitch");
  ASSERT_TRUE(override_info.has_value());
  EXPECT_EQ(override_info->trial_name, "Killswitch");
  EXPECT_EQ(override_info->group_name, "Disabled100Pct");
  EXPECT_EQ(override_info->overridden_trial, nullptr);
  EXPECT_EQ(overrides->GetRuntimeOverrides().size(), 1);
}

TEST_F(RuntimeFieldTrialOverridesTest, ObserverNotification) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  MockObserver observer;
  overrides->AddObserver(&observer);

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(pass_key, "ObsTrial1",
                                              "ObsGroup1",
                                              /*overridden_trial=*/nullptr));
  EXPECT_EQ(observer.call_count, 1);
  EXPECT_EQ(observer.last_trial_name, "ObsTrial1");
  EXPECT_EQ(observer.last_group_name, "ObsGroup1");
  EXPECT_EQ(observer.last_overridden_trial, nullptr);
  EXPECT_EQ(observer.last_previous_override_trial_name, "");

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key, "ObsTrial2", "ObsGroup2",
      /*overridden_trial=*/nullptr,
      /*previous_override_trial_name=*/"ObsTrial1"));
  EXPECT_EQ(observer.call_count, 2);
  EXPECT_EQ(observer.last_trial_name, "ObsTrial2");
  EXPECT_EQ(observer.last_group_name, "ObsGroup2");
  EXPECT_EQ(observer.last_overridden_trial, nullptr);
  EXPECT_EQ(observer.last_previous_override_trial_name, "ObsTrial1");

  overrides->RemoveObserver(&observer);
}

TEST_F(RuntimeFieldTrialOverridesTest,
       ApplyFailsWhenPreviousOverrideDoesNotExist) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  MockObserver observer;
  overrides->AddObserver(&observer);

  EXPECT_FALSE(overrides->ApplyRuntimeOverride(
      pass_key, "Killswitch", "Disabled", /*overridden_trial=*/nullptr,
      /*previous_override_trial_name=*/"NonExistentTrial"));

  EXPECT_TRUE(overrides->GetRuntimeOverrides().empty());
  EXPECT_EQ(observer.call_count, 0);

  overrides->RemoveObserver(&observer);
}

TEST_F(RuntimeFieldTrialOverridesTest,
       ApplyFailsWhenPreviousOverrideHasDifferentOverriddenTrial) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("Trial2", "Group2");

  MockObserver observer;
  overrides->AddObserver(&observer);

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key, "Killswitch1", "Disabled1", /*overridden_trial=*/trial1));
  EXPECT_EQ(observer.call_count, 1);

  // Attempt to apply a replacement override but with a different overridden
  // trial.
  EXPECT_FALSE(overrides->ApplyRuntimeOverride(
      pass_key, "Killswitch2", "Disabled2", /*overridden_trial=*/trial2,
      /*previous_override_trial_name=*/"Killswitch1"));

  // The previous override should still be present, and the new one should not
  // be added.
  EXPECT_EQ(overrides->GetRuntimeOverrides().size(), 1);
  auto override_info =
      FindOverride(overrides->GetRuntimeOverrides(), "Killswitch1");
  ASSERT_TRUE(override_info.has_value());
  EXPECT_EQ(override_info->trial_name, "Killswitch1");
  EXPECT_EQ(override_info->group_name, "Disabled1");
  EXPECT_EQ(override_info->overridden_trial, trial1);

  EXPECT_FALSE(FindOverride(overrides->GetRuntimeOverrides(), "Killswitch2")
                   .has_value());
  EXPECT_EQ(observer.call_count, 1);

  overrides->RemoveObserver(&observer);
}

TEST_F(RuntimeFieldTrialOverridesTest, ApplyFailsWhenTrialNameAlreadyExists) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("Trial2", "Group2");

  MockObserver observer;
  overrides->AddObserver(&observer);

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key, "Killswitch", "Disabled1", /*overridden_trial=*/trial1));
  EXPECT_EQ(observer.call_count, 1);

  // Attempt to apply another override with the same trial name but without
  // specifying it as the previous override (i.e. a collision).
  EXPECT_FALSE(overrides->ApplyRuntimeOverride(
      pass_key, "Killswitch", "Disabled2", /*overridden_trial=*/trial2));

  // The existing override should remain unmodified.
  EXPECT_EQ(overrides->GetRuntimeOverrides().size(), 1);
  auto override_info =
      FindOverride(overrides->GetRuntimeOverrides(), "Killswitch");
  ASSERT_TRUE(override_info.has_value());
  EXPECT_EQ(override_info->trial_name, "Killswitch");
  EXPECT_EQ(override_info->group_name, "Disabled1");
  EXPECT_EQ(override_info->overridden_trial, trial1);

  EXPECT_EQ(observer.call_count, 1);

  overrides->RemoveObserver(&observer);
}

TEST_F(RuntimeFieldTrialOverridesTest, GetRuntimeOverride) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  // Initially, looking up a trial should return std::nullopt.
  EXPECT_FALSE(overrides->GetRuntimeOverride("Trial").has_value());

  // Apply override.
  FieldTrial* trial =
      FieldTrialList::CreateFieldTrial("OriginalTrial", "OriginalGroup");
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(pass_key, "Trial", "Group",
                                              /*overridden_trial=*/trial));

  // It should now return the override info.
  auto override_info = overrides->GetRuntimeOverride("Trial");
  ASSERT_TRUE(override_info.has_value());
  EXPECT_EQ(override_info->trial_name, "Trial");
  EXPECT_EQ(override_info->group_name, "Group");
  EXPECT_EQ(override_info->overridden_trial, trial);

  // Looking up a different trial name should still return std::nullopt.
  EXPECT_FALSE(overrides->GetRuntimeOverride("OtherTrial").has_value());
}

TEST_F(RuntimeFieldTrialOverridesTest, IsFieldTrialOverridden) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("Trial2", "Group2");

  // Initially, neither trial is overridden.
  EXPECT_FALSE(overrides->IsFieldTrialOverridden(*trial1));
  EXPECT_FALSE(overrides->IsFieldTrialOverridden(*trial2));

  // Apply override for trial1.
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(pass_key, "OverrideTrial1",
                                              "Group1",
                                              /*overridden_trial=*/trial1));

  // trial1 should be overridden, trial2 should not.
  EXPECT_TRUE(overrides->IsFieldTrialOverridden(*trial1));
  EXPECT_FALSE(overrides->IsFieldTrialOverridden(*trial2));
}

}  // namespace base
