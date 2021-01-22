// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

// Test that when SSL error overriding is allowed by policy (default), the
// proceed link appears on SSL blocking pages.
IN_PROC_BROWSER_TEST_F(PolicyTest, SSLErrorOverridingAllowedDefaults) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs = browser()->profile()->GetPrefs();

  // Policy should allow overriding by default. Allow list should be empty by
  // default.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_TRUE(prefs->GetList(prefs::kSSLErrorOverrideAllowedForUrls)
                  ->GetList()
                  .empty());

  // Policy allows overriding - navigate to an SSL error page and expect the
  // proceed link.
  ui_test_utils::NavigateToURL(browser(), https_server_expired.GetURL("/"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);

  // The interstitial should display the proceed link.
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetMainFrame(), "proceed-link"));
}

// Test that when SSL error overriding is allowed by policy and the URL list is
// configured, the proceed link does not appear on SSL blocking pages if the
// page is not on the URL list.
IN_PROC_BROWSER_TEST_F(PolicyTest, SSLErrorOverridingNotAllowedForUrls) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs = browser()->profile()->GetPrefs();

  // Policy should allow overriding by default. Allow list should be empty by
  // default.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_TRUE(prefs->GetList(prefs::kSSLErrorOverrideAllowedForUrls)
                  ->GetList()
                  .empty());

  // Add a policy to allow overriding on specific sites only.
  std::vector<base::Value> allow_list;
  allow_list.emplace_back(base::Value("example.com"));
  PolicyMap policies;
  policies.Set(key::kSSLErrorOverrideAllowedForUrls, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allow_list)), nullptr);
  UpdateProviderPolicy(policies);

  // Policy should be set.
  EXPECT_FALSE(prefs->GetList(prefs::kSSLErrorOverrideAllowedForUrls)
                   ->GetList()
                   .empty());

  // Policy disallows overriding - navigate to an SSL error page and expect no
  // proceed link.
  ui_test_utils::NavigateToURL(browser(), https_server_expired.GetURL("/"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);

  // The interstitial should not display the proceed link.
  EXPECT_FALSE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetMainFrame(), "proceed-link"));

  // The interstitial should not proceed, even if the command is sent in
  // some other way (e.g., via the keyboard shortcut).
  SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
  EXPECT_TRUE(IsShowingInterstitial(tab));
}

// Test that when SSL error overriding is allowed by policy and the URL list is
// configured, the proceed link appears on SSL blocking pages if the page is on
// the URL list.
IN_PROC_BROWSER_TEST_F(PolicyTest, SSLErrorOverridingAllowedForUrls) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs = browser()->profile()->GetPrefs();

  // Policy should allow overriding by default. Allow list should be empty by
  // default.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_TRUE(prefs->GetList(prefs::kSSLErrorOverrideAllowedForUrls)
                  ->GetList()
                  .empty());

  // Add a policy to allow overriding on specific sites only.
  std::vector<base::Value> allow_list;
  allow_list.emplace_back(base::Value("127.0.0.1"));
  PolicyMap policies;
  policies.Set(key::kSSLErrorOverrideAllowedForUrls, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allow_list)), nullptr);
  UpdateProviderPolicy(policies);

  // Policy should be set.
  EXPECT_FALSE(prefs->GetList(prefs::kSSLErrorOverrideAllowedForUrls)
                   ->GetList()
                   .empty());

  // Policy allows overriding - navigate to an SSL error page and expect the
  // proceed link.
  ui_test_utils::NavigateToURL(browser(), https_server_expired.GetURL("/"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);

  // The interstitial should display the proceed link.
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetMainFrame(), "proceed-link"));
}

// Test that when SSL error overriding is disallowed by policy, the
// proceed link does not appear on SSL blocking pages and users should not
// be able to proceed.
IN_PROC_BROWSER_TEST_F(PolicyTest, SSLErrorOverridingDisallowed) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_TRUE(prefs->GetList(prefs::kSSLErrorOverrideAllowedForUrls)
                  ->GetList()
                  .empty());

  // Disallowing the proceed link by setting the policy to |false|.
  PolicyMap policies;
  policies.Set(key::kSSLErrorOverrideAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  // But allowing the URL in the allow list.
  std::vector<base::Value> allow_list;
  allow_list.emplace_back(base::Value("127.0.0.1"));
  policies.Set(key::kSSLErrorOverrideAllowedForUrls, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allow_list)), nullptr);
  UpdateProviderPolicy(policies);

  // Policy should not allow overriding anymore.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_FALSE(prefs->GetList(prefs::kSSLErrorOverrideAllowedForUrls)
                   ->GetList()
                   .empty());

  // Policy disallows overriding - navigate to an SSL error page and expect no
  // proceed link.
  ui_test_utils::NavigateToURL(browser(), https_server_expired.GetURL("/"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);

  // The interstitial should not display the proceed link.
  EXPECT_FALSE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetMainFrame(), "proceed-link"));

  // The interstitial should not proceed, even if the command is sent in
  // some other way (e.g., via the keyboard shortcut).
  SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
  EXPECT_TRUE(IsShowingInterstitial(tab));
}

}  // namespace policy
