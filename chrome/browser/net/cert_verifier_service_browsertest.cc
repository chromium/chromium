// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/net/cert_verifier_configuration.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/buildflags.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_features.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
class CertVerifierServiceChromeRootStoreFeaturePolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<
          std::tuple<bool, absl::optional<bool>>> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kChromeRootStoreUsed, feature_use_chrome_root_store());

    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

#if BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
    auto policy_val = policy_use_chrome_root_store();
    if (policy_val.has_value()) {
      SetPolicyValue(*policy_val);
    }
#endif
  }

  void SetPolicyValue(bool value) {
    policy::PolicyMap policies;
#if BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
    SetPolicy(&policies, policy::key::kChromeRootStoreEnabled,
              base::Value(value));
#endif
    UpdateProviderPolicy(policies);
  }

  void ExpectUseChromeRootStoreCorrect(bool use_chrome_root_store) {
    {
      cert_verifier::mojom::CertVerifierServiceParamsPtr params =
          GetChromeCertVerifierServiceParams(/*local_state=*/nullptr);
      ASSERT_TRUE(params);
      EXPECT_EQ(use_chrome_root_store, params->use_chrome_root_store);
    }

    // Change the policy value, and then test the params returned by
    // GetChromeCertVerifierServiceParams do not change.
    SetPolicyValue(!use_chrome_root_store);
    {
      cert_verifier::mojom::CertVerifierServiceParamsPtr params =
          GetChromeCertVerifierServiceParams(/*local_state=*/nullptr);
      ASSERT_TRUE(params);
      EXPECT_EQ(use_chrome_root_store, params->use_chrome_root_store);
    }

    // Also test the params the actual CertVerifierServiceFactory was created
    // with, to ensure the values are being plumbed through properly.
    base::test::TestFuture<cert_verifier::mojom::CertVerifierServiceParamsPtr>
        service_params_future;
    content::GetCertVerifierServiceFactory()->GetServiceParamsForTesting(
        service_params_future.GetCallback());
    ASSERT_TRUE(service_params_future.Get());
    EXPECT_EQ(use_chrome_root_store,
              service_params_future.Get()->use_chrome_root_store);
  }

  bool feature_use_chrome_root_store() const { return std::get<0>(GetParam()); }

  absl::optional<bool> policy_use_chrome_root_store() const {
    return std::get<1>(GetParam());
  }

  bool expected_use_chrome_root_store() const {
    auto policy_val = policy_use_chrome_root_store();
    if (policy_val.has_value())
      return *policy_val;
    return feature_use_chrome_root_store();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(CertVerifierServiceChromeRootStoreFeaturePolicyTest,
                       Test) {
  ExpectUseChromeRootStoreCorrect(expected_use_chrome_root_store());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CertVerifierServiceChromeRootStoreFeaturePolicyTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(absl::nullopt
#if BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
                                         ,
                                         false,
                                         true
#endif
                                         )),
    [](const testing::TestParamInfo<
        CertVerifierServiceChromeRootStoreFeaturePolicyTest::ParamType>& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "FeatureTrue" : "FeatureFalse",
           std::get<1>(info.param).has_value()
               ? (*std::get<1>(info.param) ? "PolicyTrue" : "PolicyFalse")
               : "PolicyNotSet"});
    });
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

class CertVerifierServiceEnforceLocalAnchorConstraintsFeaturePolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<
          std::tuple<bool, absl::optional<bool>>> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kEnforceLocalAnchorConstraints,
        feature_enforce_local_anchor_constraints());

    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

    auto policy_val = policy_enforce_local_anchor_constraints();
    if (policy_val.has_value()) {
      SetPolicyValue(*policy_val);
    }
  }

  void SetPolicyValue(absl::optional<bool> value) {
    policy::PolicyMap policies;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    SetPolicy(&policies, policy::key::kEnforceLocalAnchorConstraintsEnabled,
              absl::optional<base::Value>(value));
#endif
    UpdateProviderPolicy(policies);
  }

  void ExpectEnforceLocalAnchorConstraintsCorrect(
      bool enforce_local_anchor_constraints) {
    EXPECT_EQ(enforce_local_anchor_constraints,
              net::IsLocalAnchorConstraintsEnforcementEnabled());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    // Set policy to the opposite value, and then test the value returned by
    // IsLocalAnchorConstraintsEnforcementEnabled has changed.
    SetPolicyValue(!enforce_local_anchor_constraints);
    EXPECT_EQ(!enforce_local_anchor_constraints,
              net::IsLocalAnchorConstraintsEnforcementEnabled());

    // Unset the policy, the value used should go back to the one set by the
    // feature flag.
    SetPolicyValue(absl::nullopt);
    EXPECT_EQ(feature_enforce_local_anchor_constraints(),
              net::IsLocalAnchorConstraintsEnforcementEnabled());
#endif
  }

  bool feature_enforce_local_anchor_constraints() const {
    return std::get<0>(GetParam());
  }

  absl::optional<bool> policy_enforce_local_anchor_constraints() const {
    return std::get<1>(GetParam());
  }

  bool expected_enforce_local_anchor_constraints() const {
    auto policy_val = policy_enforce_local_anchor_constraints();
    if (policy_val.has_value()) {
      return *policy_val;
    }
    return feature_enforce_local_anchor_constraints();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    CertVerifierServiceEnforceLocalAnchorConstraintsFeaturePolicyTest,
    Test) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(https://crbug.com/1410924): Avoid flake on android browser tests by
  // requiring the test to always take at least 1 second to finish. Remove this
  // delay once issue 1410924 is resolved.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
#endif
  ExpectEnforceLocalAnchorConstraintsCorrect(
      expected_enforce_local_anchor_constraints());
#if BUILDFLAG(IS_ANDROID)
  run_loop.Run();
#endif
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CertVerifierServiceEnforceLocalAnchorConstraintsFeaturePolicyTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(absl::nullopt
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
                                         ,
                                         false,
                                         true
#endif
                                         )),
    [](const testing::TestParamInfo<
        CertVerifierServiceEnforceLocalAnchorConstraintsFeaturePolicyTest::
            ParamType>& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "FeatureTrue" : "FeatureFalse",
           std::get<1>(info.param).has_value()
               ? (*std::get<1>(info.param) ? "PolicyTrue" : "PolicyFalse")
               : "PolicyNotSet"});
    });
