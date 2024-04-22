// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/annotations/annotation_control.h"

#include "base/test/gtest_util.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

constexpr char kTestBooleanPolicy1[] = "Policy1";
constexpr char kTestBooleanPolicy2[] = "Policy2";

// Helper function for setting policies in PolicyMap.
void SetPolicy(PolicyMap* policies,
               const char* key,
               std::optional<base::Value> value) {
  policies->Set(key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_CLOUD, std::move(value), nullptr);
}

TEST(AnnotationControlTest, IsBlockedByPolicies_ReturnsFalseByDefault) {
  EXPECT_FALSE(AnnotationControl().IsBlockedByPolicies(PolicyMap()));
}

TEST(AnnotationControlTest, IsBlockedByPolicies_ReturnsFalseWithoutPolicies) {
  AnnotationControl control =
      AnnotationControl().Add(kTestBooleanPolicy1, base::Value(false));
  EXPECT_FALSE(control.IsBlockedByPolicies(PolicyMap()));
}

TEST(AnnotationControlTest,
     IsBlockedByPolicies_ReturnsFalseWhenBooleanPolicyMismatches) {
  AnnotationControl control =
      AnnotationControl().Add(kTestBooleanPolicy1, base::Value(false));

  PolicyMap policies = PolicyMap();
  SetPolicy(&policies, kTestBooleanPolicy1, base::Value(true));

  EXPECT_FALSE(control.IsBlockedByPolicies(policies));
}

TEST(AnnotationControlTest,
     IsBlockedByPolicies_ReturnsTrueWhenBooleanPolicyMatches) {
  AnnotationControl control =
      AnnotationControl().Add(kTestBooleanPolicy1, base::Value(false));

  PolicyMap policies = PolicyMap();
  SetPolicy(&policies, kTestBooleanPolicy1, base::Value(false));
  EXPECT_TRUE(control.IsBlockedByPolicies(policies));
}

TEST(AnnotationControlTest,
     IsBlockedByPolicies_ReturnsFalseWhenBooleanPoliciesMismatch) {
  AnnotationControl control = AnnotationControl()
                                  .Add(kTestBooleanPolicy1, base::Value(false))
                                  .Add(kTestBooleanPolicy2, base::Value(false));

  PolicyMap policies = PolicyMap();
  SetPolicy(&policies, kTestBooleanPolicy1, base::Value(false));
  SetPolicy(&policies, kTestBooleanPolicy2, base::Value(true));
  EXPECT_FALSE(control.IsBlockedByPolicies(policies));
}

TEST(AnnotationControlTest,
     IsBlockedByPolicies_ReturnsFalseWhenBooleanPoliciesMatch) {
  AnnotationControl control = AnnotationControl()
                                  .Add(kTestBooleanPolicy1, base::Value(false))
                                  .Add(kTestBooleanPolicy2, base::Value(false));

  PolicyMap policies = PolicyMap();
  SetPolicy(&policies, kTestBooleanPolicy1, base::Value(false));
  SetPolicy(&policies, kTestBooleanPolicy2, base::Value(false));
  EXPECT_TRUE(control.IsBlockedByPolicies(policies));
}

TEST(AnnotationControlTest, Add_RejectsNonBooleanPolicies) {
  EXPECT_CHECK_DEATH(
      AnnotationControl().Add(kTestBooleanPolicy1, base::Value(5)));
}

}  // namespace policy
