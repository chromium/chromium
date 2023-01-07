// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/policy/local_sync_policy_handler.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/sync/base/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class LocalSyncPolicyTest : public testing::Test {
 protected:
  PolicyMap policy_;
  LocalSyncPolicyHandler handler_;
  PrefValueMap prefs_;
};

TEST_F(LocalSyncPolicyTest, Default) {
  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_FALSE(prefs_.GetValue(syncer::prefs::kLocalSyncBackendDir, nullptr));
}

// RoamingProfileLocation policy expects a string; give it a boolean.
TEST_F(LocalSyncPolicyTest, SetPolicyInvalid) {
  policy_.Set(key::kRoamingProfileLocation, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
              nullptr);
  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_FALSE(prefs_.GetValue(syncer::prefs::kLocalSyncBackendDir, nullptr));
}

// Use a variable in the value. It should be expanded by the handler.
TEST_F(LocalSyncPolicyTest, SetPolicyValid) {
  const std::string in = "${user_name}/foo";
  policy_.Set(key::kRoamingProfileLocation, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(in), nullptr);
  handler_.ApplyPolicySettings(policy_, &prefs_);

  const base::Value* value;
  ASSERT_TRUE(prefs_.GetValue(syncer::prefs::kLocalSyncBackendDir, &value));
  ASSERT_TRUE(value->is_string());
  const std::string& out = value->GetString();
  EXPECT_NE(std::string::npos, out.find("foo"));
  EXPECT_EQ(std::string::npos, out.find("${user_name}"));
}

}  // namespace policy
