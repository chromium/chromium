// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/test/extension_test_message_listener.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace extensions {

using ContextType = ExtensionApiTest::ContextType;

namespace {

enum class SameSiteCookieSemantics {
  kModern,
  kLegacy,
};

}  // namespace

// This test cannot be run by a Service Worked-based extension
// because it uses the Document object.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ReadFromDocument) {
  ASSERT_TRUE(RunExtensionTest("cookies/read_from_doc")) << message_;
}

class CookiesApiTest : public ExtensionApiTest,
                       public testing::WithParamInterface<
                           std::tuple<ContextType, SameSiteCookieSemantics>> {
 public:
  CookiesApiTest() : ExtensionApiTest(std::get<0>(GetParam())) {}
  ~CookiesApiTest() override = default;
  CookiesApiTest(const CookiesApiTest&) = delete;
  CookiesApiTest& operator=(const CookiesApiTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    // If SameSite access semantics is "legacy", add content settings to allow
    // legacy access for all sites.
    if (!AreSameSiteCookieSemanticsModern()) {
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

    net::test_server::RegisterDefaultHandlers(embedded_test_server());
    host_resolver()->AddRule("*", "127.0.0.1");

    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 protected:
  bool RunTest(const char* extension_name,
               bool allow_in_incognito = false,
               const char* custom_arg = nullptr) {
    return RunExtensionTest(extension_name, {.custom_arg = custom_arg},
                            {.allow_in_incognito = allow_in_incognito});
  }

  ContextType GetContextType() { return std::get<0>(GetParam()); }

  bool AreSameSiteCookieSemanticsModern() {
    return std::get<1>(GetParam()) == SameSiteCookieSemantics::kModern;
  }

 private:
  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote_;
};

INSTANTIATE_TEST_SUITE_P(
    EventPage,
    CookiesApiTest,
    ::testing::Combine(::testing::Values(ContextType::kEventPage),
                       ::testing::Values(SameSiteCookieSemantics::kLegacy,
                                         SameSiteCookieSemantics::kModern)));
INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    CookiesApiTest,
    ::testing::Combine(::testing::Values(ContextType::kServiceWorker),
                       ::testing::Values(SameSiteCookieSemantics::kLegacy,
                                         SameSiteCookieSemantics::kModern)));

// A test suite that only runs with MV3 extensions.
using CookiesApiMV3Test = CookiesApiTest;
INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    CookiesApiMV3Test,
    ::testing::Combine(::testing::Values(ContextType::kServiceWorker),
                       ::testing::Values(SameSiteCookieSemantics::kLegacy,
                                         SameSiteCookieSemantics::kModern)));

// TODO(crbug.com/40839864): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Cookies DISABLED_Cookies
#else
#define MAYBE_Cookies Cookies
#endif
IN_PROC_BROWSER_TEST_P(CookiesApiTest, MAYBE_Cookies) {
  ASSERT_TRUE(RunTest("cookies/api", /*allow_in_incognito=*/false,
                      AreSameSiteCookieSemanticsModern() ? "true" : "false"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesEvents) {
  ASSERT_TRUE(RunTest("cookies/events")) << message_;
}

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesEventsSpanning) {
  // We need to initialize an incognito mode window in order have an initialized
  // incognito cookie store. Otherwise, the chrome.cookies.set operation is just
  // ignored and we won't be notified about a newly set cookie for which we want
  // to test whether the storeId is set correctly.
  OpenURLOffTheRecord(browser()->profile(), GURL("chrome://newtab/"));
  ASSERT_TRUE(RunTest("cookies/events_spanning",
                      /*allow_in_incognito=*/true))
      << message_;
}

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesEventsSpanningAsync) {
  // This version of the test creates the OTR page *after* the JavaScript test
  // code has registered the cookie listener. This tests the cookie API code
  // that listens for the new profile creation.
  //
  // The test sends us message with the string "listening" once it's registered
  // its listener. We force a reply to synchronize with the JS so the test
  // always runs the same way.
  ExtensionTestMessageListener listener("listening", ReplyBehavior::kWillReply);
  listener.SetOnSatisfied(
      base::BindLambdaForTesting([this, &listener](const std::string&) {
        OpenURLOffTheRecord(browser()->profile(), GURL("chrome://newtab/"));
        listener.Reply("ok");
      }));

  ASSERT_TRUE(RunTest("cookies/events_spanning",
                      /*allow_in_incognito=*/true))
      << message_;
}

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesNoPermission) {
  ASSERT_TRUE(RunTest("cookies/no_permission")) << message_;
}

IN_PROC_BROWSER_TEST_P(CookiesApiMV3Test, TestGetPartitionKey) {
  // Before running test, set up a top-level site (a.com) that embeds a
  // cross-site (b.com). To test the cookies.getPartitionKey() api.
  const std::string default_response = "/defaultresponse";
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", default_response)));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Inject two iframes and navigate one to a cross-site with host permissions
  // (b.com) and the other to a cross-site (c.com) with no host permissions.
  const GURL cross_site_url =
      embedded_test_server()->GetURL("b.com", default_response);
  const GURL no_host_permissions_url =
      embedded_test_server()->GetURL("c.com", default_response);

  std::string script =
      "var f = document.createElement('iframe');\n"
      "f.src = '" +
      cross_site_url.spec() +
      "';\n"
      "document.body.appendChild(f);\n"
      "var noHostFrame = document.createElement('iframe');\n"
      "noHostFrame.src = '" +
      no_host_permissions_url.spec() +
      "';\n"
      "document.body.appendChild(noHostFrame);\n";

  EXPECT_TRUE(ExecJs(contents, script));
  EXPECT_TRUE(WaitForLoadStop(contents));
  ASSERT_TRUE(RunTest("cookies/get_partition_key")) << message_;
}

}  // namespace extensions
