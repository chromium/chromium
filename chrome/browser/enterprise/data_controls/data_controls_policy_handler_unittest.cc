// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/data_controls_policy_handler.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace data_controls {

namespace {

constexpr char kTestPref[] = "data_controls.test_pref";

constexpr char kPolicyName[] = "PolicyForTesting";

constexpr char kSchema[] = R"(
      {
        "type": "object",
        "properties": {
          "PolicyForTesting": {
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "url": { "type": "string" },
                "incognito": { "type": "boolean" },
              }
            }
          }
        }
      })";

constexpr char kValidPolicy[] = R"(
  [
    {
      "url": "https://google.com",
      "incognito": true,
    },
    {
      "url": "https://foo.com",
      "incognito": false,
    }
  ]
)";

constexpr policy::PolicySource kCloudSources[] = {
    policy::PolicySource::POLICY_SOURCE_CLOUD,
    policy::PolicySource::POLICY_SOURCE_CLOUD_FROM_ASH};

constexpr policy::PolicySource kNonCloudSources[] = {
    policy::PolicySource::POLICY_SOURCE_ENTERPRISE_DEFAULT,
    policy::PolicySource::POLICY_SOURCE_COMMAND_LINE,
    policy::PolicySource::POLICY_SOURCE_ACTIVE_DIRECTORY,
    policy::PolicySource::POLICY_SOURCE_PLATFORM,
    policy::PolicySource::
        POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
};

constexpr char kInvalidPolicy[] = "[1,2,3]";

class DataControlsPolicyHandlerTest : public testing::Test {
 public:
  policy::Schema schema() {
    std::string error;
    policy::Schema validation_schema = policy::Schema::Parse(kSchema, &error);
    EXPECT_TRUE(error.empty());
    return validation_schema;
  }

  policy::PolicyMap CreatePolicyMap(const std::string& policy,
                                    policy::PolicySource policy_source) {
    policy::PolicyMap policy_map;

    policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                   policy::PolicyScope::POLICY_SCOPE_MACHINE, policy_source,
                   policy_value(policy), nullptr);

    return policy_map;
  }

  absl::optional<base::Value> policy_value(const std::string& policy) const {
    return base::JSONReader::Read(policy, base::JSON_ALLOW_TRAILING_COMMAS);
  }
};

}  // namespace

TEST_F(DataControlsPolicyHandlerTest, AllowsCloudSources) {
  for (auto scope : kCloudSources) {
    policy::PolicyMap map = CreatePolicyMap(kValidPolicy, scope);
    auto handler = std::make_unique<DataControlsPolicyHandler>(
        kPolicyName, kTestPref, schema());

    policy::PolicyErrorMap errors;
    ASSERT_TRUE(handler->CheckPolicySettings(map, &errors));
    ASSERT_TRUE(errors.empty());

    PrefValueMap prefs;
    base::Value* value_set_in_pref;
    handler->ApplyPolicySettings(map, &prefs);

    ASSERT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));

    auto* value_set_in_map = map.GetValueUnsafe(kPolicyName);
    ASSERT_TRUE(value_set_in_map);
    ASSERT_EQ(*value_set_in_map, *value_set_in_pref);
  }
}

TEST_F(DataControlsPolicyHandlerTest, BlocksNonCloudSources) {
  for (auto scope : kNonCloudSources) {
    policy::PolicyMap map = CreatePolicyMap(kValidPolicy, scope);
    auto handler = std::make_unique<DataControlsPolicyHandler>(
        kPolicyName, kTestPref, schema());

    policy::PolicyErrorMap errors;
    ASSERT_FALSE(handler->CheckPolicySettings(map, &errors));
    ASSERT_FALSE(errors.empty());
    ASSERT_TRUE(errors.HasError(kPolicyName));
    std::u16string messages = errors.GetErrorMessages(kPolicyName);
    ASSERT_EQ(messages,
              u"Ignored because the policy is not set by a cloud source.");
  }
}

TEST_F(DataControlsPolicyHandlerTest, BlocksInvalidPolicy) {
  for (auto scope : kCloudSources) {
    policy::PolicyMap map = CreatePolicyMap(kInvalidPolicy, scope);
    auto handler = std::make_unique<DataControlsPolicyHandler>(
        kPolicyName, kTestPref, schema());

    policy::PolicyErrorMap errors;
    ASSERT_FALSE(handler->CheckPolicySettings(map, &errors));
    ASSERT_FALSE(errors.empty());
    ASSERT_TRUE(errors.HasError(kPolicyName));
    std::u16string messages = errors.GetErrorMessages(kPolicyName);
    ASSERT_EQ(messages,
              u"Error at PolicyForTesting[0]: Schema validation error: Policy "
              u"type mismatch: expected: \"dictionary\", actual: \"integer\".");
  }
}

}  // namespace data_controls
