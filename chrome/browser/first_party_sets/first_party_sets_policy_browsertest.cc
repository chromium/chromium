// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/frame_test_utils.h"
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

class FirstPartySetsPolicyBrowsertest
    : public PolicyTest,
      public ::testing::WithParamInterface<std::tuple<bool, PrefState>> {
 public:
  FirstPartySetsPolicyBrowsertest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    if (IsFeatureEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(features::kFirstPartySets);
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
    if (GetPrefState() == PrefState::kDefault) {
      return;
    }
    PolicyMap policies;
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_MACHINE - since this policy lives on local state, not per
    // profile
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise
    // policy
    policies.Set(policy::key::kFirstPartySetsEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_MACHINE, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                 base::Value(IsPrefEnabled()), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    if (IsFeatureEnabled()) {
      // Only append this switch when the First-Party Sets base::Feature is
      // enabled.
      command_line->AppendSwitchASCII(
          network::switches::kUseFirstPartySet,
          base::StringPrintf("https://%s,https://%s,https://%s", kHostA, kHostB,
                             kHostC));
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

  net::EmbeddedTestServer* https_server() { return &https_server_; }

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
    // cros-party context.
    if (IsFirstPartySetsEnabled()) {
      return {};
    }
    return kSameSiteNoneCookies;
  }

 private:
  bool IsFeatureEnabled() { return std::get<0>(GetParam()); }
  PrefState GetPrefState() { return std::get<1>(GetParam()); }
  bool IsPrefEnabled() { return GetPrefState() == PrefState::kEnabled; }

  bool IsFirstPartySetsEnabled() {
    if (GetPrefState() == PrefState::kDefault) {
      return IsFeatureEnabled();
    }
    return IsFeatureEnabled() && IsPrefEnabled();
  }

  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SetNoEmbeddedFrameWithFpsMemberTopLevel) {
  // No embedded frame, FPS member.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostA, kSetSamePartyCookiesURL)));
  EXPECT_THAT(GetCanonicalCookies(web_contents()->GetBrowserContext(),
                                  https_server()->GetURL(kHostA, "/")),
              UnorderedPointwise(net::CanonicalCookieNameIs(), kAllCookies));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SetSameSiteFpsMemberEmbed) {
  // Same-site FPS-member iframe (A embedded in A).
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(%s)",
                  SetSamePartyCookiesUrl(kHostA)),
              UnorderedPointwise(net::CanonicalCookieNameIs(), kAllCookies));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SetCrossSiteSamePartyEmbedWithFpsOwnerTopLevel) {
  // Cross-site, same-party iframe (B embedded in A).
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(%s)",
                  SetSamePartyCookiesUrl(kHostB)),
              UnorderedPointwise(net::CanonicalCookieNameIs(),
                                 ExpectedCrossSiteCookiesInSamePartyContext()));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SetCrossSiteSamePartyEmbedWithFpsOwnerLeaf) {
  // Cross-site, same-party iframe (A embedded in B).
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "b.test(%s)",
                  SetSamePartyCookiesUrl(kHostA)),
              UnorderedPointwise(net::CanonicalCookieNameIs(),
                                 ExpectedCrossSiteCookiesInSamePartyContext()));
}

IN_PROC_BROWSER_TEST_P(
    FirstPartySetsPolicyBrowsertest,
    SetCrossSiteSamePartyWithTwoNestedSamePartyContextFrames) {
  // Cross-site, same-party nested iframe (A embedded in B embedded in A).
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(b.test(%s))",
                  SetSamePartyCookiesUrl(kHostA)),
              UnorderedPointwise(net::CanonicalCookieNameIs(),
                                 ExpectedCrossSiteCookiesInSamePartyContext()));
}

IN_PROC_BROWSER_TEST_P(
    FirstPartySetsPolicyBrowsertest,
    SetCrossSiteSamePartyWithThreeNestedSamePartyContextFrames) {
  // Cross-site, same-party nested iframe (A embedded in B embedded in C
  // embedded in A).
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(c.test(b.test(%s)))",
                  SetSamePartyCookiesUrl(kHostA)),
              UnorderedPointwise(net::CanonicalCookieNameIs(),
                                 ExpectedCrossSiteCookiesInSamePartyContext()));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SetCrossSiteCrossPartyEmbedWithFpsTopLevel) {
  // Cross-site, cross-party iframe (D embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "a.test(%s)",
          SetSamePartyCookiesUrl(kHostD)),
      UnorderedPointwise(net::CanonicalCookieNameIs(), kSameSiteNoneCookies));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SetCrossSiteCrossPartyEmbedWithFpsLeaf) {
  // Cross-site, cross-party iframe (A embedded in D).
  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "d.test(%s)",
          SetSamePartyCookiesUrl(kHostA)),
      UnorderedPointwise(net::CanonicalCookieNameIs(),
                         ExpectedCrossSiteCookiesInCrossPartyContext()));
}

IN_PROC_BROWSER_TEST_P(
    FirstPartySetsPolicyBrowsertest,
    SetCrossSiteCrossPartyWithTwoNestedCrossPartyContextFrames) {
  // Cross-site, cross-party nested iframe (A embedded in B embedded in D).
  EXPECT_THAT(
      content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
          web_contents(), https_server(), "d.test(b.test(%s))",
          SetSamePartyCookiesUrl(kHostA)),
      UnorderedPointwise(net::CanonicalCookieNameIs(),
                         ExpectedCrossSiteCookiesInCrossPartyContext()));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SetNoEmbeddedFrameWithNonFpsMemberTopLevel) {
  // No embedded frame, non-FPS member.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostD, kSetSamePartyCookiesURL)));
  EXPECT_THAT(GetCanonicalCookies(web_contents()->GetBrowserContext(),
                                  https_server()->GetURL(kHostD, "/")),
              UnorderedPointwise(net::CanonicalCookieNameIs(), kAllCookies));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SendNoEmbeddedFrameWithFpsMemberTopLevel) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // No embedded frame, FPS member.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostA, "/echoheader?Cookie")));
  EXPECT_THAT(
      ExtractFrameContent(web_contents()->GetMainFrame()),
      net::CookieStringIs(UnorderedPointwise(net::NameIs(), kAllCookies)));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SendSameSiteWithFpsMemberEmbed) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // Same-site FPS-member iframe (A embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(web_contents(),
                                                  https_server(), "a.test(%s)",
                                                  {0}, EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(net::NameIs(), kAllCookies)));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SendCrossSiteSamePartyWithEmbedFpsOwnerTopLevel) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostB));
  // Cross-site, same-party iframe (B embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(web_contents(),
                                                  https_server(), "a.test(%s)",
                                                  {0}, EchoCookiesUrl(kHostB)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInSamePartyContext())));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SendCrossSiteSamePartyWithEmbedFpsOwnerLeaf) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // Cross-site, same-party iframe (A embedded in B).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(web_contents(),
                                                  https_server(), "b.test(%s)",
                                                  {0}, EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInSamePartyContext())));
}

IN_PROC_BROWSER_TEST_P(
    FirstPartySetsPolicyBrowsertest,
    SendCrossSiteSamePartyWithTwoNestedSamePartyContextFrames) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // Cross-site, same-party nested iframe (A embedded in B embedded in A).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(), "a.test(b.test(%s))", {0, 0},
          EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInSamePartyContext())));
}

IN_PROC_BROWSER_TEST_P(
    FirstPartySetsPolicyBrowsertest,
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
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SendCrossSiteCrossPartyEmbedWithFpsTopLevel) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostD));
  // Cross-site, cross-party iframe (D embedded in A).
  EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                  web_contents(), https_server(), "a.test(%s)", {0},
                  EchoCookiesUrl(kHostD)),
              net::CookieStringIs(
                  UnorderedPointwise(net::NameIs(), kSameSiteNoneCookies)));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SendCrossSiteCrossPartyEmbedWithFpsLeaf) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // Cross-site, cross-party iframe (A embedded in D).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(web_contents(),
                                                  https_server(), "d.test(%s)",
                                                  {0}, EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInCrossPartyContext())));
}

IN_PROC_BROWSER_TEST_P(
    FirstPartySetsPolicyBrowsertest,
    SendCrossSiteCrossPartyWithTwoNestedCrossPartyContextFrames) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostA));
  // Cross-site, cross-party nested iframe (A embedded in B embedded in D).
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(), "d.test(b.test(%s))", {0, 0},
          EchoCookiesUrl(kHostA)),
      net::CookieStringIs(UnorderedPointwise(
          net::NameIs(), ExpectedCrossSiteCookiesInCrossPartyContext())));
}

IN_PROC_BROWSER_TEST_P(FirstPartySetsPolicyBrowsertest,
                       SendNoEmbeddedFrameWithNonFpsMemberTopLevel) {
  ASSERT_NO_FATAL_FAILURE(SetSamePartyCookies(kHostD));
  // No embedded frame, non-FPS member.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostD, "/echoheader?Cookie")));
  EXPECT_THAT(
      ExtractFrameContent(web_contents()->GetMainFrame()),
      net::CookieStringIs(UnorderedPointwise(net::NameIs(), kAllCookies)));
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    FirstPartySetsPolicyBrowsertest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));
}  // namespace
}  // namespace policy
