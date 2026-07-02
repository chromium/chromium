// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace indigo {

namespace {

// See `GenAIDefaultSettings.yaml`.
constexpr int kGenAiDefaultSettingsAllowed = 0;
constexpr int kGenAiDefaultSettingsAllowedWithoutModelImprovement = 1;
constexpr int kGenAiDefaultSettingsDisabled = 2;

class IndigoPolicyTest : public policy::PolicyTest {
 public:
  IndigoPolicyTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIndigo);
  }
  ~IndigoPolicyTest() override = default;

  Profile* profile() { return chrome_test_utils::GetProfile(this); }

  void SetPolicyCombination(std::optional<int> indigo_policy,
                            std::optional<int> gen_ai_policy) {
    policy::PolicyMap policies;
    if (indigo_policy.has_value()) {
      policies.Set(policy::key::kIndigo, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(indigo_policy.value()), nullptr);
    }
    if (gen_ai_policy.has_value()) {
      policies.Set(policy::key::kGenAiDefaultSettings,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(gen_ai_policy.value()), nullptr);
    }
    UpdateProviderPolicy(policies);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IndigoPolicyTest, PolicyEnabledWithModelImprovement) {
  SetPolicyCombination(prefs::Policy::kAllowed, std::nullopt);

  IndigoService* service = IndigoServiceFactory::GetForProfile(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_NE(service->GetLocalEligibility(),
            LocalEligibility::kDisabledByPolicy);
}

IN_PROC_BROWSER_TEST_F(IndigoPolicyTest, PolicyEnabledWithoutModelImprovement) {
  SetPolicyCombination(prefs::Policy::kAllowedWithoutModelImprovement,
                       std::nullopt);

  IndigoService* service = IndigoServiceFactory::GetForProfile(profile());
  ASSERT_NE(service, nullptr);
  // TODO(b:512247450): Change to EXPECT_NE once alternative disclaimer string
  // is ready.
  EXPECT_EQ(service->GetLocalEligibility(),
            LocalEligibility::kDisabledByPolicy);
}

IN_PROC_BROWSER_TEST_F(IndigoPolicyTest, PolicyDisabled) {
  SetPolicyCombination(prefs::Policy::kDisallowed, std::nullopt);

  IndigoService* service = IndigoServiceFactory::GetForProfile(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetLocalEligibility(),
            LocalEligibility::kDisabledByPolicy);
}

IN_PROC_BROWSER_TEST_F(IndigoPolicyTest, PolicyDefaultSettingsDisabled) {
  SetPolicyCombination(std::nullopt, kGenAiDefaultSettingsDisabled);

  IndigoService* service = IndigoServiceFactory::GetForProfile(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetLocalEligibility(),
            LocalEligibility::kDisabledByPolicy);
}

IN_PROC_BROWSER_TEST_F(IndigoPolicyTest, PolicyDefaultSettingsAllowed) {
  SetPolicyCombination(std::nullopt, kGenAiDefaultSettingsAllowed);

  IndigoService* service = IndigoServiceFactory::GetForProfile(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_NE(service->GetLocalEligibility(),
            LocalEligibility::kDisabledByPolicy);
}

IN_PROC_BROWSER_TEST_F(IndigoPolicyTest,
                       PolicyDefaultSettingsAllowedWithoutModelImprovement) {
  SetPolicyCombination(std::nullopt,
                       kGenAiDefaultSettingsAllowedWithoutModelImprovement);

  IndigoService* service = IndigoServiceFactory::GetForProfile(profile());
  ASSERT_NE(service, nullptr);
  // TODO(b:512247450): Change to EXPECT_NE once alternative disclaimer string
  // is ready.
  EXPECT_EQ(service->GetLocalEligibility(),
            LocalEligibility::kDisabledByPolicy);
}

IN_PROC_BROWSER_TEST_F(IndigoPolicyTest,
                       SpecificPolicyDisabledOverridesDefaultSettingsAllowed) {
  SetPolicyCombination(prefs::Policy::kDisallowed,
                       kGenAiDefaultSettingsAllowed);

  IndigoService* service = IndigoServiceFactory::GetForProfile(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetLocalEligibility(),
            LocalEligibility::kDisabledByPolicy);
}

IN_PROC_BROWSER_TEST_F(IndigoPolicyTest,
                       SpecificPolicyAllowedOverridesDefaultSettingsDisabled) {
  SetPolicyCombination(prefs::Policy::kAllowed, kGenAiDefaultSettingsDisabled);

  IndigoService* service = IndigoServiceFactory::GetForProfile(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_NE(service->GetLocalEligibility(),
            LocalEligibility::kDisabledByPolicy);
}

}  // namespace

}  // namespace indigo
