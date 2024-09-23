// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/memory_saver_policy_handler.h"

#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/policy_constants.h"

namespace performance_manager {

class MemorySaverPolicyHandlerTest
    : public policy::ConfigurationPolicyPrefStoreTest {
  void SetUp() override {
    handler_list_.AddHandler(std::make_unique<MemorySaverPolicyHandler>());
  }
};

TEST_F(MemorySaverPolicyHandlerTest, PolicyNotSet) {
  const base::Value* value_ptr = nullptr;
  policy::PolicyMap policy;
  UpdateProviderPolicy(policy);
  // When no policy is set, no value should be pushed to prefs.
  EXPECT_FALSE(store_->GetValue(
      performance_manager::user_tuning::prefs::kMemorySaverModeState,
      &value_ptr));
}

TEST_F(MemorySaverPolicyHandlerTest, PolicySetToFalseSetsPrefToDisabled) {
  const base::Value* value_ptr = nullptr;
  policy::PolicyMap policy;
  policy.Set(policy::key::kHighEfficiencyModeEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  UpdateProviderPolicy(policy);

  EXPECT_TRUE(store_->GetValue(
      performance_manager::user_tuning::prefs::kMemorySaverModeState,
      &value_ptr));
  EXPECT_EQ(value_ptr->GetInt(),
            static_cast<int>(performance_manager::user_tuning::prefs::
                                 MemorySaverModeState::kDisabled));
}

TEST_F(MemorySaverPolicyHandlerTest, PolicySetToTrueSetsPrefToEnabledOnTimer) {
  const base::Value* value_ptr = nullptr;
  policy::PolicyMap policy;
  policy.Set(policy::key::kHighEfficiencyModeEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  UpdateProviderPolicy(policy);

  EXPECT_TRUE(store_->GetValue(
      performance_manager::user_tuning::prefs::kMemorySaverModeState,
      &value_ptr));
  EXPECT_EQ(value_ptr->GetInt(),
            static_cast<int>(performance_manager::user_tuning::prefs::
                                 MemorySaverModeState::kEnabled));
}

}  // namespace performance_manager
