// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/local_network_access_ip_address_space_overrides_policy_handler.h"

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Testing error messages for
// policy::|LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler.
//
// Tests for setting the pref correctly are in
// components/policy/test/data/pref_mapping/LocalNetworkAccessIpAddressSpaceOverrides.json

TEST(LocalNetworkAccessIpAddressSpaceOverridesPolicyHandlerTest, ValidValues) {
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler handler(
      key::kLocalNetworkAccessIpAddressSpaceOverrides);
  PolicyMap policies;
  PolicyErrorMap errors;
  base::ListValue list;
  list.Append("100.64.0.0/10=public");
  list.Append("[2001:db8::]/32=local");
  list.Append("192.168.0.1:8000=public");
  list.Append("[2001:DB8::8:800:200C:417A]:8080=local");
  policies.Set(key::kLocalNetworkAccessIpAddressSpaceOverrides,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(list)), nullptr);

  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_EQ(0U, errors.size());
}

TEST(LocalNetworkAccessIpAddressSpaceOverridesPolicyHandlerTest,
     InvalidSingleValue) {
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler handler(
      key::kLocalNetworkAccessIpAddressSpaceOverrides);
  PolicyMap policies;
  PolicyErrorMap errors;
  base::ListValue list;
  list.Append("invalid-override");  // Invalid
  policies.Set(key::kLocalNetworkAccessIpAddressSpaceOverrides,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(list)), nullptr);
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_EQ(1U, errors.size());
  constexpr char16_t kExpected[] =
      u"Error at LocalNetworkAccessIpAddressSpaceOverrides[0]: Value doesn't "
      u"match format.";
  EXPECT_EQ(kExpected, errors.GetErrorMessages(
                           "LocalNetworkAccessIpAddressSpaceOverrides"));
}

TEST(LocalNetworkAccessIpAddressSpaceOverridesPolicyHandlerTest,
     SomeInvalidValues) {
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler handler(
      key::kLocalNetworkAccessIpAddressSpaceOverrides);
  PolicyMap policies;
  PolicyErrorMap errors;
  base::ListValue list;
  list.Append("invalid-override");          // Invalid
  list.Append("192.168.0.1:65536=public");  // Invalid (port)
  list.Append("100.64.0.0/10=public");      // Valid
  list.Append("100.64.0.0/33=public");      // Invalid (mask)
  list.Append("fc00::/7=public");           // Invalid (no brackets for IPv6)
  policies.Set(key::kLocalNetworkAccessIpAddressSpaceOverrides,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(list)), nullptr);
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_EQ(4U, errors.size());
  constexpr char16_t kExpected[] =
      u"Error at LocalNetworkAccessIpAddressSpaceOverrides[0]: Value doesn't "
      u"match format.\n"
      u"Error at LocalNetworkAccessIpAddressSpaceOverrides[1]: Value doesn't "
      u"match format.\n"
      u"Error at LocalNetworkAccessIpAddressSpaceOverrides[3]: Value doesn't "
      u"match format.\n"
      u"Error at LocalNetworkAccessIpAddressSpaceOverrides[4]: Value doesn't "
      u"match format.";
  EXPECT_EQ(kExpected, errors.GetErrorMessages(
                           "LocalNetworkAccessIpAddressSpaceOverrides"));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(LocalNetworkAccessIpAddressSpaceOverridesPolicyHandlerTest,
     DevicePolicyValidValues) {
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler handler(
      key::kDeviceLocalNetworkAccessIpAddressSpaceOverrides);
  PolicyMap policies;
  PolicyErrorMap errors;
  base::ListValue list;
  list.Append("100.64.0.0/10=public");
  list.Append("[2001:db8::]/32=local");
  policies.Set(key::kDeviceLocalNetworkAccessIpAddressSpaceOverrides,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
               POLICY_SOURCE_CLOUD, base::Value(std::move(list)), nullptr);

  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_EQ(0U, errors.size());
}

TEST(LocalNetworkAccessIpAddressSpaceOverridesPolicyHandlerTest,
     UserPolicyOverridesDevicePolicy) {
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler device_handler(
      key::kDeviceLocalNetworkAccessIpAddressSpaceOverrides);
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler user_handler(
      key::kLocalNetworkAccessIpAddressSpaceOverrides);
  PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::ListValue device_list;
  device_list.Append("100.64.0.0/10=public");
  policies.Set(key::kDeviceLocalNetworkAccessIpAddressSpaceOverrides,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
               POLICY_SOURCE_CLOUD, base::Value(std::move(device_list)),
               nullptr);

  base::ListValue user_list;
  user_list.Append("192.168.0.1:8000=public");
  policies.Set(key::kLocalNetworkAccessIpAddressSpaceOverrides,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(user_list)), nullptr);

  ASSERT_TRUE(device_handler.CheckPolicySettings(policies, &errors));
  device_handler.ApplyPolicySettings(policies, &prefs);
  ASSERT_TRUE(user_handler.CheckPolicySettings(policies, &errors));
  user_handler.ApplyPolicySettings(policies, &prefs);

  base::Value* result_value = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      prefs::kManagedLocalNetworkAccessIpAddressSpaceOverrides, &result_value));
  ASSERT_TRUE(result_value->is_list());
  EXPECT_EQ(1u, result_value->GetList().size());
  EXPECT_EQ("192.168.0.1:8000=public", result_value->GetList()[0].GetString());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace policy
