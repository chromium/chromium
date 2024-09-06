// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/safe_browsing_policy_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

using safe_browsing::ReusedPasswordAccountType;
using testing::Return;

namespace policy {

int IsEnhancedProtectionMessageVisibleOnInterstitial(
    SafeBrowsingPolicyTest* browser_test) {
  const std::string command = base::StringPrintf(
      "var node = document.getElementById('enhanced-protection-message');"
      "var result;"
      "if (node) {"
      "  result = node.offsetWidth > 0 || "
      "      node.offsetHeight > 0 ? %d : %d;"
      "} else {"
      // The node should be present but not visible, so trigger an error
      // by sending false if it's not present.
      "  result = %d;"
      "}"
      "result;",
      security_interstitials::CMD_TEXT_FOUND,
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_ERROR);

  content::WebContents* tab =
      chrome_test_utils::GetActiveWebContents(browser_test);
  if (!browser_test->IsShowingInterstitial(tab)) {
    ADD_FAILURE() << "Expected interstitial when checking for enhanced "
                     "protection message.";
  }
  return content::EvalJs(tab->GetPrimaryMainFrame(), command).ExtractInt();
}

// Test extended reporting is managed by policy.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       SafeBrowsingExtendedReportingPolicyManaged) {
  // Set the extended reporting pref to True and ensure the enterprise policy
  // can overwrite it.
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  prefs->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled, true);

  // Set the enterprise policy to disable extended reporting.
  EXPECT_TRUE(
      prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed));
  PolicyMap policies;
  policies.Set(key::kSafeBrowsingExtendedReportingEnabled,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(false), nullptr);
  UpdateProviderPolicy(policies);
  // Policy should have overwritten the pref, and it should be managed.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kSafeBrowsingScoutReportingEnabled));

  // Also make sure the SafeBrowsing prefs helper functions agree with the
  // policy.
  if (base::FeatureList::IsEnabled(
          safe_browsing::kExtendedReportingRemovePrefDependency)) {
    EXPECT_FALSE(safe_browsing::IsExtendedReportingPolicyManaged(*prefs));
  } else {
    EXPECT_TRUE(safe_browsing::IsExtendedReportingPolicyManaged(*prefs));
  }

  // Note that making SBER policy managed does NOT affect the SBEROptInAllowed
  // setting, which is intentionally kept distinct for now. When the latter is
  // deprecated, then SBER's policy management will imply whether the checkbox
  // is visible.
  if (base::FeatureList::IsEnabled(
          safe_browsing::kExtendedReportingRemovePrefDependency)) {
    EXPECT_FALSE(safe_browsing::IsExtendedReportingOptInAllowed(*prefs));
  } else {
    EXPECT_TRUE(safe_browsing::IsExtendedReportingOptInAllowed(*prefs));
  }
}

#if !BUILDFLAG(IS_ANDROID)
// Test that when Safe Browsing state is managed by policy, the enhanced
// protection message does not appear on SSL blocking pages.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest, SafeBrowsingStatePolicyManaged) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  // Set the Safe Browsing state to standard protection.
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  safe_browsing::SetSafeBrowsingState(
      prefs, safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  // First, navigate to an SSL error page and make sure the enhanced protection
  // message appears by default.
  ASSERT_TRUE(NavigateToUrl(https_server_expired.GetURL("/"), this));
  EXPECT_EQ(security_interstitials::CMD_TEXT_FOUND,
            IsEnhancedProtectionMessageVisibleOnInterstitial(this));

  // Set the enterprise policy to force standard protection.
  PolicyMap policies;
  policies.Set(policy::key::kSafeBrowsingProtectionLevel,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(/* standard protection */ 1), nullptr);
  UpdateProviderPolicy(policies);
  // Policy should have overwritten the pref, and it should be managed.
  EXPECT_EQ(safe_browsing::SafeBrowsingState::STANDARD_PROTECTION,
            safe_browsing::GetSafeBrowsingState(*prefs));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kSafeBrowsingEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kSafeBrowsingEnhanced));

  // Navigate to an SSL error page, the enhanced protection message should not
  // appear.
  ASSERT_TRUE(NavigateToUrl(https_server_expired.GetURL("/"), this));
  EXPECT_EQ(security_interstitials::CMD_TEXT_NOT_FOUND,
            IsEnhancedProtectionMessageVisibleOnInterstitial(this));
}

// Test that when safe browsing allowlist domains are set by policy, safe
// browsing service gets the correct value.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest, SafeBrowsingAllowlistDomains) {
  // Without setting up the enterprise policy,
  // |GetSafeBrowsingDomainsPref(..) should return empty list.
  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();
  EXPECT_FALSE(
      prefs->FindPreference(prefs::kSafeBrowsingAllowlistDomains)->IsManaged());
  std::vector<std::string> canonicalized_domains;
  safe_browsing::GetSafeBrowsingAllowlistDomainsPref(*prefs,
                                                     &canonicalized_domains);
  EXPECT_TRUE(canonicalized_domains.empty());

  // Add 2 allowlisted domains to this policy.
  PolicyMap policies;
  base::Value::List allowlist_domains;
  allowlist_domains.Append("mydomain.com");
  allowlist_domains.Append("mydomain.net");
  policies.Set(key::kSafeBrowsingAllowlistDomains, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(allowlist_domains.Clone()), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kSafeBrowsingAllowlistDomains)->IsManaged());
  safe_browsing::GetSafeBrowsingAllowlistDomainsPref(*prefs,
                                                     &canonicalized_domains);
  EXPECT_EQ(2u, canonicalized_domains.size());
  EXPECT_EQ("mydomain.com", canonicalized_domains[0]);
  EXPECT_EQ("mydomain.net", canonicalized_domains[1]);

  // Invalid domains will be skipped.
  allowlist_domains.clear();
  allowlist_domains.Append("%EF%BF%BDzyx.com");
  policies.Set(key::kSafeBrowsingAllowlistDomains, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(allowlist_domains.Clone()), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kSafeBrowsingAllowlistDomains)->IsManaged());
  canonicalized_domains.clear();
  safe_browsing::GetSafeBrowsingAllowlistDomainsPref(*prefs,
                                                     &canonicalized_domains);
  EXPECT_TRUE(canonicalized_domains.empty());
}

// Test that when password protection login URLs are set by policy, password
// protection service gets the correct value.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest, PasswordProtectionLoginURLs) {
  // Without setting up the enterprise policy,
  // |GetPasswordProtectionLoginURLsPref(..) should return empty list.
  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();
  EXPECT_FALSE(
      prefs->FindPreference(prefs::kPasswordProtectionLoginURLs)->IsManaged());
  std::vector<GURL> login_urls;
  safe_browsing::GetPasswordProtectionLoginURLsPref(*prefs, &login_urls);
#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS prepopulates chrome:// URLs.
  EXPECT_FALSE(login_urls.empty());
#else
  EXPECT_TRUE(login_urls.empty());
#endif
  // Add 2 login URLs to this enterprise policy .
  PolicyMap policies;
  base::Value::List login_url_values;
  login_url_values.Append("https://login.mydomain.com");
  login_url_values.Append("https://mydomian.com/login.html");
  policies.Set(key::kPasswordProtectionLoginURLs, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(login_url_values.Clone()), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kPasswordProtectionLoginURLs)->IsManaged());
  safe_browsing::GetPasswordProtectionLoginURLsPref(*prefs, &login_urls);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(3u, login_urls.size());
  EXPECT_EQ(GURL("chrome://os-settings"), login_urls[0]);
  EXPECT_EQ(GURL("https://login.mydomain.com"), login_urls[1]);
  EXPECT_EQ(GURL("https://mydomian.com/login.html"), login_urls[2]);
#else
  EXPECT_EQ(2u, login_urls.size());
  EXPECT_EQ(GURL("https://login.mydomain.com"), login_urls[0]);
  EXPECT_EQ(GURL("https://mydomian.com/login.html"), login_urls[1]);
#endif

  // Verify unsupported schemes, or invalid URLs will be skipped.
  login_url_values.clear();
  login_url_values.Append("invalid");
  login_url_values.Append("ftp://login.mydomain.com");
  policies.Set(key::kPasswordProtectionLoginURLs, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(login_url_values.Clone()), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kPasswordProtectionLoginURLs)->IsManaged());
  login_urls.clear();
  safe_browsing::GetPasswordProtectionLoginURLsPref(*prefs, &login_urls);
#if BUILDFLAG(IS_CHROMEOS)
  // Prepopulated URL should be present.
  EXPECT_FALSE(login_urls.empty());
#else
  EXPECT_TRUE(login_urls.empty());
#endif
}

// Test that when password protection change password URL is set by policy,
// password protection service gets the correct value.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       PasswordProtectionChangePasswordURL) {
  // Without setting up the enterprise policy,
  // |GetEnterpriseChangePasswordURL(..) should return default GAIA change
  // password URL.
  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();
  const safe_browsing::ChromePasswordProtectionService* const service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(chrome_test_utils::GetProfile(this));
  EXPECT_FALSE(
      prefs->FindPreference(prefs::kPasswordProtectionChangePasswordURL)
          ->IsManaged());
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kPasswordProtectionChangePasswordURL));
  EXPECT_TRUE(service->GetEnterpriseChangePasswordURL().DomainIs(
      "accounts.google.com"));

  // Add change password URL to this enterprise policy .
  PolicyMap policies;
  policies.Set(key::kPasswordProtectionChangePasswordURL,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value("https://changepassword.mydomain.com"), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->FindPreference(prefs::kPasswordProtectionChangePasswordURL)
                  ->IsManaged());
  EXPECT_EQ(GURL("https://changepassword.mydomain.com"),
            service->GetEnterpriseChangePasswordURL());

  // Verify non-http/https change password URL will be skipped.
  policies.Set(key::kPasswordProtectionChangePasswordURL,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value("data:text/html,login page"), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->FindPreference(prefs::kPasswordProtectionChangePasswordURL)
                  ->IsManaged());
  EXPECT_TRUE(service->GetEnterpriseChangePasswordURL().DomainIs(
      "accounts.google.com"));
}

class MockPasswordProtectionService
    : public safe_browsing::ChromePasswordProtectionService {
 public:
  MockPasswordProtectionService(safe_browsing::SafeBrowsingService* sb_service,
                                Profile* profile)
      : safe_browsing::ChromePasswordProtectionService(sb_service, profile) {}
  ~MockPasswordProtectionService() override {}

  MOCK_CONST_METHOD0(IsPrimaryAccountGmail, bool());

  AccountInfo GetAccountInfo() const override {
    AccountInfo info;
    info.email = "user@mycompany.com";
    return info;
  }
};

// Test that when password protection warning trigger is set for users who are
// not signed-into Chrome, Chrome password protection service gets the correct
// value.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       PasswordProtectionWarningTriggerNotLoggedIn) {
  MockPasswordProtectionService mock_service(
      g_browser_process->safe_browsing_service(),
      chrome_test_utils::GetProfile(this));

  // If user is not signed-in, |GetPasswordProtectionWarningTriggerPref(...)|
  // should return |PHISHING_REUSE| unless specified by policy.
  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();
  EXPECT_FALSE(prefs->FindPreference(prefs::kPasswordProtectionWarningTrigger)
                   ->IsManaged());
  EXPECT_EQ(safe_browsing::PHISHING_REUSE,
            mock_service.GetPasswordProtectionWarningTriggerPref(
                ReusedPasswordAccountType()));
  // Sets the enterprise policy to 1 (a.k.a PASSWORD_REUSE).
  PolicyMap policies;
  policies.Set(key::kPasswordProtectionWarningTrigger, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->FindPreference(prefs::kPasswordProtectionWarningTrigger)
                  ->IsManaged());
  EXPECT_EQ(safe_browsing::PASSWORD_REUSE,
            mock_service.GetPasswordProtectionWarningTriggerPref(
                ReusedPasswordAccountType()));
  // Sets the enterprise policy to 2 (a.k.a PHISHING_REUSE).
  policies.Set(key::kPasswordProtectionWarningTrigger, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(2), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(safe_browsing::PHISHING_REUSE,
            mock_service.GetPasswordProtectionWarningTriggerPref(
                ReusedPasswordAccountType()));
}

// Test that when password protection warning trigger is set for Gmail users,
// Chrome password protection service gets the correct
// value.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       PasswordProtectionWarningTriggerGmail) {
  MockPasswordProtectionService mock_service(
      g_browser_process->safe_browsing_service(),
      chrome_test_utils::GetProfile(this));

  // If user is a Gmail user, |GetPasswordProtectionWarningTriggerPref(...)|
  // should return |PHISHING_REUSE| unless specified by policy.
  EXPECT_CALL(mock_service, IsPrimaryAccountGmail())
      .WillRepeatedly(Return(true));
  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();
  EXPECT_FALSE(prefs->FindPreference(prefs::kPasswordProtectionWarningTrigger)
                   ->IsManaged());
  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::GMAIL);
  account_type.set_is_account_syncing(true);
  EXPECT_EQ(safe_browsing::PHISHING_REUSE,
            mock_service.GetPasswordProtectionWarningTriggerPref(account_type));
  // Sets the enterprise policy to 1 (a.k.a PASSWORD_REUSE). Gmail accounts
  // should always return PHISHING_REUSE if the policy is not set to 0.
  PolicyMap policies;
  policies.Set(key::kPasswordProtectionWarningTrigger, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->FindPreference(prefs::kPasswordProtectionWarningTrigger)
                  ->IsManaged());
  EXPECT_EQ(safe_browsing::PHISHING_REUSE,
            mock_service.GetPasswordProtectionWarningTriggerPref(account_type));
  // Sets the enterprise policy to 2 (a.k.a PHISHING_REUSE).
  policies.Set(key::kPasswordProtectionWarningTrigger, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(2), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(safe_browsing::PHISHING_REUSE,
            mock_service.GetPasswordProtectionWarningTriggerPref(account_type));
  // Sets the enterprise policy to 0 (a.k.a PASSWORD_PROTECTION_OFF).
  policies.Set(key::kPasswordProtectionWarningTrigger, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(safe_browsing::PASSWORD_PROTECTION_OFF,
            mock_service.GetPasswordProtectionWarningTriggerPref(account_type));
}

// Test that when password protection warning trigger is set for GSuite users,
// Chrome password protection service gets the correct value.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPolicyTest,
                       PasswordProtectionWarningTriggerGSuite) {
  MockPasswordProtectionService mock_service(
      g_browser_process->safe_browsing_service(),
      chrome_test_utils::GetProfile(this));
  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();
  PolicyMap policies;

  // If user is a GSuite user, |GetPasswordProtectionWarningTriggerPref(...)|
  // should return |PHISHING_REUSE| unless specified by policy.
  EXPECT_FALSE(prefs->FindPreference(prefs::kPasswordProtectionWarningTrigger)
                   ->IsManaged());
  EXPECT_EQ(safe_browsing::PHISHING_REUSE,
            mock_service.GetPasswordProtectionWarningTriggerPref(
                ReusedPasswordAccountType()));
  // Sets the enterprise policy to 1 (a.k.a PASSWORD_REUSE).
  policies.Set(key::kPasswordProtectionWarningTrigger, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->FindPreference(prefs::kPasswordProtectionWarningTrigger)
                  ->IsManaged());
  EXPECT_EQ(safe_browsing::PASSWORD_REUSE,
            mock_service.GetPasswordProtectionWarningTriggerPref(
                ReusedPasswordAccountType()));
  // Sets the enterprise policy to 2 (a.k.a PHISHING_REUSE).
  policies.Set(key::kPasswordProtectionWarningTrigger, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(2), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(safe_browsing::PHISHING_REUSE,
            mock_service.GetPasswordProtectionWarningTriggerPref(
                ReusedPasswordAccountType()));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace policy
