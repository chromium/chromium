// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/frame_test_utils.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

using ::testing::UnorderedPointwise;

enum PrefState { kDefault, kDisabled, kEnabled };

const char* kHostA = "a.test";
const char* kHostB = "b.test";
const char* kHostC = "c.test";
const char* kHostD = "d.test";
const char* kSamePartyLaxCookieName = "sameparty_lax_cookie";
const char* kSamePartyNoneCookieName = "sameparty_none_cookie";
const char* kSamePartyUnspecifiedCookieName = "sameparty_unspecified_cookie";
const std::string kSetSamePartyCookiesURL = base::StrCat({
    "/set-cookie?",
    kSamePartyLaxCookieName,
    "=1;SameParty;Secure;SameSite=Lax&",
    kSamePartyNoneCookieName,
    "=1;SameParty;Secure;SameSite=None&",
    kSamePartyUnspecifiedCookieName,
    "=1;SameParty;Secure",
});
const std::vector<std::string> kAllCookies = {kSamePartyLaxCookieName,
                                              kSamePartyNoneCookieName,
                                              kSamePartyUnspecifiedCookieName};
const std::vector<std::string> kSameSiteNoneCookies = {
    kSamePartyNoneCookieName};

class EnabledPolicyBrowsertest
    : public PolicyTest,
      public ::testing::WithParamInterface<std::tuple<bool, PrefState>> {
 public:
  EnabledPolicyBrowsertest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        pref_enabled_(GetPrefState() != PrefState::kDisabled) {
    if (IsFeatureEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kFirstPartySets,
           net::features::kSamePartyAttributeEnabled},
          {});
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kFirstPartySets);
    }
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    // Add content/test/data for cross_site_iframe_factory.html
    https_server()->ServeFilesFromSourceDirectory("content/test/data");
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    SetUpPolicyMapWithOverridesPolicy();
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_USER - since this policy is per profile, not on local state
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise policy
    if (GetPrefState() != PrefState::kDefault) {
      policies_.Set(policy::key::kFirstPartySetsEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                    base::Value(IsPrefEnabled()), nullptr);
    }

    provider_.UpdateChromePolicy(policies_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    if (IsFeatureEnabled()) {
      // Only append this switch when the First-Party Sets base::Feature is
      // enabled.
      command_line->AppendSwitchASCII(
          network::switches::kUseFirstPartySet,
          base::StringPrintf(
              R"({"primary": "https://%s",)"
              R"("associatedSites": ["https://%s","https://%s"]})",
              kHostA, kHostB, kHostC));
    }
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SetSamePartyCookies(const std::string& host) {
    content::BrowserContext* context = web_contents()->GetBrowserContext();
    ASSERT_TRUE(content::SetCookie(
        context, https_server()->GetURL(host, "/"),
        base::StrCat(
            {kSamePartyLaxCookieName, "=1; SameSite=Lax; Secure; SameParty"})));
    ASSERT_TRUE(content::SetCookie(
        context, https_server()->GetURL(host, "/"),
        base::StrCat({kSamePartyNoneCookieName,
                      "=1; SameSite=None; Secure; SameParty"})));
    ASSERT_TRUE(
        content::SetCookie(context, https_server()->GetURL(host, "/"),
                           base::StrCat({kSamePartyUnspecifiedCookieName,
                                         "=1; Secure; SameParty"})));
  }

  GURL EchoCookiesUrl(const std::string& host) {
    return https_server()->GetURL(host, "/echoheader?Cookie");
  }

  GURL SetSamePartyCookiesUrl(const std::string& host) {
    return https_server()->GetURL(host, kSetSamePartyCookiesURL);
  }

  std::string ExtractFrameContent(content::RenderFrameHost* frame) const {
    return content::EvalJs(frame, "document.body.textContent").ExtractString();
  }

  virtual void SetUpPolicyMapWithOverridesPolicy() {}

  net::EmbeddedTestServer* https_server() { return &https_server_; }
  PolicyMap& policy_map() { return policies_; }

  std::vector<std::string> ExpectedCrossSiteCookiesInSamePartyContext() {
    // Returns the expected cookies that are accessible in a cross-site,
    // same-party context.
    if (IsFirstPartySetsEnabled()) {
      return kAllCookies;
    }
    return kSameSiteNoneCookies;
  }

  std::vector<std::string> ExpectedCrossSiteCookiesInCrossPartyContext() {
    // Returns the expected cookies that are accessible in a cross-site,
    // cross-party context.
    if (IsFirstPartySetsEnabled()) {
      return {};
    }
    return kSameSiteNoneCookies;
  }

  // Reverses the state of the First-Party Sets enabled preference.
  void FlipEnabledPolicy() {
    pref_enabled_ = !pref_enabled_;
    policy_map().Set(policy::key::kFirstPartySetsEnabled,
                     POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                     POLICY_SOURCE_ENTERPRISE_DEFAULT,
                     base::Value(pref_enabled_), nullptr);

    provider_.UpdateChromePolicy(policy_map());
  }

  bool IsFirstPartySetsEnabled() {
    return IsFeatureEnabled() && IsPrefEnabled();
  }

  // Clear cookies for the current browser context, returning the number
  // cleared.
  uint32_t ClearCookies() {
    return content::DeleteCookies(web_contents()->GetBrowserContext(),
                                  network::mojom::CookieDeletionFilter());
  }

 private:
  bool IsFeatureEnabled() { return std::get<0>(GetParam()); }
  PrefState GetPrefState() { return std::get<1>(GetParam()); }
  bool IsPrefEnabled() { return pref_enabled_; }

  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  PolicyMap policies_;
  bool pref_enabled_;
};

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SetNoEmbeddedFrameWithFpsAssociatedSiteTopLevel) {
  // No embedded frame, FPS associated site.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostA, kSetSamePartyCookiesURL)));
  EXPECT_THAT(GetCanonicalCookies(web_contents()->GetBrowserContext(),
                                  https_server()->GetURL(kHostA, "/")),
              UnorderedPointwise(net::CanonicalCookieNameIs(), kAllCookies));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SetSameSiteFpsAssociatedSiteEmbed) {
  // Same-site FPS-associated-site iframe (A embedded in A).
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(%s)",
                  SetSamePartyCookiesUrl(kHostA)),
              UnorderedPointwise(net::CanonicalCookieNameIs(), kAllCookies));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SetCrossSiteSamePartyEmbedWithFpsPrimaryTopLevel) {
  std::vector<std::string> expected_cookies =
      ExpectedCrossSiteCookiesInSamePartyContext();
  // Cross-site, same-party iframe (B embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "a.test(%s)",
          SetSamePartyCookiesUrl(kHostB)),
      UnorderedPointwise(net::CanonicalCookieNameIs(), expected_cookies));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), expected_cookies.size());
  FlipEnabledPolicy();

  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(%s)",
                  SetSamePartyCookiesUrl(kHostB)),
              UnorderedPointwise(net::CanonicalCookieNameIs(),
                                 ExpectedCrossSiteCookiesInSamePartyContext()));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SetCrossSiteSamePartyEmbedWithFpsPrimaryLeaf) {
  std::vector<std::string> expected_cookies =
      ExpectedCrossSiteCookiesInSamePartyContext();
  // Cross-site, same-party iframe (A embedded in B).
  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "b.test(%s)",
          SetSamePartyCookiesUrl(kHostA)),
      UnorderedPointwise(net::CanonicalCookieNameIs(), expected_cookies));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), expected_cookies.size());
  FlipEnabledPolicy();

  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "b.test(%s)",
                  SetSamePartyCookiesUrl(kHostA)),
              UnorderedPointwise(net::CanonicalCookieNameIs(),
                                 ExpectedCrossSiteCookiesInSamePartyContext()));
}

IN_PROC_BROWSER_TEST_P(
    EnabledPolicyBrowsertest,
    SetCrossSiteSamePartyWithTwoNestedSamePartyContextFrames) {
  std::vector<std::string> expected_cookies =
      ExpectedCrossSiteCookiesInSamePartyContext();
  // Cross-site, same-party nested iframe (A embedded in B embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "a.test(b.test(%s))",
          SetSamePartyCookiesUrl(kHostA)),
      UnorderedPointwise(net::CanonicalCookieNameIs(), expected_cookies));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), expected_cookies.size());
  FlipEnabledPolicy();

  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(b.test(%s))",
                  SetSamePartyCookiesUrl(kHostA)),
              UnorderedPointwise(net::CanonicalCookieNameIs(),
                                 ExpectedCrossSiteCookiesInSamePartyContext()));
}

IN_PROC_BROWSER_TEST_P(
    EnabledPolicyBrowsertest,
    SetCrossSiteSamePartyWithThreeNestedSamePartyContextFrames) {
  std::vector<std::string> expected_cookies =
      ExpectedCrossSiteCookiesInSamePartyContext();
  // Cross-site, same-party nested iframe (A embedded in B embedded in C
  // embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "a.test(c.test(b.test(%s)))",
          SetSamePartyCookiesUrl(kHostA)),
      UnorderedPointwise(net::CanonicalCookieNameIs(), expected_cookies));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), expected_cookies.size());
  FlipEnabledPolicy();

  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(c.test(b.test(%s)))",
                  SetSamePartyCookiesUrl(kHostA)),
              UnorderedPointwise(net::CanonicalCookieNameIs(),
                                 ExpectedCrossSiteCookiesInSamePartyContext()));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SetCrossSiteCrossPartyEmbedWithFpsTopLevel) {
  // Cross-site, cross-party iframe (D embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "a.test(%s)",
          SetSamePartyCookiesUrl(kHostD)),
      UnorderedPointwise(net::CanonicalCookieNameIs(), kSameSiteNoneCookies));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SetCrossSiteCrossPartyEmbedWithFpsLeaf) {
  std::vector<std::string> expected_cookies =
      ExpectedCrossSiteCookiesInCrossPartyContext();
  // Cross-site, cross-party iframe (A embedded in D).
  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "d.test(%s)",
          SetSamePartyCookiesUrl(kHostA)),
      UnorderedPointwise(net::CanonicalCookieNameIs(), expected_cookies));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), expected_cookies.size());
  FlipEnabledPolicy();

  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "d.test(%s)",
          SetSamePartyCookiesUrl(kHostA)),
      UnorderedPointwise(net::CanonicalCookieNameIs(),
                         ExpectedCrossSiteCookiesInCrossPartyContext()));
}

IN_PROC_BROWSER_TEST_P(
    EnabledPolicyBrowsertest,
    SetCrossSiteCrossPartyWithTwoNestedCrossPartyContextFrames) {
  std::vector<std::string> expected_cookies =
      ExpectedCrossSiteCookiesInCrossPartyContext();
  // Cross-site, cross-party nested iframe (A embedded in B embedded in
  // D).
  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "d.test(b.test(%s))",
          SetSamePartyCookiesUrl(kHostA)),
      UnorderedPointwise(net::CanonicalCookieNameIs(),
                         ExpectedCrossSiteCookiesInCrossPartyContext()));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), expected_cookies.size());
  FlipEnabledPolicy();

  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "d.test(b.test(%s))",
          SetSamePartyCookiesUrl(kHostA)),
      UnorderedPointwise(net::CanonicalCookieNameIs(),
                         ExpectedCrossSiteCookiesInCrossPartyContext()));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SetNoEmbeddedFrameWithNonFpsAssociatedSiteTopLevel) {
  // No embedded frame, non-FPS site.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostD, kSetSamePartyCookiesURL)));
  EXPECT_THAT(GetCanonicalCookies(web_contents()->GetBrowserContext(),
                                  https_server()->GetURL(kHostD, "/")),
              UnorderedPointwise(net::CanonicalCookieNameIs(), kAllCookies));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SendNoEmbeddedFrameWithFpsAssociatedSiteTopLevel) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // No embedded frame, FPS associated site.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostA, "/echoheader?Cookie")));
  EXPECT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedPointwise(net::NameIs(), kAllCookies)));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SendSameSiteWithFpsAssociatedSiteEmbed) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // Same-site FPS-associated-site iframe (A embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(web_contents(),
                                                  https_server(), "a.test(%s)",
                                                  {0}, EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(net::NameIs(), kAllCookies)));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SendCrossSiteSamePartyWithEmbedFpsPrimaryTopLevel) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostB));
  // Cross-site, same-party iframe (B embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(web_contents(),
                                                  https_server(), "a.test(%s)",
                                                  {0}, EchoCookiesUrl(kHostB)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInSamePartyContext())));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), 3u);
  FlipEnabledPolicy();

  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostB));
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(web_contents(),
                                                  https_server(), "a.test(%s)",
                                                  {0}, EchoCookiesUrl(kHostB)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInSamePartyContext())));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SendCrossSiteSamePartyWithEmbedFpsPrimaryLeaf) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // Cross-site, same-party iframe (A embedded in B).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(web_contents(),
                                                  https_server(), "b.test(%s)",
                                                  {0}, EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInSamePartyContext())));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), 3u);
  FlipEnabledPolicy();

  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(web_contents(),
                                                  https_server(), "b.test(%s)",
                                                  {0}, EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInSamePartyContext())));
}

IN_PROC_BROWSER_TEST_P(
    EnabledPolicyBrowsertest,
    SendCrossSiteSamePartyWithTwoNestedSamePartyContextFrames) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // Cross-site, same-party nested iframe (A embedded in B embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(), "a.test(b.test(%s))", {0, 0},
          EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInSamePartyContext())));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), 3u);
  FlipEnabledPolicy();

  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(), "a.test(b.test(%s))", {0, 0},
          EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInSamePartyContext())));
}

IN_PROC_BROWSER_TEST_P(
    EnabledPolicyBrowsertest,
    SendCrossSiteSamePartyWithThreeNestedSamePartyContextFrames) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // Cross-site, same-party nested iframe (A embedded in B embedded in C
  // embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(), "a.test(c.test(b.test(%s)))",
          {0, 0, 0}, EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInSamePartyContext())));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), 3u);
  FlipEnabledPolicy();

  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(), "a.test(c.test(b.test(%s)))",
          {0, 0, 0}, EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInSamePartyContext())));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SendCrossSiteCrossPartyEmbedWithFpsTopLevel) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostD));
  // Cross-site, cross-party iframe (D embedded in A).
  EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                  web_contents(), https_server(), "a.test(%s)", {0},
                  EchoCookiesUrl(kHostD)),
              net::CookieStringIs(
                  UnorderedPointwise(net::NameIs(), kSameSiteNoneCookies)));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SendCrossSiteCrossPartyEmbedWithFpsLeaf) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // Cross-site, cross-party iframe (A embedded in D).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(web_contents(),
                                                  https_server(), "d.test(%s)",
                                                  {0}, EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInCrossPartyContext())));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), 3u);
  FlipEnabledPolicy();

  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(web_contents(),
                                                  https_server(), "d.test(%s)",
                                                  {0}, EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInCrossPartyContext())));
}

IN_PROC_BROWSER_TEST_P(
    EnabledPolicyBrowsertest,
    SendCrossSiteCrossPartyWithTwoNestedCrossPartyContextFrames) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // Cross-site, cross-party nested iframe (A embedded in B embedded in D).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(), "d.test(b.test(%s))", {0, 0},
          EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInCrossPartyContext())));

  // Clear cookies from above and flip the First-Party Sets enabled pref.
  ASSERT_EQ(ClearCookies(), 3u);
  FlipEnabledPolicy();

  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(), "d.test(b.test(%s))", {0, 0},
          EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInCrossPartyContext())));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       SendNoEmbeddedFrameWithNonFpsAssociatedSiteTopLevel) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostD));
  // No embedded frame, non-FPS site.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostD, "/echoheader?Cookie")));
  EXPECT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedPointwise(net::NameIs(), kAllCookies)));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                       DefaultOverridesPolicy_SetCookiesFromSamePartyContext) {
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: A, associatedSites: [B, C]} (unchanged)
  //
  // `A` should still be able to set its cookies from a cross-site, same-party
  // nested iframe (A embedded in B embedded in C embedded in A).
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(c.test(b.test(%s)))",
                  SetSamePartyCookiesUrl(kHostA)),
              UnorderedPointwise(net::CanonicalCookieNameIs(),
                                 ExpectedCrossSiteCookiesInSamePartyContext()));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    EnabledPolicyBrowsertest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));

class OverridesPolicyEmptyBrowsertest : public EnabledPolicyBrowsertest {
 public:
  void SetUpPolicyMapWithOverridesPolicy() override {
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_USER - since this policy is per profile, not on local state
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise
    // policy
    policy_map().Set(policy::key::kFirstPartySetsOverrides,
                     POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                     POLICY_SOURCE_ENTERPRISE_DEFAULT,
                     base::JSONReader::Read(R"( {} )"), nullptr);
  }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyEmptyBrowsertest,
                       SetCookiesFromSamePartyContext) {
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: A, associatedSites: [B, C]} (unchanged)
  //
  // `A` should still be able to set its cookies from a cross-site, same-party
  // nested iframe (A embedded in B embedded in C embedded in A).
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(c.test(b.test(%s)))",
                  SetSamePartyCookiesUrl(kHostA)),
              UnorderedPointwise(net::CanonicalCookieNameIs(),
                                 ExpectedCrossSiteCookiesInSamePartyContext()));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyEmptyBrowsertest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));

class OverridesPolicyReplacementBrowsertest : public EnabledPolicyBrowsertest {
 public:
  void SetUpPolicyMapWithOverridesPolicy() override {
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_USER - since this policy is per profile, not on local state
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise
    // policy
    policy_map().Set(policy::key::kFirstPartySetsOverrides,
                     POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                     POLICY_SOURCE_ENTERPRISE_DEFAULT,
                     base::JSONReader::Read(R"(
                              {
                                "replacements": [
                                  {
                                    "primary": "https://d.test",
                                    "associatedSites": ["https://b.test",
                                    "https://a.test"]
                                  }
                                ],
                                "additions": []
                              }
                            )"),
                     nullptr);
  }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyReplacementBrowsertest,
                       SetCookiesFromSamePartyContext) {
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: D, associatedSites: [A, B]}

  {  // `D` should now be able to set its cookies from a cross-site, same-party
    // nested iframe (D embedded in B embedded in A embedded in D).
    EXPECT_THAT(
        content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
            web_contents(), https_server(), "d.test(a.test(b.test(%s)))",
            SetSamePartyCookiesUrl(kHostD)),
        UnorderedPointwise(net::CanonicalCookieNameIs(),
                           ExpectedCrossSiteCookiesInSamePartyContext()));
  }
  {  // `A` should no longer be able to set its cookies from the cross-site
     // nested iframe (A embedded in B embedded in C) since it's no longer in
     // the same First-Party Set as those sites.
    EXPECT_THAT(
        content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
            web_contents(), https_server(), "c.test(b.test(%s))",
            SetSamePartyCookiesUrl(kHostA)),
        UnorderedPointwise(net::CanonicalCookieNameIs(),
                           ExpectedCrossSiteCookiesInCrossPartyContext()));
  }
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyReplacementBrowsertest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));

class OverridesPolicyAdditionBrowsertest : public EnabledPolicyBrowsertest {
 public:
  void SetUpPolicyMapWithOverridesPolicy() override {
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_USER - since this policy is per profile, not on local state
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise
    // policy
    policy_map().Set(policy::key::kFirstPartySetsOverrides,
                     POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                     POLICY_SOURCE_ENTERPRISE_DEFAULT,
                     base::JSONReader::Read(R"(
                              {
                                "replacements": [],
                                "additions": [
                                  {
                                    "primary": "https://a.test",
                                    "associatedSites": ["https://d.test"]
                                  }
                                ]
                              }
                            )"),
                     nullptr);
  }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyAdditionBrowsertest,
                       SetCookiesFromSamePartyContext) {
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: A, associatedSites: [B, C, D]}}
  //
  // `D` should now be able to set its cookies from a cross-site, same-party
  // nested iframe (D embedded in B embedded in A embedded in C).
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "c.test(a.test(b.test(%s)))",
                  SetSamePartyCookiesUrl(kHostD)),
              UnorderedPointwise(net::CanonicalCookieNameIs(),
                                 ExpectedCrossSiteCookiesInSamePartyContext()));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyAdditionBrowsertest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));

class OverridesPolicyReplacementAndAdditionBrowsertest
    : public EnabledPolicyBrowsertest {
 public:
  void SetUpPolicyMapWithOverridesPolicy() override {
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_USER - since this policy is per profile, not on local state
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise
    // policy
    policy_map().Set(policy::key::kFirstPartySetsOverrides,
                     POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                     POLICY_SOURCE_ENTERPRISE_DEFAULT,
                     base::JSONReader::Read(R"(
                              {
                                "replacements": [
                                  {
                                    "primary": "https://a.test",
                                    "associatedSites": ["https://d.test"]
                                  }
                                ],
                                "additions": [
                                  {
                                    "primary": "https://b.test",
                                    "associatedSites": ["https://c.test"]
                                  }
                                ]
                              }
                            )"),
                     nullptr);
  }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyReplacementAndAdditionBrowsertest,
                       SetCookiesFromSamePartyContext) {
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: A, associatedSites: [D]} and {primary: B, associatedSites: [C]}.

  {  // `A` and `B` are no longer in the same First-Party Set so `A` should no
     // longer be able to set its cookies from a nested iframe in B.
    EXPECT_THAT(
        content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
            web_contents(), https_server(), "b.test(%s)",
            SetSamePartyCookiesUrl(kHostA)),
        UnorderedPointwise(net::CanonicalCookieNameIs(),
                           ExpectedCrossSiteCookiesInCrossPartyContext()));
  }

  {  // Both `A` and `B` should be able to set their cookies from a cross-site,
    // same-party nested iframe (A embedded in D) and (B embedded in C),
    // respectively.
    EXPECT_THAT(
        content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
            web_contents(), https_server(), "d.test(%s)",
            SetSamePartyCookiesUrl(kHostA)),
        UnorderedPointwise(net::CanonicalCookieNameIs(),
                           ExpectedCrossSiteCookiesInSamePartyContext()));
    EXPECT_THAT(
        content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
            web_contents(), https_server(), "c.test(%s)",
            SetSamePartyCookiesUrl(kHostB)),
        UnorderedPointwise(net::CanonicalCookieNameIs(),
                           ExpectedCrossSiteCookiesInSamePartyContext()));
  }
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyReplacementAndAdditionBrowsertest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));
}  // namespace
}  // namespace policy
