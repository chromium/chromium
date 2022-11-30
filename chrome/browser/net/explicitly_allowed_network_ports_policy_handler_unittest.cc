// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/explicitly_allowed_network_ports_policy_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "net/base/port_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ExplicitlyAllowedNetworkPortsPolicyHandlerTest : public testing::Test {
 protected:
  // Port 6000 is used by these tests as a port that can always be allowed.
  ExplicitlyAllowedNetworkPortsPolicyHandlerTest()
      : scoped_allowable_port_(6000) {}

  void SetPolicyValue(base::Value value) {
    policies_.Set(policy::key::kExplicitlyAllowedNetworkPorts,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, std::move(value), nullptr);
  }

  void CheckAndApplyPolicySettings() {
    if (handler_.CheckPolicySettings(policies_, &errors_)) {
      handler_.ApplyPolicySettings(policies_, &prefs_);
    }
  }

  PolicyErrorMap& errors() { return errors_; }
  bool has_error() {
    return errors_.HasError(policy::key::kExplicitlyAllowedNetworkPorts);
  }

  const base::Value* pref_value() {
    const base::Value* value = nullptr;
    if (!prefs_.GetValue(prefs::kExplicitlyAllowedNetworkPorts, &value))
      return nullptr;

    return value;
  }

 private:
  PolicyMap policies_;
  PolicyErrorMap errors_;
  PrefValueMap prefs_;
  ExplicitlyAllowedNetworkPortsPolicyHandler handler_;
  net::ScopedAllowablePortForTesting scoped_allowable_port_;
};

TEST_F(ExplicitlyAllowedNetworkPortsPolicyHandlerTest, Unset) {
  CheckAndApplyPolicySettings();
  EXPECT_TRUE(errors().empty());
  auto* value = pref_value();
  EXPECT_FALSE(value);
}

TEST_F(ExplicitlyAllowedNetworkPortsPolicyHandlerTest, Empty) {
  SetPolicyValue(base::Value(base::Value::Type::LIST));
  CheckAndApplyPolicySettings();
  EXPECT_TRUE(errors().empty());
  auto* value = pref_value();
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_list());
  EXPECT_TRUE(value->GetList().empty());
}

TEST_F(ExplicitlyAllowedNetworkPortsPolicyHandlerTest, Valid) {
  base::Value::List policy_value;
  policy_value.Append("6000");
  SetPolicyValue(base::Value(std::move(policy_value)));
  CheckAndApplyPolicySettings();
  EXPECT_TRUE(errors().empty());
  auto* value = pref_value();
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_list());
  ASSERT_EQ(value->GetList().size(), 1u);
  const auto& element = value->GetList()[0];
  EXPECT_TRUE(element.is_int());
  ASSERT_TRUE(element.GetIfInt());
  EXPECT_EQ(element.GetIfInt().value(), 6000);
}

TEST_F(ExplicitlyAllowedNetworkPortsPolicyHandlerTest, NotAList) {
  SetPolicyValue(base::Value(base::Value::Type::INTEGER));
  CheckAndApplyPolicySettings();
  EXPECT_TRUE(has_error());
  EXPECT_FALSE(pref_value());
}

// Non-string types are removed from the list, but the policy is still applied.
TEST_F(ExplicitlyAllowedNetworkPortsPolicyHandlerTest, MixedTypes) {
  base::Value::List policy_value;
  policy_value.Append(79);
  policy_value.Append("6000");
  SetPolicyValue(base::Value(std::move(policy_value)));
  CheckAndApplyPolicySettings();
  EXPECT_TRUE(has_error());
  auto* value = pref_value();
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_list());
  ASSERT_EQ(value->GetList().size(), 1u);
  const auto& element = value->GetList()[0];
  EXPECT_TRUE(element.is_int());
  ASSERT_TRUE(element.GetIfInt());
  EXPECT_EQ(element.GetIfInt().value(), 6000);
}

// Invalid strings are removed, but the policy is still applied.
TEST_F(ExplicitlyAllowedNetworkPortsPolicyHandlerTest, InvalidStrings) {
  const std::string kValues[] = {
      "-1",            // Too small.
      "100000",        // Too big.
      "smtp",          // Not a number.
      "25.0",          // Not an integer.
      "2E2",           // Not an integer.
      "",              // Not an integer.
      "1 1",           // Contains a space.
      "100000000000",  // Much too big.
      "\"514\"",       // Contains extra quotes.
      "6000",          // Valid.
  };
  base::Value::List policy_value;
  for (const auto& value : kValues) {
    policy_value.Append(value);
  }
  SetPolicyValue(base::Value(std::move(policy_value)));
  CheckAndApplyPolicySettings();
  EXPECT_TRUE(has_error());
  auto* value = pref_value();
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_list());
  ASSERT_EQ(value->GetList().size(), 1u);
  const auto& element = value->GetList()[0];
  EXPECT_TRUE(element.is_int());
  ASSERT_TRUE(element.GetIfInt());
  EXPECT_EQ(element.GetIfInt().value(), 6000);
}

}  // namespace policy
