// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::Key;
using testing::Pair;
using testing::UnorderedElementsAre;

const char* kSamePartySameSiteLaxCookieName = "sameparty_samesite_lax_cookie";
const char* kSamePartySameSiteNoneCookieName = "sameparty_samesite_none_cookie";
const char* kSamePartySameSiteUnspecifiedCookieName =
    "sameparty_samesite_unspecified_cookie";
const char* kSameSiteNoneCookieName = "samesite_none_cookie";

class CookieStoreSamePartyTest : public InProcessBrowserTest {
 public:
  explicit CookieStoreSamePartyTest(bool enable_fps)
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        enable_fps_(enable_fps) {
    if (!enable_fps) {
      feature_list_.InitAndDisableFeature(features::kFirstPartySets);
    }
    // If FPS is to be enabled, that happens in `SetUpCommandLine` when the
    // `kUseFirstPartySet` switch is provided.
  }

  CookieStoreSamePartyTest(const CookieStoreSamePartyTest&) = delete;
  CookieStoreSamePartyTest& operator=(const CookieStoreSamePartyTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    if (enable_fps_) {
      command_line->AppendSwitchASCII(
          network::switches::kUseFirstPartySet,
          R"({"primary": "https://a.test",)"
          R"("associatedSites": ["https://b.test"]})");
    }
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(https_server_.Start());
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* GetDescendantFrame(
      const std::vector<int>& indices) {
    content::RenderFrameHost* frame = contents()->GetPrimaryMainFrame();
    for (int index : indices) {
      frame = ChildFrameAt(frame, index);
    }
    return frame;
  }

  std::string GetCookiesViaCookieStore(content::RenderFrameHost* frame) {
    std::string script = R"(
      (async () => {
          const cookies = await cookieStore.getAll();
          return cookies.map(c => `${c.name}=${c.value}`).join(';');
      })();
    )";
    return content::EvalJs(frame, script).ExtractString();
  }

  std::string GetSamePartyAttributesViaCookieStore(
      content::RenderFrameHost* frame) {
    return content::EvalJs(frame, R"(
      (async () => {
          const cookies = await cookieStore.getAll();
          return cookies.map(c => `${c.name}=${c.sameParty}`).join(';');
      })();
    )")
        .ExtractString();
  }

  void SetSamePartyCookies(const std::string& host) {
    Profile* profile = browser()->profile();
    ASSERT_TRUE(content::SetCookie(
        profile, https_server().GetURL(host, "/"),
        base::StrCat({kSamePartySameSiteLaxCookieName,
                      "=1; samesite=lax; secure; sameparty"})));
    ASSERT_TRUE(content::SetCookie(
        profile, https_server().GetURL(host, "/"),
        base::StrCat({kSamePartySameSiteNoneCookieName,
                      "=1; samesite=none; secure; sameparty"})));
    ASSERT_TRUE(content::SetCookie(
        profile, https_server().GetURL(host, "/"),
        base::StrCat({kSamePartySameSiteUnspecifiedCookieName,
                      "=1; secure; sameparty"})));
    ASSERT_TRUE(content::SetCookie(
        profile, https_server().GetURL(host, "/"),
        base::StrCat({kSameSiteNoneCookieName, "=1; samesite=none; secure"})));
  }

  std::string SetSamePartyCookieString(base::StringPiece name,
                                       bool same_party,
                                       base::StringPiece same_site) {
    return content::JsReplace(
        R"(
          (async () => {
            await cookieStore.set({name: $1, value: '1',
                sameParty: $2, sameSite: $3});
            return true;
          })();
        )",
        name, same_party, same_site);
  }

  void SetSamePartyCookiesViaCookieStore(content::RenderFrameHost* frame,
                                         bool is_same_party_context,
                                         bool site_is_in_fps) {
    if (is_same_party_context) {
      ASSERT_TRUE(site_is_in_fps);
    }

    {
      content::EvalJsResult result = content::EvalJs(
          frame, SetSamePartyCookieString(kSamePartySameSiteLaxCookieName, true,
                                          "lax"));
      if (!is_same_party_context || !site_is_in_fps) {
        // If the site is in an FPS and the context isn't same-party, the set
        // will fail; otherwise, if the context is cross-site, the set will
        // fail. We assume any call is from a cross-site context, so therefore
        // any call where the site is not in an FPS should fail.
        ASSERT_FALSE(result.error.empty());
      } else {
        ASSERT_TRUE(result.ExtractBool());
      }
    }

    {
      content::EvalJsResult result = content::EvalJs(
          frame, SetSamePartyCookieString(kSamePartySameSiteNoneCookieName,
                                          true, "none"));
      if (site_is_in_fps && !is_same_party_context) {
        ASSERT_FALSE(result.error.empty());
      } else {
        ASSERT_TRUE(result.ExtractBool());
      }
    }

    // Can't test unspecified SameSite here, since the CookieStore supplies
    // 'Strict' when SameSite is not provided explicitly.

    ASSERT_TRUE(
        content::EvalJs(frame, SetSamePartyCookieString(kSameSiteNoneCookieName,
                                                        false, "none"))
            .ExtractBool());
  }

  const net::test_server::EmbeddedTestServer& https_server() const {
    return https_server_;
  }

 private:
  net::test_server::EmbeddedTestServer https_server_;
  bool enable_fps_;
  base::test::ScopedFeatureList feature_list_;
};

class CookieStoreSamePartyEnabledTest : public CookieStoreSamePartyTest {
 public:
  CookieStoreSamePartyEnabledTest() : CookieStoreSamePartyTest(true) {}
};

IN_PROC_BROWSER_TEST_F(CookieStoreSamePartyEnabledTest,
                       ReadCookies_SamePartyContext) {
  SetSamePartyCookies("b.test");
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(b.test)"}))));

  EXPECT_THAT(GetCookiesViaCookieStore(GetDescendantFrame({0})),
              net::CookieStringIs(UnorderedElementsAre(
                  Key(kSamePartySameSiteLaxCookieName),
                  Key(kSamePartySameSiteNoneCookieName),
                  Key(kSamePartySameSiteUnspecifiedCookieName),
                  Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(CookieStoreSamePartyEnabledTest,
                       ReadCookies_CrossPartyContext) {
  SetSamePartyCookies("c.test");
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(c.test)"}))));

  // c.test isn't in the FPS, so SameParty is ignored, so we get SameSite=None
  // cookies.
  EXPECT_THAT(GetCookiesViaCookieStore(GetDescendantFrame({0})),
              net::CookieStringIs(
                  UnorderedElementsAre(Key(kSamePartySameSiteNoneCookieName),
                                       Key(kSameSiteNoneCookieName))));

  ASSERT_EQ(4U, content::DeleteCookies(browser()->profile(),
                                       network::mojom::CookieDeletionFilter()));

  SetSamePartyCookies("b.test");
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(c.test(b.test))"}))));

  // SameParty is not ignored, so we get the SameSite=None cookie.
  EXPECT_THAT(
      GetCookiesViaCookieStore(GetDescendantFrame({0, 0})),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(CookieStoreSamePartyEnabledTest,
                       ReadAttribute_SamePartyContext) {
  SetSamePartyCookies("b.test");
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(b.test)"}))));

  EXPECT_THAT(GetSamePartyAttributesViaCookieStore(GetDescendantFrame({0})),
              net::CookieStringIs(UnorderedElementsAre(
                  Pair(kSamePartySameSiteLaxCookieName, "true"),
                  Pair(kSamePartySameSiteNoneCookieName, "true"),
                  Pair(kSamePartySameSiteUnspecifiedCookieName, "true"),
                  Pair(kSameSiteNoneCookieName, "false"))));
}

IN_PROC_BROWSER_TEST_F(CookieStoreSamePartyEnabledTest,
                       ReadAttribute_CrossPartyContext) {
  SetSamePartyCookies("c.test");
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(c.test)"}))));

  EXPECT_THAT(GetSamePartyAttributesViaCookieStore(GetDescendantFrame({0})),
              net::CookieStringIs(UnorderedElementsAre(
                  Pair(kSamePartySameSiteNoneCookieName, "true"),
                  Pair(kSameSiteNoneCookieName, "false"))));

  ASSERT_EQ(4U, content::DeleteCookies(browser()->profile(),
                                       network::mojom::CookieDeletionFilter()));

  SetSamePartyCookies("b.test");
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(c.test(b.test))"}))));

  EXPECT_THAT(GetSamePartyAttributesViaCookieStore(GetDescendantFrame({0, 0})),
              net::CookieStringIs(UnorderedElementsAre(
                  Pair(kSameSiteNoneCookieName, "false"))));
}

IN_PROC_BROWSER_TEST_F(CookieStoreSamePartyEnabledTest,
                       WriteCookies_SamePartyContext) {
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(b.test)"}))));
  SetSamePartyCookiesViaCookieStore(GetDescendantFrame({0}),
                                    /*is_same_party_context=*/true,
                                    /*site_is_in_fps=*/true);

  EXPECT_THAT(content::GetCookies(browser()->profile(), GURL("https://b.test")),
              net::CookieStringIs(
                  UnorderedElementsAre(Key(kSamePartySameSiteLaxCookieName),
                                       Key(kSamePartySameSiteNoneCookieName),
                                       Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(CookieStoreSamePartyEnabledTest,
                       WriteCookies_CrossPartyContext) {
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(c.test)"}))));
  SetSamePartyCookiesViaCookieStore(GetDescendantFrame({0}),
                                    /*is_same_party_context=*/false,
                                    /*site_is_in_fps=*/false);

  EXPECT_THAT(content::GetCookies(browser()->profile(), GURL("https://c.test")),
              net::CookieStringIs(
                  UnorderedElementsAre(Key(kSamePartySameSiteNoneCookieName),
                                       Key(kSameSiteNoneCookieName))));

  ASSERT_GE(2U, content::DeleteCookies(browser()->profile(),
                                       network::mojom::CookieDeletionFilter()));
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(c.test(b.test))"}))));
  SetSamePartyCookiesViaCookieStore(GetDescendantFrame({0, 0}),
                                    /*is_same_party_context=*/false,
                                    /*site_is_in_fps=*/true);
  EXPECT_THAT(
      content::GetCookies(browser()->profile(), GURL("https://b.test")),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

class CookieStoreSamePartyDisabledTest : public CookieStoreSamePartyTest {
 public:
  CookieStoreSamePartyDisabledTest() : CookieStoreSamePartyTest(false) {}
};

IN_PROC_BROWSER_TEST_F(CookieStoreSamePartyDisabledTest,
                       ReadAttribute_CrossPartyContext) {
  SetSamePartyCookies("c.test");
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(c.test)"}))));

  EXPECT_THAT(GetSamePartyAttributesViaCookieStore(GetDescendantFrame({0})),
              net::CookieStringIs(UnorderedElementsAre(
                  Pair(kSamePartySameSiteNoneCookieName, "undefined"),
                  Pair(kSameSiteNoneCookieName, "undefined"))));

  ASSERT_EQ(4U, content::DeleteCookies(browser()->profile(),
                                       network::mojom::CookieDeletionFilter()));

  SetSamePartyCookies("b.test");
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(c.test(b.test))"}))));

  EXPECT_THAT(GetSamePartyAttributesViaCookieStore(GetDescendantFrame({0, 0})),
              net::CookieStringIs(UnorderedElementsAre(
                  Pair(kSamePartySameSiteNoneCookieName, "undefined"),
                  Pair(kSameSiteNoneCookieName, "undefined"))));
}

}  // namespace
