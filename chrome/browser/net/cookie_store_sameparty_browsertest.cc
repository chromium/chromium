// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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

const char* kSamePartySameSiteLaxCookieName = "sameparty_samesite_lax_cookie";
const char* kSamePartySameSiteNoneCookieName = "sameparty_samesite_none_cookie";
const char* kSamePartySameSiteUnspecifiedCookieName =
    "sameparty_samesite_unspecified_cookie";
const char* kSameSiteNoneCookieName = "samesite_none_cookie";

class CookieStoreSamePartyTest : public InProcessBrowserTest {
 public:
  CookieStoreSamePartyTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  CookieStoreSamePartyTest(const CookieStoreSamePartyTest&) = delete;
  CookieStoreSamePartyTest& operator=(const CookieStoreSamePartyTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(network::switches::kUseFirstPartySet,
                                    "https://a.test,https://b.test");
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
    content::RenderFrameHost* frame = contents()->GetMainFrame();
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

  const net::test_server::EmbeddedTestServer& https_server() const {
    return https_server_;
  }

 private:
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(CookieStoreSamePartyTest, ReadCookies_SamePartyContext) {
  SetSamePartyCookies("b.test");
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(b.test)"}))));

  EXPECT_THAT(GetCookiesViaCookieStore(GetDescendantFrame({0})),
              net::CookieStringIs(testing::UnorderedElementsAre(
                  testing::Key(kSamePartySameSiteLaxCookieName),
                  testing::Key(kSamePartySameSiteNoneCookieName),
                  testing::Key(kSamePartySameSiteUnspecifiedCookieName),
                  testing::Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(CookieStoreSamePartyTest,
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
              net::CookieStringIs(testing::UnorderedElementsAre(
                  testing::Key(kSamePartySameSiteNoneCookieName),
                  testing::Key(kSameSiteNoneCookieName))));

  ASSERT_EQ(4U, content::DeleteCookies(browser()->profile(),
                                       network::mojom::CookieDeletionFilter()));

  SetSamePartyCookies("b.test");
  ASSERT_TRUE(NavigateToURL(
      contents(),
      https_server().GetURL("a.test",
                            base::StrCat({"/cross_site_iframe_factory.html?",
                                          "a.test(c.test(b.test))"}))));

  // SameParty is not ignored, so we get the SameSite=None cookie.
  EXPECT_THAT(GetCookiesViaCookieStore(GetDescendantFrame({0, 0})),
              net::CookieStringIs(testing::UnorderedElementsAre(
                  testing::Key(kSameSiteNoneCookieName))));
}

}  // namespace
