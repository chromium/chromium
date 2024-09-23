// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string_view>

#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Returns the Sha256 hash of the SPKI of |cert|.
net::HashValue GetSPKIHash(const CRYPTO_BUFFER* cert) {
  std::string_view spki_bytes;
  EXPECT_TRUE(net::asn1::ExtractSPKIFromDERCert(
      net::x509_util::CryptoBufferAsStringPiece(cert), &spki_bytes));
  net::HashValue sha256(net::HASH_VALUE_SHA256);
  crypto::SHA256HashString(spki_bytes, sha256.data(), crypto::kSHA256Length);
  return sha256;
}

}  // namespace

// Class used to run browser tests that verify SSL UI triggered due to
// Certificate Transparency verification failures/successes.
class CertificateTransparencyBrowserTest : public CertVerifierBrowserTest {
 public:
  CertificateTransparencyBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }

  CertificateTransparencyBrowserTest(
      const CertificateTransparencyBrowserTest&) = delete;
  CertificateTransparencyBrowserTest& operator=(
      const CertificateTransparencyBrowserTest&) = delete;

  ~CertificateTransparencyBrowserTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        std::nullopt);
  }

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  }

  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    CertVerifierBrowserTest::SetUp();
  }

  // Sets the policy identified by |policy_name| to the value specified by
  // |list_values|, ensuring that the corresponding list pref |pref_name| is
  // updated to match. |policy_name| must specify a policy that is a list of
  // string values.
  void ConfigureStringListPolicy(PrefService* pref_service,
                                 const char* policy_name,
                                 const char* pref_name,
                                 const std::vector<std::string>& list_values) {
    base::Value::List policy_value;
    for (const auto& value : list_values) {
      policy_value.Append(value);
    }
    policy::PolicyMap policy_map;
    policy_map.Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                   base::Value(std::move(policy_value)), nullptr);

    EXPECT_NO_FATAL_FAILURE(UpdateChromePolicy(policy_map));

    const base::Value::List& pref_value = pref_service->GetList(pref_name);
    std::vector<std::string> pref_values;
    for (const auto& value : pref_value) {
      ASSERT_TRUE(value.is_string());
      pref_values.push_back(value.GetString());
    }
    EXPECT_THAT(pref_values, testing::UnorderedElementsAreArray(list_values));
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  void UpdateChromePolicy(const policy::PolicyMap& policies) {
    policy_provider_.UpdateChromePolicy(policies);
    ASSERT_TRUE(base::CurrentThread::Get());

    base::RunLoop().RunUntilIdle();

    content::FlushNetworkServiceInstanceForTesting();
  }

  net::EmbeddedTestServer https_server_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// Visit an HTTPS page that has a publicly trusted certificate issued after
// the Certificate Transparency requirement date of April 2018. The connection
// should be blocked, as the server will not be providing CT details, and the
// Chrome CT Policy should be being enforced.
IN_PROC_BROWSER_TEST_F(CertificateTransparencyBrowserTest,
                       EnforcedAfterApril2018) {
  SystemNetworkContextManager::GetInstance()->SetCTLogListTimelyForTesting();

  ASSERT_TRUE(https_server()->Start());

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "may_2018.pem");
  ASSERT_TRUE(verify_result.verified_cert);
  verify_result.is_issued_by_known_root = true;
  verify_result.public_key_hashes.push_back(
      GetSPKIHash(verify_result.verified_cert->cert_buffer()));

  mock_cert_verifier()->AddResultForCert(https_server()->GetCertificate().get(),
                                         verify_result, net::OK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/ssl/google.html")));

  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      net::CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED,
      security_state::DANGEROUS,
      ssl_test_util::AuthState::SHOWING_INTERSTITIAL);
}

// Visit an HTTPS page that has a publicly trusted certificate issued after
// the Certificate Transparency requirement date of April 2018. The connection
// would normally be blocked, as the server will not be providing CT details,
// and the Chrome CT Policy should be being enforced; however, because a policy
// configuration exists that disables CT enforcement for that cert, the
// connection should succeed.
IN_PROC_BROWSER_TEST_F(CertificateTransparencyBrowserTest,
                       EnforcedAfterApril2018UnlessPoliciesSet) {
  ASSERT_TRUE(https_server()->Start());

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "may_2018.pem");
  ASSERT_TRUE(verify_result.verified_cert);
  verify_result.is_issued_by_known_root = true;
  verify_result.public_key_hashes.push_back(
      GetSPKIHash(verify_result.verified_cert->cert_buffer()));

  mock_cert_verifier()->AddResultForCert(https_server()->GetCertificate().get(),
                                         verify_result, net::OK);

  ASSERT_NO_FATAL_FAILURE(ConfigureStringListPolicy(
      browser()->profile()->GetPrefs(),
      policy::key::kCertificateTransparencyEnforcementDisabledForCas,
      certificate_transparency::prefs::kCTExcludedSPKIs,
      {verify_result.public_key_hashes.back().ToString()}));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/ssl/google.html")));
  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      ssl_test_util::CertError::NONE, security_state::SECURE,
      ssl_test_util::AuthState::NONE);
}
