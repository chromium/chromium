// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "net/net_buildflags.h"

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED) || \
    BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "base/strings/strcat.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_cert_verifier_service_factory.h"
#include "net/base/features.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif  // BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED) ||
        // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
class CertVerifierServiceCertVerifierBuiltinFeaturePolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<
          std::tuple<bool, absl::optional<bool>>> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kCertVerifierBuiltinFeature,
        feature_use_builtin_cert_verifier());

    content::SetCertVerifierServiceFactoryForTesting(
        &test_cert_verifier_service_factory_);

    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
    auto policy_val = policy_use_builtin_cert_verifier();
    if (policy_val.has_value()) {
      policy::PolicyMap policies;
      SetPolicy(&policies, policy::key::kBuiltinCertificateVerifierEnabled,
                base::Value(*policy_val));
      UpdateProviderPolicy(policies);
    }
#endif
  }

  void TearDownInProcessBrowserTestFixture() override {
    content::SetCertVerifierServiceFactoryForTesting(nullptr);
  }

  void ExpectUseBuiltinCertVerifierCorrect(
      cert_verifier::mojom::CertVerifierCreationParams::CertVerifierImpl
          use_builtin_cert_verifier) {
    ASSERT_LE(1ul, test_cert_verifier_service_factory_.num_captured_params());
    for (size_t i = 0;
         i < test_cert_verifier_service_factory_.num_captured_params(); i++) {
      SCOPED_TRACE(i);
      ASSERT_TRUE(test_cert_verifier_service_factory_.GetParamsAtIndex(i)
                      ->creation_params);
      EXPECT_EQ(use_builtin_cert_verifier,
                test_cert_verifier_service_factory_.GetParamsAtIndex(i)
                    ->creation_params->use_builtin_cert_verifier);
    }

    // Send them to the actual CertVerifierServiceFactory.
    test_cert_verifier_service_factory_.ReleaseAllCertVerifierParams();
  }

  bool feature_use_builtin_cert_verifier() const {
    return std::get<0>(GetParam());
  }

  absl::optional<bool> policy_use_builtin_cert_verifier() const {
    return std::get<1>(GetParam());
  }

  bool expected_use_builtin_cert_verifier() const {
    auto policy_val = policy_use_builtin_cert_verifier();
    if (policy_val.has_value())
      return *policy_val;
    return feature_use_builtin_cert_verifier();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  cert_verifier::TestCertVerifierServiceFactoryImpl
      test_cert_verifier_service_factory_;
};

IN_PROC_BROWSER_TEST_P(CertVerifierServiceCertVerifierBuiltinFeaturePolicyTest,
                       Test) {
  ExpectUseBuiltinCertVerifierCorrect(
      expected_use_builtin_cert_verifier()
          ? cert_verifier::mojom::CertVerifierCreationParams::CertVerifierImpl::
                kBuiltin
          : cert_verifier::mojom::CertVerifierCreationParams::CertVerifierImpl::
                kSystem);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CertVerifierServiceCertVerifierBuiltinFeaturePolicyTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(absl::nullopt
#if BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
                                         ,
                                         false,
                                         true
#endif
                                         )),
    [](const testing::TestParamInfo<
        CertVerifierServiceCertVerifierBuiltinFeaturePolicyTest::ParamType>&
           info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "FeatureTrue" : "FeatureFalse",
           std::get<1>(info.param).has_value()
               ? (*std::get<1>(info.param) ? "PolicyTrue" : "PolicyFalse")
               : "PolicyNotSet"});
    });
#endif  // BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
class CertVerifierServiceChromeRootStoreFeaturePolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<
          std::tuple<bool, absl::optional<bool>>> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kChromeRootStoreUsed, feature_use_chrome_root_store());

    content::SetCertVerifierServiceFactoryForTesting(
        &test_cert_verifier_service_factory_);

    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

#if BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
    auto policy_val = policy_use_chrome_root_store();
    if (policy_val.has_value()) {
      policy::PolicyMap policies;
      SetPolicy(&policies, policy::key::kChromeRootStoreEnabled,
                base::Value(*policy_val));
      UpdateProviderPolicy(policies);
    }
#endif
  }

  void TearDownInProcessBrowserTestFixture() override {
    content::SetCertVerifierServiceFactoryForTesting(nullptr);
  }

  void ExpectUseChromeRootStoreCorrect(
      cert_verifier::mojom::CertVerifierCreationParams::ChromeRootImpl
          use_chrome_root_store) {
    ASSERT_LE(1ul, test_cert_verifier_service_factory_.num_captured_params());
    for (size_t i = 0;
         i < test_cert_verifier_service_factory_.num_captured_params(); i++) {
      SCOPED_TRACE(i);
      ASSERT_TRUE(test_cert_verifier_service_factory_.GetParamsAtIndex(i)
                      ->creation_params);
      EXPECT_EQ(use_chrome_root_store,
                test_cert_verifier_service_factory_.GetParamsAtIndex(i)
                    ->creation_params->use_chrome_root_store);
    }

    // Send them to the actual CertVerifierServiceFactory.
    test_cert_verifier_service_factory_.ReleaseAllCertVerifierParams();
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

  cert_verifier::TestCertVerifierServiceFactoryImpl
      test_cert_verifier_service_factory_;
};

IN_PROC_BROWSER_TEST_P(CertVerifierServiceChromeRootStoreFeaturePolicyTest,
                       Test) {
  ExpectUseChromeRootStoreCorrect(
      expected_use_chrome_root_store()
          ? cert_verifier::mojom::CertVerifierCreationParams::ChromeRootImpl::
                kRootChrome
          : cert_verifier::mojom::CertVerifierCreationParams::ChromeRootImpl::
                kRootSystem);
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
