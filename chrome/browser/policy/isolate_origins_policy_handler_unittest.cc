// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/isolate_origins_policy_handler.h"

#include <memory>

#include "base/byte_size.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/site_isolation/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class IsolateOriginsPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicy(const std::string& policy_name, base::Value value) {
    policies_.Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, std::move(value), nullptr);
  }

  void ApplyPolicies() { handler_.ApplyPolicySettings(policies_, &prefs_); }

  IsolateOriginsPolicyHandler handler_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
};

#if BUILDFLAG(IS_ANDROID)

TEST_F(IsolateOriginsPolicyHandlerTest, ApplyPolicySettings_AndroidLegacy) {
  SetPolicy(key::kIsolateOriginsAndroid, base::Value("https://example.com"));
  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kIsolateOrigins, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(value->GetString(), "https://example.com");
}

TEST_F(IsolateOriginsPolicyHandlerTest, ApplyPolicySettings_AndroidIgnoreDesktop) {
  SetPolicy(key::kIsolateOrigins, base::Value("https://example.com"));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(prefs::kIsolateOrigins, nullptr));
}

TEST_F(IsolateOriginsPolicyHandlerTest, CheckPolicySettings_ValidAndroid) {
  SetPolicy(key::kIsolateOriginsAndroid, base::Value("https://example.com"));
  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());
}

TEST_F(IsolateOriginsPolicyHandlerTest, CheckPolicySettings_InvalidAndroid) {
  SetPolicy(key::kIsolateOriginsAndroid, base::Value(123));
  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_FALSE(errors_.empty());
}

#else  // BUILDFLAG(IS_ANDROID)

TEST_F(IsolateOriginsPolicyHandlerTest, ApplyPolicySettings_Desktop) {
  SetPolicy(key::kIsolateOrigins, base::Value("https://example.com"));
  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kIsolateOrigins, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(value->GetString(), "https://example.com");
}

TEST_F(IsolateOriginsPolicyHandlerTest, ApplyPolicySettings_DesktopIgnoreAndroid) {
  SetPolicy("IsolateOriginsAndroid", base::Value("https://example.com"));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(prefs::kIsolateOrigins, nullptr));
}

TEST_F(IsolateOriginsPolicyHandlerTest, CheckPolicySettings_ValidDesktop) {
  SetPolicy(key::kIsolateOrigins, base::Value("https://example.com"));
  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());
}

TEST_F(IsolateOriginsPolicyHandlerTest, CheckPolicySettings_InvalidDesktop) {
  SetPolicy(key::kIsolateOrigins, base::Value(123));
  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_FALSE(errors_.empty());
}

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)

class IsolateOriginsPolicyHandlerShortlistTest
    : public IsolateOriginsPolicyHandlerTest {
 public:
  IsolateOriginsPolicyHandlerShortlistTest() {
    feature_list_.InitAndEnableFeature(
        site_isolation::features::kIsolateOriginsShortlist);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// On a high-end device (> 3.2GB RAM), standard IsolateOrigins should be used.
TEST_F(IsolateOriginsPolicyHandlerShortlistTest, HighEndDevice_UseStandard) {
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::MiB(4000));  // 4GB
  SetPolicy(key::kIsolateOrigins, base::Value("https://domain-a.com"));
  SetPolicy(key::kIsolateOriginsShortlist, base::Value("https://domain-b.com"));
  SetPolicy(key::kIsolateOriginsAndroid, base::Value("https://domain-c.com"));
  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kIsolateOrigins, &value));
  EXPECT_EQ(value->GetString(), "https://domain-a.com");
}

// On a high-end device, shortlist should be ignored if standard is not set.
TEST_F(IsolateOriginsPolicyHandlerShortlistTest,
       HighEndDevice_IgnoreShortlist) {
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::MiB(4000));  // 4GB
  SetPolicy(key::kIsolateOriginsShortlist, base::Value("https://example.com"));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(prefs::kIsolateOrigins, nullptr));
}

// On a low-end device (<= 3.2GB RAM), IsolateOriginsShortlist should be used.
TEST_F(IsolateOriginsPolicyHandlerShortlistTest, LowEndDevice_UseShortlist) {
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::MiB(2000));  // 2GB
  SetPolicy(key::kIsolateOrigins, base::Value("https://domain-a.com"));
  SetPolicy(key::kIsolateOriginsShortlist, base::Value("https://domain-b.com"));
  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kIsolateOrigins, &value));
  EXPECT_EQ(value->GetString(), "https://domain-b.com");
}

// Exactly at the memory threshold boundary (3200MB), shortlist should be used.
TEST_F(IsolateOriginsPolicyHandlerShortlistTest, ExactlyThreshold_UseShortlist) {
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::MiB(3200));  // 3200MB (3.2GB)
  SetPolicy(key::kIsolateOrigins, base::Value("https://domain-a.com"));
  SetPolicy(key::kIsolateOriginsShortlist, base::Value("https://domain-b.com"));
  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kIsolateOrigins, &value));
  EXPECT_EQ(value->GetString(), "https://domain-b.com");
}

// On a low-end device, standard should be ignored.
TEST_F(IsolateOriginsPolicyHandlerShortlistTest, LowEndDevice_IgnoreStandard) {
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::MiB(2000));  // 2GB
  SetPolicy(key::kIsolateOrigins, base::Value("https://example.com"));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(prefs::kIsolateOrigins, nullptr));
}

// On a low-end Android device, if shortlist is not set, it should fall back to
// the deprecated IsolateOriginsAndroid policy.
TEST_F(IsolateOriginsPolicyHandlerShortlistTest,
       LowEndAndroid_FallbackToLegacy) {
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::MiB(2000));  // 2GB
  SetPolicy(key::kIsolateOriginsAndroid, base::Value("https://example.com"));
  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kIsolateOrigins, &value));
  EXPECT_EQ(value->GetString(), "https://example.com");
}

// On a low-end Android device, shortlist takes precedence over legacy.
TEST_F(IsolateOriginsPolicyHandlerShortlistTest,
       LowEndAndroid_ShortlistPrecedenceOverLegacy) {
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::MiB(2000));  // 2GB
  SetPolicy(key::kIsolateOriginsShortlist, base::Value("https://domain-a.com"));
  SetPolicy(key::kIsolateOriginsAndroid, base::Value("https://domain-b.com"));
  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kIsolateOrigins, &value));
  EXPECT_EQ(value->GetString(), "https://domain-a.com");
}

// On a high-end Android device, if standard is not set, it should fall back to
// the deprecated IsolateOriginsAndroid policy.
TEST_F(IsolateOriginsPolicyHandlerShortlistTest,
       HighEndAndroid_FallbackToLegacy) {
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::MiB(4000));  // 4GB
  SetPolicy(key::kIsolateOriginsAndroid, base::Value("https://example.com"));
  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kIsolateOrigins, &value));
  EXPECT_EQ(value->GetString(), "https://example.com");
}

TEST_F(IsolateOriginsPolicyHandlerShortlistTest, CheckPolicySettings_Valid) {
  SetPolicy(key::kIsolateOrigins, base::Value("https://domain-a.com"));
  SetPolicy(key::kIsolateOriginsShortlist, base::Value("https://domain-b.com"));
  SetPolicy(key::kIsolateOriginsAndroid, base::Value("https://domain-c.com"));
  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());
}

TEST_F(IsolateOriginsPolicyHandlerShortlistTest,
       CheckPolicySettings_InvalidIsolateOrigins) {
  SetPolicy(key::kIsolateOrigins, base::Value(123));
  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_FALSE(errors_.empty());
}

TEST_F(IsolateOriginsPolicyHandlerShortlistTest,
       CheckPolicySettings_InvalidIsolateOriginsShortlist) {
  SetPolicy(key::kIsolateOriginsShortlist, base::Value(123));
  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_FALSE(errors_.empty());
}

TEST_F(IsolateOriginsPolicyHandlerShortlistTest,
       CheckPolicySettings_InvalidIsolateOriginsAndroid) {
  SetPolicy(key::kIsolateOriginsAndroid, base::Value(123));
  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_FALSE(errors_.empty());
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace policy
