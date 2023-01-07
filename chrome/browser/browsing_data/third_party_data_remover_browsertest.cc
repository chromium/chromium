// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/third_party_data_remover.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

using content::BrowserThread;

namespace {

const std::vector<std::string> kStorageTypes{
    "LocalStorage", "FileSystem",   "FileSystemAccess", "SessionStorage",
    "IndexedDb",    "CacheStorage", "ServiceWorker",    "StorageFoundation"};

class ThirdPartyDataRemoverTest : public InProcessBrowserTest {
 public:
  ThirdPartyDataRemoverTest(const ThirdPartyDataRemoverTest&) = delete;
  ThirdPartyDataRemoverTest& operator=(const ThirdPartyDataRemoverTest&) =
      delete;

 protected:
  ThirdPartyDataRemoverTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeatures(
        {features::kClientStorageAccessContextAuditing}, {});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    // Storage Foundation has to be enabled, since it is accessed from the tests
    // that use chrome/browser/net/storage_test_utils.cc.
    // TODO(fivedots): Remove this switch once Storage Foundation
    // is enabled by default.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "StorageFoundationAPI");
  }

  network::mojom::CookieManager* CookieManager() {
    return browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess();
  }

  void SetCookie(std::string host,
                 std::string name,
                 net::CookieSameSite same_site,
                 net::CookieOptions::SameSiteCookieContext cookie_context) {
    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateUnsafeCookieForTesting(
            name, "foobar", host, "/", base::Time(), base::Time(), base::Time(),
            base::Time(),
            /*secure=*/true, /*httponly=*/false, same_site,
            net::COOKIE_PRIORITY_LOW, /*same_party=*/false);
    net::CookieOptions options;
    options.set_same_site_cookie_context(cookie_context);
    bool result_out;
    base::RunLoop run_loop;

    CookieManager()->SetCanonicalCookie(
        *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
        options,
        base::BindLambdaForTesting([&](net::CookieAccessResult result) {
          result_out = result.status.IsInclude();
          run_loop.Quit();
        }));
    run_loop.Run();
    EXPECT_TRUE(result_out);
  }

  std::vector<net::CanonicalCookie> GetAllCookies() {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> cookies_out;
    CookieManager()->GetAllCookies(base::BindLambdaForTesting(
        [&](const std::vector<net::CanonicalCookie>& cookies) {
          cookies_out = cookies;
          run_loop.Quit();
        }));
    run_loop.Run();
    return cookies_out;
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateFrameTo(const std::string& host) {
    GURL page = https_server_.GetURL(host, "/browsing_data/site_data.html");
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", page));
  }

  content::RenderFrameHost* GetFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  }

  void AddStorage(const std::string& top_level_host,
                  const std::string& storage_host) {
    NavigateToPageWithFrame(top_level_host);
    NavigateFrameTo(storage_host);
    for (const auto& data_type : kStorageTypes) {
      bool result = false;
      EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
          GetFrame(), "set" + data_type + "()", &result));
      EXPECT_TRUE(result);
    }
  }

  void ExpectStorage(const std::string& host, bool expected) {
    NavigateToPageWithFrame(host);
    NavigateFrameTo(host);
    for (const auto& data_type : kStorageTypes) {
      bool actual = false;
      EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
          GetFrame(), "has" + data_type + "();", &actual));
      EXPECT_EQ(expected, actual) << " for " << data_type;
    }
  }

  void RunClearThirdPartyData() {
    base::RunLoop run_loop;
    ClearThirdPartyData(run_loop.QuitClosure(), browser()->profile());
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::EmbeddedTestServer https_server_;
};

// Test that ClearThirdPartyData clears SameSite=None cookies.
IN_PROC_BROWSER_TEST_F(ThirdPartyDataRemoverTest, ClearsSameSiteNoneCookies) {
  SetCookie(
      "www.google.com", "foo", net::CookieSameSite::NO_RESTRICTION,
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE));
  SetCookie("www.google.com", "bar", net::CookieSameSite::STRICT_MODE,
            net::CookieOptions::SameSiteCookieContext(
                net::CookieOptions::SameSiteCookieContext::ContextType::
                    SAME_SITE_STRICT));

  RunClearThirdPartyData();

  std::vector<net::CanonicalCookie> cookies = GetAllCookies();
  ASSERT_EQ(1u, cookies.size());
  ASSERT_EQ("bar", cookies[0].Name());
}

// Test that ClearThirdPartyData only clears storage accessed in cross-site
// contexts when access context auditing is enabled.
IN_PROC_BROWSER_TEST_F(ThirdPartyDataRemoverTest,
                       ClearsStorageAccessedInThirdPartyContext) {
  ExpectStorage("xsite.com", false);
  ExpectStorage("samesite.com", false);
  AddStorage("toplevel.com", "xsite.com");
  AddStorage("foo.samesite.com", "samesite.com");
  ExpectStorage("xsite.com", true);
  ExpectStorage("samesite.com", true);

  RunClearThirdPartyData();

  ExpectStorage("xsite.com", false);
  ExpectStorage("samesite.com", true);
}

class ThirdPartyDataRemoverFallbackTest : public ThirdPartyDataRemoverTest {
 public:
  ThirdPartyDataRemoverFallbackTest(const ThirdPartyDataRemoverFallbackTest&) =
      delete;
  ThirdPartyDataRemoverFallbackTest& operator=(
      const ThirdPartyDataRemoverFallbackTest&) = delete;

 protected:
  ThirdPartyDataRemoverFallbackTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kClientStorageAccessContextAuditing});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::EmbeddedTestServer https_server_;
};

// Test the fallback behavior of ClearThirdPartyData when access context
// auditing is not enabled. In this case the function should clear storage for
// domains
IN_PROC_BROWSER_TEST_F(ThirdPartyDataRemoverFallbackTest,
                       ClearsStorageForDomainsWithSameSiteNoneCookies) {
  SetCookie(
      "xsitecookie.com", "foo", net::CookieSameSite::NO_RESTRICTION,
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE));
  SetCookie("samesitecookie.com", "bar", net::CookieSameSite::STRICT_MODE,
            net::CookieOptions::SameSiteCookieContext(
                net::CookieOptions::SameSiteCookieContext::ContextType::
                    SAME_SITE_STRICT));

  ExpectStorage("xsitecookie.com", false);
  ExpectStorage("samesitecookie.com", false);
  ExpectStorage("nocookie.com", false);
  // Storage for xsitecookie.com is only accessed in same-site context but
  // should be cleared anyway.
  AddStorage("xsitecookie.com", "xsitecookie.com");
  // Storage for samesitecookie is only accessed in cross-site contexts, but
  // since it does not have a SameSite=None cookie it should not be cleared.
  AddStorage("toplevel.com", "samesitecookie.com");
  // Similarly, if a site has no cookies it should still keep its storage.
  AddStorage("toplevel.com", "nocookie.com");
  ExpectStorage("xsitecookie.com", true);
  ExpectStorage("samesitecookie.com", true);
  ExpectStorage("nocookie.com", true);

  RunClearThirdPartyData();

  ExpectStorage("xsitecookie.com", false);
  ExpectStorage("samesitecookie.com", true);
  ExpectStorage("nocookie.com", true);
}

}  // namespace
