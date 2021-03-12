// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace policy {

class CertificateTransparencyPolicyTest : public PolicyTest {
 public:
  CertificateTransparencyPolicyTest() {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }

  ~CertificateTransparencyPolicyTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        base::nullopt);
  }
};

IN_PROC_BROWSER_TEST_F(CertificateTransparencyPolicyTest,
                       CertificateTransparencyEnforcementDisabledForUrls) {
  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_ok.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());

  // Require CT for all hosts (in the absence of policy).
  SetRequireCTForTesting(true);

  ui_test_utils::NavigateToURL(browser(), https_server_ok.GetURL("/"));

  // The page should initially be blocked.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetMainFrame(), "proceed-link"));
  EXPECT_NE(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Now exempt the URL from being blocked by setting policy.
  base::Value disabled_urls(base::Value::Type::LIST);
  disabled_urls.Append(https_server_ok.host_port_pair().HostForURL());

  PolicyMap policies;
  policies.Set(key::kCertificateTransparencyEnforcementDisabledForUrls,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::move(disabled_urls), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  ui_test_utils::NavigateToURL(browser(),
                               https_server_ok.GetURL("/simple.html"));

  // There should be no interstitial after the page loads.
  EXPECT_FALSE(IsShowingInterstitial(tab));
  EXPECT_EQ(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Now ensure that this setting still works after a network process crash.
  if (!content::IsOutOfProcessNetworkService())
    return;

  ui_test_utils::NavigateToURL(browser(),
                               https_server_ok.GetURL("/title1.html"));

  SimulateNetworkServiceCrash();
  SetRequireCTForTesting(true);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_ok.GetURL("/simple.html"));

  // There should be no interstitial after the page loads.
  EXPECT_FALSE(IsShowingInterstitial(tab));
  EXPECT_EQ(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

IN_PROC_BROWSER_TEST_F(CertificateTransparencyPolicyTest,
                       CertificateTransparencyEnforcementDisabledForCas) {
  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_ok.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());

  // Require CT for all hosts (in the absence of policy).
  SetRequireCTForTesting(true);

  ui_test_utils::NavigateToURL(browser(), https_server_ok.GetURL("/"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // The page should initially be blocked.
  content::RenderFrameHost* main_frame;
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          web_contents);
  ASSERT_TRUE(helper);
  ASSERT_TRUE(
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
  main_frame = web_contents->GetMainFrame();
  ASSERT_TRUE(content::WaitForRenderFrameReady(main_frame));
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      main_frame, "proceed-link"));

  EXPECT_NE(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Now exempt the leaf SPKI from being blocked by setting policy.
  net::HashValue leaf_hash;
  ASSERT_TRUE(net::x509_util::CalculateSha256SpkiHash(
      https_server_ok.GetCertificate()->cert_buffer(), &leaf_hash));
  base::Value disabled_spkis(base::Value::Type::LIST);
  disabled_spkis.Append(leaf_hash.ToString());

  PolicyMap policies;
  policies.Set(key::kCertificateTransparencyEnforcementDisabledForCas,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::move(disabled_spkis), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  ui_test_utils::NavigateToURL(browser(),
                               https_server_ok.GetURL("/simple.html"));

  // Check we are no longer in the interstitial.
  EXPECT_EQ(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

}  // namespace policy
