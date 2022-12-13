// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_policy_handler.h"

#include "chrome/browser/extensions/policy_handlers.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

const char kTestPref[] = "unit_test.test_pref";

TEST(NativeMessagingHostListPolicyHandlerTest, CheckPolicySettings) {
  base::Value::List list;
  policy::PolicyMap policy_map;
  NativeMessagingHostListPolicyHandler handler(
      policy::key::kNativeMessagingBlocklist, kTestPref, true);

  policy_map.Set(policy::key::kNativeMessagingBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  {
    policy::PolicyErrorMap errors;
    EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
    EXPECT_TRUE(errors.empty());
  }

  list.Append("test.a.b");
  policy_map.Set(policy::key::kNativeMessagingBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  {
    policy::PolicyErrorMap errors;
    EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
    EXPECT_TRUE(errors.empty());
  }

  list.Append("*");
  policy_map.Set(policy::key::kNativeMessagingBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  {
    policy::PolicyErrorMap errors;
    EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
    EXPECT_TRUE(errors.empty());
  }

  list.Append("invalid Name");
  policy_map.Set(policy::key::kNativeMessagingBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  {
    policy::PolicyErrorMap errors;
    EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
    EXPECT_FALSE(errors.empty());
    EXPECT_FALSE(
        errors.GetErrors(policy::key::kNativeMessagingBlocklist).empty());
  }
}

TEST(NativeMessagingHostListPolicyHandlerTest, ApplyPolicySettings) {
  base::Value::List policy;
  base::Value::List expected;
  policy::PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = nullptr;
  NativeMessagingHostListPolicyHandler handler(
      policy::key::kNativeMessagingBlocklist, kTestPref, true);

  policy.Append("com.example.test");
  expected.Append("com.example.test");

  policy_map.Set(policy::key::kNativeMessagingBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(policy.Clone()),
                 nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, value->GetList());

  policy.Append("*");
  expected.Append("*");

  policy_map.Set(policy::key::kNativeMessagingBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(policy.Clone()),
                 nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, value->GetList());

  policy.Append("invalid Name");
  policy_map.Set(policy::key::kNativeMessagingBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(policy.Clone()),
                 nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, value->GetList());
}

}  // namespace extensions
