// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace {

class CookieStoreSameSiteTest : public InProcessBrowserTest,
                                public ::testing::WithParamInterface<bool> {
 protected:
  CookieStoreSameSiteTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  CookieStoreSameSiteTest(const CookieStoreSameSiteTest&) = delete;
  CookieStoreSameSiteTest& operator=(const CookieStoreSameSiteTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(https_server_.Start());

    // If SameSite access semantics is "legacy", add content settings to allow
    // legacy access for all sites.
    if (!HasNonLegacySameSiteAccessSemantics()) {
      browser()
          ->profile()
          ->GetDefaultStoragePartition()
          ->GetNetworkContext()
          ->GetCookieManager(
              cookie_manager_remote_.BindNewPipeAndPassReceiver());
      cookie_manager_remote_->SetContentSettings(
          ContentSettingsType::LEGACY_COOKIE_ACCESS,
          {ContentSettingPatternSource(
              ContentSettingsPattern::Wildcard(),
              ContentSettingsPattern::Wildcard(),
              base::Value(ContentSetting::CONTENT_SETTING_ALLOW),
              content_settings::ProviderType::kNone, false /* incognito */)},
          base::NullCallback());
      cookie_manager_remote_.FlushForTesting();
    }
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  content::RenderFrameHost* GetFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  }

  std::string GetCookieSameSite(const std::string& cookie_name) {
    std::string script = base::StringPrintf(R"(
      (async () => {
          const cookie = await cookieStore.get('%s');
          return cookie ? cookie.sameSite : '';
      }) ();
    )",
                                            cookie_name.c_str());
    return content::EvalJs(GetFrame(), script).ExtractString();
  }

 protected:
  bool HasNonLegacySameSiteAccessSemantics() { return GetParam(); }

  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote_;
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_P(CookieStoreSameSiteTest,
                       CookieStoreUsesEffectiveSameSiteForUnspecifiedCookies) {
  NavigateToPageWithFrame("a.test");

  // Create unspecified sameSite cookie with document.cookie because CookieStore
  // doesn't allow unspecified sameSite.
  EXPECT_TRUE(content::ExecJs(GetFrame(),
                              "document.cookie='unspecified-cookie=value'"));

  std::string sameSite = GetCookieSameSite("unspecified-cookie");
  if (HasNonLegacySameSiteAccessSemantics()) {
    EXPECT_EQ("lax", sameSite);
  } else {
    EXPECT_EQ("none", sameSite);
  }
}

IN_PROC_BROWSER_TEST_P(CookieStoreSameSiteTest,
                       CookieStoreSameSiteDefaultsToStrict) {
  NavigateToPageWithFrame("a.test");

  // Create cookie with CookieStore with no sameSite.
  EXPECT_TRUE(content::ExecJs(GetFrame(),
                              "cookieStore.set('default-cookie', 'value')"));

  EXPECT_EQ("strict", GetCookieSameSite("default-cookie"));
}

IN_PROC_BROWSER_TEST_P(CookieStoreSameSiteTest,
                       CookieStoreUsesSpecifiedSameSite) {
  NavigateToPageWithFrame("a.test");

  // Create cookie with CookieStore with none sameSite. CookieStore defaults to
  // Secure when written from a secure web origin, therefore allows for sameSite
  // none.
  EXPECT_TRUE(content::ExecJs(GetFrame(),
                              "cookieStore.set({name: 'none-cookie', value: "
                              "'value', sameSite: 'none'})"));

  EXPECT_EQ("none", GetCookieSameSite("none-cookie"));
}

INSTANTIATE_TEST_SUITE_P(All, CookieStoreSameSiteTest, ::testing::Bool());
}  // namespace
