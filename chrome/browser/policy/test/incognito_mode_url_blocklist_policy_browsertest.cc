// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/policy/url_blocking_policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace policy {

class IncognitoUrlBlockingPolicyTest
    : public UrlBlockingPolicyTest,
      public testing::WithParamInterface<bool> {
 protected:
  bool IsBrowserInIncognitoMode() const { return GetParam(); }
};

void CheckCanOpenURL(Browser* browser, const std::string& spec) {
  GURL url(spec);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetLastCommittedURL());

  std::u16string blocked_page_title;
  if (url.has_host()) {
    blocked_page_title = base::UTF8ToUTF16(url.GetHost());
  } else {
    // Local file paths show the full URL.
    blocked_page_title = base::UTF8ToUTF16(url.spec());
  }
  EXPECT_NE(blocked_page_title, contents->GetTitle());
}

INSTANTIATE_TEST_SUITE_P(All, IncognitoUrlBlockingPolicyTest, testing::Bool());

// Checks that URLs can be blocklisted only in Incognito mode.
IN_PROC_BROWSER_TEST_P(IncognitoUrlBlockingPolicyTest, IncognitoBlocklist) {
  Browser* test_browser;
  if (IsBrowserInIncognitoMode()) {
    test_browser =
        OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  } else {
    test_browser = browser();
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  const auto url =
      embedded_test_server()->GetURL("aaa.com", "/empty.html").spec();

  // Set a blocklist.
  base::ListValue blocklist;
  blocklist.Append("aaa.com");
  PolicyMap policies;
  policies.Set(key::kIncognitoModeUrlBlocklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(blocklist)), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlocklistPolicy();
  if (IsBrowserInIncognitoMode()) {
    CheckURLIsBlocked(test_browser, url,
                      /*is_blocked_by_incognito_policy=*/true);
  } else {
    CheckCanOpenURL(test_browser, url);
  }
}

IN_PROC_BROWSER_TEST_F(IncognitoUrlBlockingPolicyTest,
                       IncognitoAllBlocklistAndAllowlist) {
  // Checks that the allowlist works as an exception to the blocklist.
  Browser* test_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set a blocklist that blocks everything.
  base::ListValue blocklist;
  blocklist.Append("*");
  PolicyMap policies;
  policies.Set(key::kIncognitoModeUrlBlocklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(blocklist)), nullptr);

  // Set an allowlist that allows an URL.
  base::ListValue allowlist;
  allowlist.Append("aaa.com");
  policies.Set(key::kIncognitoModeUrlAllowlist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allowlist)), nullptr);

  UpdateProviderPolicy(policies);
  FlushBlocklistPolicy();

  CheckCanOpenURL(
      test_browser,
      embedded_test_server()->GetURL("aaa.com", "/empty.html").spec());
}

IN_PROC_BROWSER_TEST_F(IncognitoUrlBlockingPolicyTest,
                       IncognitoBlocklistAndAllowlist) {
  // Checks that the allowlist works as an exception to specific blocklist
  // entries.
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string blocked_url1 =
      embedded_test_server()->GetURL("blocked1.com", "/empty.html").spec();
  const std::string blocked_url2 =
      embedded_test_server()->GetURL("blocked2.com", "/empty.html").spec();
  const std::string allowed_url =
      embedded_test_server()->GetURL("allowed.com", "/empty.html").spec();

  // Set a blocklist that blocks specific URLs.
  base::ListValue blocklist;
  blocklist.Append("blocked1.com");
  blocklist.Append("blocked2.com");
  blocklist.Append("allowed.com");
  PolicyMap policies;
  policies.Set(key::kIncognitoModeUrlBlocklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(blocklist)), nullptr);

  // Set an allowlist that allows `allowed_url`.
  base::ListValue allowlist;
  allowlist.Append("allowed.com");
  policies.Set(key::kIncognitoModeUrlAllowlist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allowlist)), nullptr);

  UpdateProviderPolicy(policies);
  FlushBlocklistPolicy();

  CheckURLIsBlocked(incognito_browser, blocked_url1,
                    /*is_blocked_by_incognito_policy=*/true);
  CheckURLIsBlocked(incognito_browser, blocked_url2,
                    /*is_blocked_by_incognito_policy=*/true);
  CheckCanOpenURL(incognito_browser, allowed_url);
}

IN_PROC_BROWSER_TEST_P(IncognitoUrlBlockingPolicyTest,
                       IncognitoBlocklistAndUrlAllowlist) {
  // Checks that the Incognito blocklist takes precedence over the URL
  // allowlist in Incognito mode.
  Browser* test_browser;
  if (IsBrowserInIncognitoMode()) {
    test_browser =
        OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  } else {
    test_browser = browser();
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  const auto url = embedded_test_server()
                       ->GetURL("blockincognito.com", "/empty.html")
                       .spec();

  // Set a blocklist for Incognito mode that blocks `url`.
  base::ListValue incognito_blocklist;
  incognito_blocklist.Append("blockincognito.com");
  PolicyMap policies;
  policies.Set(key::kIncognitoModeUrlBlocklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(incognito_blocklist)), nullptr);

  // Set a general URL allowlist that allows `url`.
  base::ListValue url_allowlist;
  url_allowlist.Append("blockincognito.com");
  policies.Set(key::kURLAllowlist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(std::move(url_allowlist)),
               nullptr);

  UpdateProviderPolicy(policies);
  FlushBlocklistPolicy();

  if (IsBrowserInIncognitoMode()) {
    CheckURLIsBlocked(test_browser, url,
                      /*is_blocked_by_incognito_policy=*/true);
  } else {
    CheckCanOpenURL(test_browser, url);
  }
}

IN_PROC_BROWSER_TEST_P(IncognitoUrlBlockingPolicyTest,
                       UrlBlocklistAndIncognitoAllowlist) {
  // Checks that the Incognito allowlist works as an exception to the
  // URL blocklist, but only in Incognito mode.
  Browser* test_browser;
  if (IsBrowserInIncognitoMode()) {
    test_browser =
        OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  } else {
    test_browser = browser();
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  const auto url =
      embedded_test_server()->GetURL("blocked.com", "/empty.html").spec();

  // Set a general blocklist that blocks `url`.
  base::ListValue url_blocklist;
  url_blocklist.Append("blocked.com");
  PolicyMap policies;
  policies.Set(key::kURLBlocklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(std::move(url_blocklist)),
               nullptr);

  // Set an Incognito allowlist that allows `url`.
  base::ListValue incognito_allowlist;
  incognito_allowlist.Append("blocked.com");
  policies.Set(key::kIncognitoModeUrlAllowlist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(incognito_allowlist)), nullptr);

  UpdateProviderPolicy(policies);
  FlushBlocklistPolicy();

  if (IsBrowserInIncognitoMode()) {
    CheckCanOpenURL(test_browser, url);
  } else {
    CheckURLIsBlocked(test_browser, url,
                      /*is_blocked_by_incognito_policy=*/false);
  }
}

IN_PROC_BROWSER_TEST_P(IncognitoUrlBlockingPolicyTest,
                       UrlBlocklistAndIncognitoBlocklist) {
  // Checks the blocking rules and the resulting error messages for both regular
  // and Incognito mode blocklists.
  Browser* test_browser;
  if (IsBrowserInIncognitoMode()) {
    test_browser =
        OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  } else {
    test_browser = browser();
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  const auto regular_blocked_url =
      embedded_test_server()
          ->GetURL("blockedregular.com", "/empty.html")
          .spec();
  const auto incognito_blocked_url =
      embedded_test_server()
          ->GetURL("blockedincognito.com", "/empty.html")
          .spec();
  const auto both_blocked_url =
      embedded_test_server()->GetURL("blockedboth.com", "/empty.html").spec();

  // Set a  URLBlocklist that blocks `regular_blocked_url`.
  base::ListValue url_blocklist;
  url_blocklist.Append("blockedregular.com");
  url_blocklist.Append("blockedboth.com");
  PolicyMap policies;
  policies.Set(key::kURLBlocklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(std::move(url_blocklist)),
               nullptr);

  // Set an Incognito blocklist that blocks `incognito_blocked_url`.
  base::ListValue incognito_blocklist;
  incognito_blocklist.Append("blockedincognito.com");
  incognito_blocklist.Append("blockedboth.com");
  policies.Set(key::kIncognitoModeUrlBlocklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(incognito_blocklist)), nullptr);

  UpdateProviderPolicy(policies);
  FlushBlocklistPolicy();

  // The regular blocklist blocks `regular_blocked_url` in both regular and
  // Incognito mode with the same message.
  CheckURLIsBlocked(test_browser, regular_blocked_url,
                    /*is_blocked_by_incognito_policy=*/false);

  // The Incognito blocklist blocks `incognito_blocked_url` only in Incognito
  // mode.
  if (IsBrowserInIncognitoMode()) {
    CheckURLIsBlocked(test_browser, incognito_blocked_url,
                      /*is_blocked_by_incognito_policy=*/true);
  } else {
    CheckCanOpenURL(test_browser, incognito_blocked_url);
  }
  // The blocklist blocks `both_blocked_url` in both regular and
  // Incognito mode with the different messages.
  CheckURLIsBlocked(
      test_browser, both_blocked_url,
      /*is_blocked_by_incognito_policy=*/IsBrowserInIncognitoMode());
}

IN_PROC_BROWSER_TEST_F(IncognitoUrlBlockingPolicyTest, IncognitoAllowlistOnly) {
  // Checks that setting only the Incognito allowlist allows specific URLs
  // and blocks others in Incognito mode.
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string allowed_url =
      embedded_test_server()->GetURL("allowed.com", "/empty.html").spec();
  const std::string blocked_url =
      embedded_test_server()->GetURL("blocked.com", "/empty.html").spec();

  // Only set the Incognito allowlist.
  base::ListValue allowlist;
  allowlist.Append("allowed.com");
  PolicyMap policies;
  policies.Set(key::kIncognitoModeUrlAllowlist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allowlist)), nullptr);

  UpdateProviderPolicy(policies);
  FlushBlocklistPolicy();

  CheckCanOpenURL(incognito_browser, allowed_url);
  CheckURLIsBlocked(incognito_browser, blocked_url,
                    /*is_blocked_by_incognito_policy=*/true);
}

IN_PROC_BROWSER_TEST_F(IncognitoUrlBlockingPolicyTest,
                       IncognitoAllowlistAndIncognitoDisabled) {
  // Checks that the Incognito allowlist allows specific URLs even if
  // IncognitoModeAvailability is set to disabled.
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string allowed_url =
      embedded_test_server()->GetURL("allowed.com", "/empty.html").spec();
  const std::string blocked_url =
      embedded_test_server()->GetURL("blocked.com", "/empty.html").spec();

  PolicyMap policies;
  // Disable Incognito mode generally.
  policies.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   policy::IncognitoModeAvailability::kDisabled)),
               nullptr);

  // Set an Incognito allowlist.
  base::ListValue allowlist;
  allowlist.Append("allowed.com");
  policies.Set(key::kIncognitoModeUrlAllowlist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allowlist)), nullptr);

  UpdateProviderPolicy(policies);
  FlushBlocklistPolicy();

  CheckCanOpenURL(incognito_browser, allowed_url);
  CheckURLIsBlocked(incognito_browser, blocked_url,
                    /*is_blocked_by_incognito_policy=*/true);
}

IN_PROC_BROWSER_TEST_F(IncognitoUrlBlockingPolicyTest,
                       IncognitoAllowlistBlocklistAndIncognitoDisabled) {
  // Checks that the Incognito allowlist allows specific URLs even if
  // IncognitoModeAvailability is set to disabled, and blocklist is set.
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string allowed_url =
      embedded_test_server()->GetURL("allowed.com", "/empty.html").spec();
  const std::string blocked_url =
      embedded_test_server()->GetURL("blocked.com", "/empty.html").spec();
  const std::string other_blocked_url =
      embedded_test_server()->GetURL("other.com", "/empty.html").spec();

  PolicyMap policies;
  // Disable Incognito mode generally.
  policies.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   policy::IncognitoModeAvailability::kDisabled)),
               nullptr);

  // Set an Incognito allowlist.
  base::ListValue allowlist;
  allowlist.Append("allowed.com");
  policies.Set(key::kIncognitoModeUrlAllowlist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allowlist)), nullptr);

  // Set an Incognito blocklist.
  base::ListValue blocklist;
  blocklist.Append("blocked.com");
  policies.Set(key::kIncognitoModeUrlBlocklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(blocklist)), nullptr);

  UpdateProviderPolicy(policies);
  FlushBlocklistPolicy();

  CheckCanOpenURL(incognito_browser, allowed_url);
  CheckURLIsBlocked(incognito_browser, blocked_url,
                    /*is_blocked_by_incognito_policy=*/true);
  CheckURLIsBlocked(incognito_browser, other_blocked_url,
                    /*is_blocked_by_incognito_policy=*/true);
}

}  // namespace policy
