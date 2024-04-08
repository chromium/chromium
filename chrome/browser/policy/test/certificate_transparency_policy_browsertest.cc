// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/safe_browsing_policy_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/hash_value.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace policy {

class CertificateTransparencyPolicyTest : public SafeBrowsingPolicyTest {
 public:
  CertificateTransparencyPolicyTest() {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }

  ~CertificateTransparencyPolicyTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        std::nullopt);
  }
};

IN_PROC_BROWSER_TEST_F(CertificateTransparencyPolicyTest,
                       CertificateTransparencyEnforcementDisabledForUrls) {
  SystemNetworkContextManager::GetInstance()->SetCTLogListTimelyForTesting();

  // Make the test root be interpreted as a known root so that CT will be
  // required.
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  net::ScopedTestKnownRoot scoped_known_root(root_cert.get());

  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  constexpr char kHostname[] = "example.com";
  https_server_ok.SetCertHostnames({kHostname});
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());

  ASSERT_TRUE(NavigateToUrl(https_server_ok.GetURL(kHostname, "/"), this));

  // The page should initially be blocked.
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(IsShowingInterstitial(tab));

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetPrimaryMainFrame(), "proceed-link"));
  EXPECT_NE(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());

  // Now exempt the URL from being blocked by setting policy.
  base::Value::List disabled_urls;
  disabled_urls.Append(kHostname);

  PolicyMap policies;
  policies.Set(key::kCertificateTransparencyEnforcementDisabledForUrls,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(disabled_urls)), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlocklistPolicy();

  ASSERT_TRUE(
      NavigateToUrl(https_server_ok.GetURL(kHostname, "/simple.html"), this));

  // There should be no interstitial after the page loads.
  EXPECT_FALSE(IsShowingInterstitial(tab));
  EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());

  // Now ensure that this setting still works after a network process crash.
  if (!content::IsOutOfProcessNetworkService())
    return;

  ASSERT_TRUE(
      NavigateToUrl(https_server_ok.GetURL(kHostname, "/title1.html"), this));

  SimulateNetworkServiceCrash();

  ASSERT_TRUE(
      NavigateToUrl(https_server_ok.GetURL(kHostname, "/simple.html"), this));

  // There should be no interstitial after the page loads.
  EXPECT_FALSE(IsShowingInterstitial(tab));
  EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
}

IN_PROC_BROWSER_TEST_F(CertificateTransparencyPolicyTest,
                       CertificateTransparencyEnforcementDisabledForCas) {
  SystemNetworkContextManager::GetInstance()->SetCTLogListTimelyForTesting();

  // Make the test root be interpreted as a known root so that CT will be
  // required.
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  net::ScopedTestKnownRoot scoped_known_root(root_cert.get());

  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  constexpr char kHostname[] = "example.com";
  https_server_ok.SetCertHostnames({kHostname});
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());

  ASSERT_TRUE(NavigateToUrl(https_server_ok.GetURL(kHostname, "/"), this));

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // The page should initially be blocked.
  content::RenderFrameHost* main_frame;
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          web_contents);
  ASSERT_TRUE(helper);
  ASSERT_TRUE(
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
  main_frame = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(content::WaitForRenderFrameReady(main_frame));
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      main_frame, "proceed-link"));

  EXPECT_NE(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());

  // Now exempt the leaf SPKI from being blocked by setting policy.
  net::HashValue leaf_hash;
  ASSERT_TRUE(net::x509_util::CalculateSha256SpkiHash(
      https_server_ok.GetCertificate()->cert_buffer(), &leaf_hash));
  base::Value::List disabled_spkis;
  disabled_spkis.Append(leaf_hash.ToString());

  PolicyMap policies;
  policies.Set(key::kCertificateTransparencyEnforcementDisabledForCas,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(disabled_spkis)), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlocklistPolicy();

  ASSERT_TRUE(
      NavigateToUrl(https_server_ok.GetURL(kHostname, "/simple.html"), this));

  // Check we are no longer in the interstitial.
  EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
}

}  // namespace policy
