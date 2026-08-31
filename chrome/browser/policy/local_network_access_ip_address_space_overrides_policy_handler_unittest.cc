// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/local_network_access_ip_address_space_overrides_policy_handler.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Testing error messages for
// policy::|LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler.
//
// Tests for setting the pref correctly are in
// components/policy/test/data/pref_mapping/LocalNetworkAccessIpAddressSpaceOverrides.json

TEST(LocalNetworkAccessIpAddressSpaceOverridesPolicyHandlerTest, ValidValues) {
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler handler;
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
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler handler;
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
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler handler;
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

}  // namespace policy
