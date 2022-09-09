// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/values.h"
#include "chrome/browser/net/disk_cache_dir_policy_handler.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DiskCacheDirPolicyTest : public testing::Test {
 protected:
  PolicyMap policy_;
  DiskCacheDirPolicyHandler handler_;
  PrefValueMap prefs_;
};

TEST_F(DiskCacheDirPolicyTest, Default) {
  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_FALSE(prefs_.GetValue(prefs::kDiskCacheDir, NULL));
}

TEST_F(DiskCacheDirPolicyTest, SetPolicyInvalid) {
  // DiskCacheDir policy expects a string; give it a boolean.
  policy_.Set(key::kDiskCacheDir, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_FALSE(prefs_.GetValue(prefs::kDiskCacheDir, NULL));
}

TEST_F(DiskCacheDirPolicyTest, SetPolicyValid) {
  // Use a variable in the value. It should be expanded by the handler.
  const std::string in = "${user_name}/foo";
  policy_.Set(key::kDiskCacheDir, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              POLICY_SOURCE_CLOUD, base::Value(in), nullptr);
  handler_.ApplyPolicySettings(policy_, &prefs_);

  const base::Value* value;
  ASSERT_TRUE(prefs_.GetValue(prefs::kDiskCacheDir, &value));
  ASSERT_TRUE(value->is_string());
  const std::string& out = value->GetString();
  EXPECT_NE(std::string::npos, out.find("foo"));
  EXPECT_EQ(std::string::npos, out.find("${user_name}"));
}

}  // namespace policy
