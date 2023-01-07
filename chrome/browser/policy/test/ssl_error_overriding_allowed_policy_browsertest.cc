// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/policy/safe_browsing_policy_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

void SendInterstitialCommand(
    content::WebContents* tab,
    security_interstitials::SecurityInterstitialCommand command) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
      ->CommandReceived(base::NumberToString(command));
  return;
}

// Test that when SSL error overriding policies are unset, the proceed link
// appears on SSL blocking pages.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       SSLErrorOverridingAllowedDefaults) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();

  // Policy should allow overriding by default. Allow list should be empty by
  // default.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_TRUE(
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins).empty());

  // Policy allows overriding - navigate to an SSL error page and expect the
  // proceed link.
  ASSERT_TRUE(NavigateToUrl(https_server_expired.GetURL("/"), this));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(IsShowingInterstitial(tab));

  // The interstitial should display the proceed link.
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetPrimaryMainFrame(), "proceed-link"));
}

// Test that when SSL error overriding is allowed, the origin list is ignored
// and the proceed link appears on SSL blocking pages.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       SSLErrorOverridingAllowedEnabled) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();

  // Policy should allow overriding by default. Allow list should be empty by
  // default.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_TRUE(
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins).empty());

  // Add a policy to allow overriding on specific sites only. Since
  // kSSLErrorOverrideAllowed is enabled, this should do nothing.
  base::Value::List allow_list;
  allow_list.Append("example.com");
  PolicyMap policies;
  policies.Set(key::kSSLErrorOverrideAllowedForOrigins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allow_list)), nullptr);
  UpdateProviderPolicy(policies);

  // Policy should be set.
  EXPECT_FALSE(
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins).empty());

  // Policy allows overriding - navigate to an SSL error page and expect the
  // proceed link.
  ASSERT_TRUE(NavigateToUrl(https_server_expired.GetURL("/"), this));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(IsShowingInterstitial(tab));

  // The interstitial should display the proceed link.
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetPrimaryMainFrame(), "proceed-link"));
}

// Test that when SSL error overriding is disabled, the proceed link does not
// appear appear on SSL blocking pages.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       SSLErrorOverridingAllowedDisabled) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));

  // Disallowing the proceed link by setting the policy to |false|.
  PolicyMap policies;
  policies.Set(key::kSSLErrorOverrideAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);

  // Policy should not allow overriding.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));

  // Policy disallows overriding - navigate to an SSL error page and expect no
  // proceed link.
  ASSERT_TRUE(NavigateToUrl(https_server_expired.GetURL("/"), this));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(IsShowingInterstitial(tab));

  // The interstitial should not display the proceed link.
  EXPECT_FALSE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetPrimaryMainFrame(), "proceed-link"));

  // The interstitial should not proceed, even if the command is sent in
  // some other way (e.g., via the keyboard shortcut).
  SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
  EXPECT_TRUE(IsShowingInterstitial(tab));
}

// Test that when SSL error overriding is disallowed by policy and the origin
// list is configured, the proceed link does not appear on SSL blocking pages if
// the page is not on the origin list.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       SSLErrorOverridingAllowedForOriginsWrongOrigin) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();

  // Policy should allow overriding by default. Allow list should be empty by
  // default.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_TRUE(
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins).empty());

  // Disallowing the proceed link by setting the policy to |false|.
  PolicyMap policies;
  policies.Set(key::kSSLErrorOverrideAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  // Add a policy to allow overriding on specific sites only.
  base::Value::List allow_list;
  allow_list.Append("example.com");
  policies.Set(key::kSSLErrorOverrideAllowedForOrigins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allow_list)), nullptr);
  UpdateProviderPolicy(policies);

  // Policy should be set.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_FALSE(
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins).empty());

  // Policy disallows overriding - navigate to an SSL error page and expect no
  // proceed link.
  ASSERT_TRUE(NavigateToUrl(https_server_expired.GetURL("/"), this));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(IsShowingInterstitial(tab));

  // The interstitial should not display the proceed link.
  EXPECT_FALSE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetPrimaryMainFrame(), "proceed-link"));

  // The interstitial should not proceed, even if the command is sent in
  // some other way (e.g., via the keyboard shortcut).
  SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
  EXPECT_TRUE(IsShowingInterstitial(tab));
}

// Test that when SSL error overriding is disallowed by policy and the origin
// list is configured incorrectly, the proceed link does not appear on SSL
// blocking pages.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       SSLErrorOverridingForOriginsBadInput) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_TRUE(
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins).empty());

  // Disallowing the proceed link by setting the policy to |false|.
  PolicyMap policies;
  policies.Set(key::kSSLErrorOverrideAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  base::Value::List allow_list;
  // We ignore "*" or badly formed patterns as inputs.
  allow_list.Append("*");
  allow_list.Append("bad 127.0.0.1 input");
  policies.Set(key::kSSLErrorOverrideAllowedForOrigins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allow_list)), nullptr);
  UpdateProviderPolicy(policies);

  // Policy should not allow overriding.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_FALSE(
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins).empty());

  // Policy disallows overriding - navigate to an SSL error page and expect no
  // proceed link.
  ASSERT_TRUE(NavigateToUrl(https_server_expired.GetURL("/"), this));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(IsShowingInterstitial(tab));

  // The interstitial should not display the proceed link.
  EXPECT_FALSE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetPrimaryMainFrame(), "proceed-link"));

  // The interstitial should not proceed, even if the command is sent in
  // some other way (e.g., via the keyboard shortcut).
  SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
  EXPECT_TRUE(IsShowingInterstitial(tab));
}

// Test that when SSL error overriding is disallowed by policy and the origin
// list is empty, the proceed link does not appear on SSL blocking pages.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       SSLErrorOverridingForOriginsEmptyList) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_TRUE(
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins).empty());

  // Disallowing the proceed link by setting the policy to |false|.
  PolicyMap policies;
  policies.Set(key::kSSLErrorOverrideAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  // The policy is intentionally configured with an empty list for this test.
  base::Value::List allow_list;
  policies.Set(key::kSSLErrorOverrideAllowedForOrigins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allow_list)), nullptr);
  UpdateProviderPolicy(policies);

  // Policy should not allow overriding.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_TRUE(
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins).empty());

  // Policy disallows overriding - navigate to an SSL error page and expect no
  // proceed link.
  ASSERT_TRUE(NavigateToUrl(https_server_expired.GetURL("/"), this));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(IsShowingInterstitial(tab));

  // The interstitial should not display the proceed link.
  EXPECT_FALSE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetPrimaryMainFrame(), "proceed-link"));

  // The interstitial should not proceed, even if the command is sent in
  // some other way (e.g., via the keyboard shortcut).
  SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
  EXPECT_TRUE(IsShowingInterstitial(tab));
}

// Test that when SSL error overriding is disallowed by policy and the origin
// list is configured, the proceed link appears on SSL blocking pages if the
// page is on the origin list.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       SSLErrorOverridingAllowedForOrigins) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();

  // Policy should allow overriding by default. Allow list should be empty by
  // default.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_TRUE(
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins).empty());

  // Disallowing the proceed link by setting the policy to |false|.
  PolicyMap policies;
  policies.Set(key::kSSLErrorOverrideAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  // Add a policy to allow overriding on specific sites only. The path should be
  // ignored.
  base::Value::List allow_list;
  allow_list.Append("127.0.0.1/my/path/to/file.ext");
  policies.Set(key::kSSLErrorOverrideAllowedForOrigins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allow_list)), nullptr);
  UpdateProviderPolicy(policies);

  // Policy should be set.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));
  EXPECT_FALSE(
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins).empty());

  // Policy allows overriding - navigate to an SSL error page and expect the
  // proceed link.
  ASSERT_TRUE(NavigateToUrl(https_server_expired.GetURL("/"), this));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(IsShowingInterstitial(tab));

  // The interstitial should display the proceed link.
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetPrimaryMainFrame(), "proceed-link"));
}

}  // namespace policy
