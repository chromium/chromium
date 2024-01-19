// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/buildflags.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/internal/trust_store_features.h"
#include "net/cert/x509_util.h"
#include "net/net_buildflags.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX)
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util_nss.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

#if BUILDFLAG(CHROME_CERTIFICATE_POLICIES_SUPPORTED)
// Testing the CACertificates policy
class CertVerifierServiceCACertificatesPolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

    if (add_cert_to_policy()) {
      scoped_refptr<net::X509Certificate> root_cert = net::ImportCertFromFile(
          net::EmbeddedTestServer::GetRootCertPemPath());
      ASSERT_TRUE(root_cert);

      std::string b64_cert = base::Base64Encode(
          net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer()));
      base::Value certs_value(base::Value::Type::LIST);
      certs_value.GetList().Append(b64_cert);
      policy::PolicyMap policies;
      SetPolicy(&policies, policy::key::kCACertificates,
                std::make_optional(std::move(certs_value)));
      UpdateProviderPolicy(policies);
    }
  }

  bool add_cert_to_policy() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(CertVerifierServiceCACertificatesPolicyTest,
                       TestCACertificatesPolicy) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_AUTO);
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store + policies.
  net::TestRootCerts::GetInstance()->Clear();

  ASSERT_TRUE(NavigateToUrl(https_test_server.GetURL("/simple.html"), this));
  EXPECT_NE(add_cert_to_policy(),
            chrome_browser_interstitials::IsShowingInterstitial(
                chrome_test_utils::GetActiveWebContents(this)));
}

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifierServiceCACertificatesPolicyTest,
                         ::testing::Bool());

// Testing the CADistrutedCertificates policy
class CertVerifierServiceCADistrustedCertificatesPolicyTest
    : public policy::PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);

    std::string b64_cert = base::Base64Encode(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer()));
    base::Value certs_value(base::Value::Type::LIST);
    certs_value.GetList().Append(b64_cert);
    policy::PolicyMap policies;
    // Distrust the test server certificate
    SetPolicy(&policies, policy::key::kCADistrustedCertificates,
              std::make_optional(std::move(certs_value)));
    UpdateProviderPolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(CertVerifierServiceCADistrustedCertificatesPolicyTest,
                       TestPolicy) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_AUTO);
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  // We don't clear the test roots but the cert should still be distrusted based
  // on the enterprise policy.

  ASSERT_TRUE(NavigateToUrl(https_test_server.GetURL("/simple.html"), this));

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      chrome_test_utils::GetActiveWebContents(this)));
}

class CertVerifierServiceCATrustedDistrustedCertificatesPolicyTest
    : public policy::PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);

    std::string b64_cert = base::Base64Encode(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer()));
    policy::PolicyMap policies;
    // Distrust the test server certificate
    {
      base::Value certs_value(base::Value::Type::LIST);
      certs_value.GetList().Append(b64_cert);
      SetPolicy(&policies, policy::key::kCADistrustedCertificates,
                std::make_optional(std::move(certs_value)));
    }
    // Trust the test server certificate
    {
      base::Value certs_value(base::Value::Type::LIST);
      certs_value.GetList().Append(b64_cert);
      SetPolicy(&policies, policy::key::kCACertificates,
                std::make_optional(std::move(certs_value)));
    }
    UpdateProviderPolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(
    CertVerifierServiceCATrustedDistrustedCertificatesPolicyTest,
    TestDistrustOverridesTrust) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_AUTO);
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  // We don't clear the test roots but the cert should still be distrusted based
  // on the enterprise policy.

  ASSERT_TRUE(NavigateToUrl(https_test_server.GetURL("/simple.html"), this));

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      chrome_test_utils::GetActiveWebContents(this)));
}

// Testing the CAHintCertificate policy
class CertVerifierServiceCAHintCertificatesPolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

    // Don't serve the intermediate either via AIA or as part of the handshake.
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.intermediate =
        net::EmbeddedTestServer::IntermediateType::kMissing;
    https_test_server_.SetSSLConfig(cert_config);
    https_test_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_test_server_.Start());

    if (add_cert_to_policy()) {
      // Add the intermediate as a hint.
      scoped_refptr<net::X509Certificate> intermediate_cert =
          https_test_server_.GetGeneratedIntermediate();
      ASSERT_TRUE(intermediate_cert);

      std::string b64_cert =
          base::Base64Encode(net::x509_util::CryptoBufferAsStringPiece(
              intermediate_cert->cert_buffer()));
      base::Value certs_value(base::Value::Type::LIST);
      certs_value.GetList().Append(b64_cert);
      policy::PolicyMap policies;
      SetPolicy(&policies, policy::key::kCAHintCertificates,
                std::make_optional(std::move(certs_value)));
      UpdateProviderPolicy(policies);
    }
  }

  net::EmbeddedTestServer https_test_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};

  bool add_cert_to_policy() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(CertVerifierServiceCAHintCertificatesPolicyTest,
                       TestPolicy) {
  ASSERT_TRUE(NavigateToUrl(https_test_server_.GetURL("/simple.html"), this));

  EXPECT_NE(add_cert_to_policy(),
            chrome_browser_interstitials::IsShowingInterstitial(
                chrome_test_utils::GetActiveWebContents(this)));
}

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifierServiceCAHintCertificatesPolicyTest,
                         ::testing::Bool());

#if BUILDFLAG(IS_LINUX)
// Test the CAPlatformIntegrationEnabled policy.
//
// Ideally we'd have this set up for every platform where this policy is
// supported, but on most platforms its really hard to modify the OS root
// store in an integration test without possibly messing up other tests.
// Except on Linux.
class CertVerifierServiceCAPlatformIntegrationPolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    policy::PolicyTest::SetUpOnMainThread();

    // Set up test NSS DB
    nss_db_ = std::make_unique<crypto::ScopedTestNSSDB>();
    cert_db_ = std::make_unique<net::NSSCertDatabase>(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(nss_db_->slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(nss_db_->slot())));
    ASSERT_TRUE(nss_db_->is_open());

    // Add root cert to test NSS DB.
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    net::ScopedCERTCertificateList nss_certs;
    net::ScopedCERTCertificate nss_cert =
        net::x509_util::CreateCERTCertificateFromX509Certificate(
            root_cert.get());
    ASSERT_TRUE(nss_cert);
    nss_certs.push_back(std::move(nss_cert));

    net::NSSCertDatabase::ImportCertFailureList failure_list;
    cert_db_->ImportCACerts(nss_certs,
                            /*trust_bits=*/net::NSSCertDatabase::TRUSTED_SSL,
                            &failure_list);
    ASSERT_TRUE(failure_list.empty());
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kCAPlatformIntegrationEnabled,
              absl::optional<base::Value>(platform_root_store_enabled()));
    UpdateProviderPolicy(policies);
  }

  bool platform_root_store_enabled() const { return GetParam(); }

 private:
  std::unique_ptr<crypto::ScopedTestNSSDB> nss_db_;
  std::unique_ptr<net::NSSCertDatabase> cert_db_;
};

IN_PROC_BROWSER_TEST_P(CertVerifierServiceCAPlatformIntegrationPolicyTest,
                       TestCAPlatformIntegrationPolicy) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_AUTO);
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  // `net::EmbeddedTestServer` uses `net::TestRootCerts` to install a trusted
  // root.
  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store + policies.
  net::TestRootCerts::GetInstance()->Clear();

  ASSERT_TRUE(NavigateToUrl(https_test_server.GetURL("/simple.html"), this));
  EXPECT_NE(platform_root_store_enabled(),
            chrome_browser_interstitials::IsShowingInterstitial(
                chrome_test_utils::GetActiveWebContents(this)));
}

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifierServiceCAPlatformIntegrationPolicyTest,
                         ::testing::Bool());
#endif  // BUILDFLAG(IS_LINUX)

#endif  // BUILDFLAG(CHROME_CERTIFICATE_POLICIES_SUPPORTED)

#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
class CertVerifierServiceChromeRootStoreOptionalTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    // This test puts a test cert in the Chrome Root Store, which will fail in
    // builds where Certificate Transparency is required, so disable CT
    // during this test.
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        false);
    previous_use_chrome_root_store_ =
        SystemNetworkContextManager::IsUsingChromeRootStore();

    content::GetCertVerifierServiceFactory()->SetUseChromeRootStore(
        use_chrome_root_store(), base::DoNothing());
  }

  void TearDownOnMainThread() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        std::nullopt);
    content::GetCertVerifierServiceFactory()->SetUseChromeRootStore(
        previous_use_chrome_root_store_, base::DoNothing());
  }

  bool use_chrome_root_store() const { return GetParam(); }

 protected:
  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  bool previous_use_chrome_root_store_;
};

IN_PROC_BROWSER_TEST_P(CertVerifierServiceChromeRootStoreOptionalTest, Test) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  // Use a runtime generated cert, as the pre-generated ok_cert has too long of
  // a validity period to be accepted by a publicly trusted root.
  https_test_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_AUTO);
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store.
  net::TestRootCerts::GetInstance()->Clear();

  {
    // Create updated Chrome Root Store with just the test server root cert.
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(net::CompiledChromeRootStoreVersion() +
                                       1);

    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));

    std::string proto_serialized;
    root_store_proto.SerializeToString(&proto_serialized);
    cert_verifier::mojom::ChromeRootStorePtr root_store_ptr =
        cert_verifier::mojom::ChromeRootStore::New(
            base::as_bytes(base::make_span(proto_serialized)));

    base::RunLoop update_run_loop;
    content::GetCertVerifierServiceFactory()->UpdateChromeRootStore(
        std::move(root_store_ptr), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  EXPECT_EQ(use_chrome_root_store(),
            content::NavigateToURL(GetActiveWebContents(),
                                   https_test_server.GetURL("/simple.html")));

  // The navigation should show an interstitial if CRS was not in use, since
  // the root was only trusted in the test CRS update and won't be trusted by
  // the platform roots that are used when CRS is not used.
  EXPECT_NE(use_chrome_root_store(),
            chrome_browser_interstitials::IsShowingInterstitial(
                GetActiveWebContents()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifierServiceChromeRootStoreOptionalTest,
                         ::testing::Bool());
#endif  // BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)

class CertVerifierServiceEnforceLocalAnchorConstraintsFeaturePolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<
          std::tuple<bool, std::optional<bool>>> {
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

  void SetPolicyValue(std::optional<bool> value) {
    policy::PolicyMap policies;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    SetPolicy(&policies, policy::key::kEnforceLocalAnchorConstraintsEnabled,
              std::optional<base::Value>(value));
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
    SetPolicyValue(std::nullopt);
    EXPECT_EQ(feature_enforce_local_anchor_constraints(),
              net::IsLocalAnchorConstraintsEnforcementEnabled());
#endif
  }

  bool feature_enforce_local_anchor_constraints() const {
    return std::get<0>(GetParam());
  }

  std::optional<bool> policy_enforce_local_anchor_constraints() const {
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
                       ::testing::Values(std::nullopt
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
