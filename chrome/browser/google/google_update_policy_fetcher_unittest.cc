// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_policy_fetcher.h"

#include <string_view>

#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr std::string_view kTestAppId = "com.google.Chrome";

constexpr std::string_view kTestJson = R"({
  "policiesByName": {
    "LastCheckPeriod": {
      "prevailingSource": "Device Management",
      "valuesBySource": {
        "Device Management": 120
      }
    },
    "ProxyMode": {
      "prevailingSource": "Managed Preferences",
      "valuesBySource": {
        "Managed Preferences": "fixed_servers"
      }
    },
    "UpdatesSuppressed": {
      "prevailingSource": "Device Management",
      "valuesBySource": {
        "Device Management": {
          "StartHour": 1,
          "StartMinute": 30,
          "Duration": 60
        }
      }
    }
  },
  "policiesByAppId": {
    "com.google.Chrome": {
      "Update": {
        "prevailingSource": "Device Management",
        "valuesBySource": {
          "Device Management": 1
        }
      }
    }
  }
})";

constexpr std::string_view kJsonWithUnknownSource = R"({
  "policiesByName": {
    "LastCheckPeriod": {
      "prevailingSource": "Magic Source",
      "valuesBySource": {
        "Magic Source": 120
      }
    }
  }
})";

}  // namespace

TEST(GoogleUpdatePolicyFetcherTest, ParsePoliciesJsonIntoPolicyMap) {
  policy::PolicyMap policies;
  ParsePoliciesJsonIntoPolicyMap(kTestJson, kTestAppId, &policies);

  // Check global policy.
  const policy::PolicyMap::Entry* last_check =
      policies.Get(kAutoUpdateCheckPeriodMinutes);
  ASSERT_TRUE(last_check);
  EXPECT_EQ(last_check->value(base::Value::Type::INTEGER)->GetInt(), 120);
  EXPECT_EQ(last_check->source, policy::POLICY_SOURCE_CLOUD);

  const policy::PolicyMap::Entry* proxy_mode = policies.Get(kProxyMode);
  ASSERT_TRUE(proxy_mode);
  EXPECT_EQ(proxy_mode->value(base::Value::Type::STRING)->GetString(),
            "fixed_servers");
  EXPECT_EQ(proxy_mode->source, policy::POLICY_SOURCE_PLATFORM);

  // Check composite policy (UpdatesSuppressed).
  const policy::PolicyMap::Entry* start_hour =
      policies.Get(kUpdatesSuppressedStartHour);
  ASSERT_TRUE(start_hour);
  EXPECT_EQ(start_hour->value(base::Value::Type::INTEGER)->GetInt(), 1);

  const policy::PolicyMap::Entry* duration =
      policies.Get(kUpdatesSuppressedDurationMin);
  ASSERT_TRUE(duration);
  EXPECT_EQ(duration->value(base::Value::Type::INTEGER)->GetInt(), 60);

  // Check app policy.
  const policy::PolicyMap::Entry* update_policy = policies.Get(kUpdatePolicy);
  ASSERT_TRUE(update_policy);
  EXPECT_EQ(update_policy->value(base::Value::Type::INTEGER)->GetInt(), 1);
  EXPECT_EQ(update_policy->source, policy::POLICY_SOURCE_CLOUD);
}

TEST(GoogleUpdatePolicyFetcherTest, ParseEmptyJson) {
  policy::PolicyMap policies;
  ParsePoliciesJsonIntoPolicyMap("", kTestAppId, &policies);
  EXPECT_TRUE(policies.empty());
}

TEST(GoogleUpdatePolicyFetcherTest, ParseInvalidJson) {
  policy::PolicyMap policies;
  ParsePoliciesJsonIntoPolicyMap("{invalid", kTestAppId, &policies);
  EXPECT_TRUE(policies.empty());
}

TEST(GoogleUpdatePolicyFetcherTest, ParsePoliciesJsonWithUnknownSource) {
  policy::PolicyMap policies;
  ParsePoliciesJsonIntoPolicyMap(kJsonWithUnknownSource, kTestAppId, &policies);

  // The policy should be ignored because "Magic Source" is unknown.
  EXPECT_TRUE(policies.empty());
}

TEST(GoogleUpdatePolicyFetcherTest, ParsePoliciesJsonCaseInsensitiveAppId) {
  constexpr std::string_view kJsonWithUppercaseGuid = R"({
    "policiesByAppId": {
      "{8A69D345-D564-463C-AFF1-A69D9E530F96}": {
        "Update": {
          "prevailingSource": "Device Management",
          "valuesBySource": {
            "Device Management": 1
          }
        },
        "MajorVersionRolloutPolicy": {
          "prevailingSource": "Device Management",
          "valuesBySource": {
            "Device Management": 2
          }
        },
        "MinorVersionRolloutPolicy": {
          "prevailingSource": "Device Management",
          "valuesBySource": {
            "Device Management": 1
          }
        }
      }
    }
  })";

  policy::PolicyMap policies;
  // Look up using lowercase 'c' GUID.
  ParsePoliciesJsonIntoPolicyMap(
      kJsonWithUppercaseGuid, "{8A69D345-D564-463c-AFF1-A69D9E530F96}", &policies);

  const policy::PolicyMap::Entry* update_policy = policies.Get(kUpdatePolicy);
  ASSERT_TRUE(update_policy);
  EXPECT_EQ(update_policy->value(base::Value::Type::INTEGER)->GetInt(), 1);

  const policy::PolicyMap::Entry* major_rollout =
      policies.Get(kMajorVersionRolloutPolicy);
  ASSERT_TRUE(major_rollout);
  EXPECT_EQ(major_rollout->value(base::Value::Type::INTEGER)->GetInt(), 2);
  EXPECT_EQ(major_rollout->source, policy::POLICY_SOURCE_CLOUD);

  const policy::PolicyMap::Entry* minor_rollout =
      policies.Get(kMinorVersionRolloutPolicy);
  ASSERT_TRUE(minor_rollout);
  EXPECT_EQ(minor_rollout->value(base::Value::Type::INTEGER)->GetInt(), 1);
  EXPECT_EQ(minor_rollout->source, policy::POLICY_SOURCE_CLOUD);
}
