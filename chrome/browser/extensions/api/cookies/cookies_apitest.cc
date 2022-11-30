// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
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
      cookie_manager_remote_->SetContentSettingsForLegacyCookieAccess(
          {ContentSettingPatternSource(
              ContentSettingsPattern::Wildcard(),
              ContentSettingsPattern::Wildcard(),
              base::Value(ContentSetting::CONTENT_SETTING_ALLOW),
              std::string() /* source */, false /* incognito */)});
      cookie_manager_remote_.FlushForTesting();
    }
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

// TODO(crbug.com/1325506): Flaky on Windows.
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

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesNoPermission) {
  ASSERT_TRUE(RunTest("cookies/no_permission")) << message_;
}

}  // namespace extensions
