// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/protocol_handlers/protocol_handlers_manager.h"

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

#if BUILDFLAG(IS_MAC)
#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/custom_handlers/chrome_protocol_handler_registry_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "components/custom_handlers/protocol_handler_navigation_throttle.h"
#endif

namespace {

static constexpr char kExtensionPath[] = "protocol_handlers_api/Extensions";

custom_handlers::ProtocolHandler CreateProtocolHandler(
    const std::string& protocol,
    const GURL& url) {
  return custom_handlers::ProtocolHandler::CreateProtocolHandler(protocol, url);
}

custom_handlers::ProtocolHandler CreateExtensionProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    const std::string& extension_id,
    bool is_allowed_in_incognito = false) {
  return custom_handlers::ProtocolHandler::CreateExtensionProtocolHandler(
      protocol, url, extension_id, is_allowed_in_incognito);
}

#if BUILDFLAG(IS_MAC)
// Test delegate that disables OS-level registration. The real delegate calls
// `shell_integration::DefaultSchemeClientWorker::StartSetAsDefault`, which on
// Mac fails because the test app_bundle is not valid; the failure callback
// then deletes the handler when `ShouldRemoveHandlersNotInOS()` is true.
//
// Mirrors the pattern in
// chrome/browser/custom_handlers/protocol_handler_registry_browsertest.cc.
class TestProtocolHandlerRegistryDelegate
    : public ChromeProtocolHandlerRegistryDelegate {
  void RegisterWithOSAsDefaultClient(const std::string& protocol,
                                     DefaultClientCallback callback) override {}
  void CheckDefaultClientWithOS(const std::string& protocol,
                                DefaultClientCallback callback) override {}
  bool ShouldRemoveHandlersNotInOS() override { return false; }
};

// Builds a `ProtocolHandlerRegistry` with the real chrome `PrefService` and the
// chrome-derived test delegate above. Used as a `SetTestingFactory` closure so
// the test delegate is installed at registry-construction time on both the
// regular and lazily-created primary OTR profiles. Preserves the
// OverlayUserPrefStore mechanism the OTR isolation design depends on; do not
// regress to a `prefs=nullptr` setup.
std::unique_ptr<KeyedService> BuildTestProtocolHandlerRegistry(
    content::BrowserContext* context) {
  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  CHECK(prefs);
  return custom_handlers::ProtocolHandlerRegistry::Create(
      prefs, std::make_unique<TestProtocolHandlerRegistryDelegate>(),
      context->IsOffTheRecord());
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

using HandlerPermissionGrantedCallback = custom_handlers::
    ProtocolHandlerNavigationThrottle::HandlerPermissionGrantedCallback;
using HandlerPermissionDeniedCallback = custom_handlers::
    ProtocolHandlerNavigationThrottle::HandlerPermissionDeniedCallback;

namespace extensions {

class ProtocolHandlersManagerBrowserTest : public ExtensionBrowserTest {
 public:
  ProtocolHandlersManagerBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionProtocolHandlers);
#if !BUILDFLAG(IS_ANDROID)
    custom_handlers::ProtocolHandlerNavigationThrottle::
        GetDialogLaunchCallbackForTesting() = base::BindRepeating(
            [](HandlerPermissionGrantedCallback granted_callback,
               HandlerPermissionDeniedCallback denied_callback) {
              std::move(granted_callback).Run(/*remember=*/true);
            });
#endif
  }
  ProtocolHandlersManagerBrowserTest(
      const ProtocolHandlersManagerBrowserTest&) = delete;
  ProtocolHandlersManagerBrowserTest& operator=(
      const ProtocolHandlersManagerBrowserTest&) = delete;

 protected:
  custom_handlers::ProtocolHandlerRegistry* GetProtocolHandlersRegistry() {
    return ProtocolHandlerRegistryFactory::GetForBrowserContext(
        browser()->profile());
  }
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Toggling the incognito setting reloads the extension via
  // ReloadExtensionIfEnabled, which fires OnExtensionUnloaded/OnExtensionLoaded
  // asynchronously. Without waiting, the regular-profile
  // ProtocolHandlersManager may not have re-registered handlers (and persisted
  // the updated is_allowed_in_incognito flag to prefs) before the test creates
  // a fresh incognito browser that loads from those prefs.
  void SetExtensionIncognitoEnabledAndWait(const ExtensionId& id,
                                           bool enabled) {
    TestExtensionRegistryObserver observer(
        ExtensionRegistry::Get(browser()->profile()), id);
    util::SetIsIncognitoEnabled(id, browser()->profile(), enabled);
    observer.WaitForExtensionLoaded();
  }

  // On Mac, `ProtocolHandlerRegistryFactory::GetForBrowserContext()` returns
  // null without an explicit testing factory (the factory's
  // `ServiceIsNULLWhileTesting()` is true and the context is treated as a
  // testing context). The closure below routes around that and also installs a
  // chrome-derived test delegate at construction time on both the regular and
  // primary OTR profiles, so no real shell-integration call ever fires.
  // Non-Mac platforms return a non-null service from the production factory.
  // TODO(crbug.com/40482153): pinpoint why only Mac flips the
  // testing-context flag and remove this Mac-specific override.
#if BUILDFLAG(IS_MAC)
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
    ProtocolHandlerRegistryFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce(&BuildTestProtocolHandlerRegistry));
  }
#endif

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Loading an extension triggers the protocol handlers registration.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest, RegisterHandlers) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));

  GURL url1("web+ecsearch:cats");
  GURL url2("web+ducksearch:dogs");
  const custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();
  content::WebContents* web_contents = GetWebContents();

  // Check the registry.
  ASSERT_EQ(1u, registry->GetHandlersFor(url1.GetScheme()).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(url2.GetScheme()).size());

  // Test the handlers.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_EQ("https://www.ecosia.org/search?q=web%2Becsearch%3Acats",
            web_contents->GetLastCommittedURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  ASSERT_EQ("https://duckduckgo.com/?q=web%2Bducksearch%3Adogs",
            web_contents->GetLastCommittedURL());
}

// Uninstalling an extension deregisters its associated protocol handlers.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest, UnregisterHandlers) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));

  GURL url1("web+ecsearch:cats");
  GURL url2("web+ducksearch:dogs");
  const custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();
  content::WebContents* web_contents = GetWebContents();

  // Check the registry.
  ASSERT_EQ(1u, registry->GetHandlersFor(url1.GetScheme()).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(url2.GetScheme()).size());

  // Check handlers are removed when uninstalling.
  UninstallExtension(last_loaded_extension_id());

  // Ensure the custom handler has been removed.
  ASSERT_FALSE(registry->IsHandledProtocol(url1.GetScheme()));
  ASSERT_FALSE(registry->IsHandledProtocol(url2.GetScheme()));

  // Test the navigation without the handlers.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_EQ("about:blank", web_contents->GetLastCommittedURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  ASSERT_EQ("about:blank", web_contents->GetLastCommittedURL());
}

// The extension's associated handlers are deregistered when it's disabled and
// registered again when enabled.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       DisableAndEnableExtension) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));

  GURL url1("web+ecsearch:cats");
  GURL url2("web+ducksearch:dogs");
  const ExtensionId& extension_id = last_loaded_extension_id();
  const custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();
  content::WebContents* web_contents = GetWebContents();

  // Check the registry.
  ASSERT_EQ(1u, registry->GetHandlersFor(url1.GetScheme()).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(url2.GetScheme()).size());

  // Check handlers are removed when disabling.
  DisableExtension(extension_id);

  // Ensure the custom handler has been removed.
  ASSERT_FALSE(registry->IsHandledProtocol(url1.GetScheme()));
  ASSERT_FALSE(registry->IsHandledProtocol(url2.GetScheme()));

  // Test the navigation without the handlers.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_EQ("about:blank", web_contents->GetLastCommittedURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  ASSERT_EQ("about:blank", web_contents->GetLastCommittedURL());

  // Check handlers are registered when enabling.
  EnableExtension(extension_id);

  // Test the handlers.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_EQ("https://www.ecosia.org/search?q=web%2Becsearch%3Acats",
            web_contents->GetLastCommittedURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  ASSERT_EQ("https://duckduckgo.com/?q=web%2Bducksearch%3Adogs",
            web_contents->GetLastCommittedURL());
}

// An extension update with changes in the protocol handlers declared in the
// Manifest causes the old ones to be deregistered and triggers the registration
// of the new handlers.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest, UpdateExtension) {
  GURL url1("web+ecsearch:cats");
  GURL url2("web+ducksearch:dogs");
  GURL url3("ipfs:dogs");
  const custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();
  content::WebContents* web_contents = GetWebContents();

  constexpr char kManifestV1[] =
      R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 3,
        "protocol_handlers": [
          {
            "name": "Ecosia Search Handler",
            "protocol": "web+ecsearch",
            "uriTemplate": "https://www.ecosia.org/search?q=%s"
          },
          {
            "name": "DuckDuckGo Search Handler",
            "protocol": "web+ducksearch",
            "uriTemplate": "https://duckduckgo.com/?q=%s"
          }
        ]
      })";

  // Write the initial manifest and install the extension.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifestV1);
  base::FilePath crx_v1_path = test_dir.Pack("v1.crx");
  const Extension* extension = InstallExtensionWithUIAutoConfirm(
      crx_v1_path, 1 /*+1 installed extension*/);
  ASSERT_TRUE(extension);

  // Check the initial handlers are in the registry.
  ASSERT_EQ(1u, registry->GetHandlersFor(url1.GetScheme()).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(url2.GetScheme()).size());

  constexpr char kManifestV2[] =
      R"({
        "name": "Test",
        "version": "2",
        "manifest_version": 3,
        "protocol_handlers": [
          {
            "name": "Bing Search Handler",
            "protocol": "ipfs",
            "uriTemplate": "https://www.bing.com/search?q=%s"
          }
        ]
      })";

  // Change the Manifest and update the extension.
  test_dir.WriteManifest(kManifestV2);
  base::FilePath crx_v2_path = test_dir.Pack("v2.crx");
  UpdateExtension(extension->id(), crx_v2_path, /*expected_change=*/0);

  // Check the registry after the update.
  ASSERT_FALSE(registry->HasDefaultHandler(url1.GetScheme()));
  ASSERT_FALSE(registry->HasDefaultHandler(url2.GetScheme()));
  ASSERT_TRUE(registry->HasDefaultHandler(url3.GetScheme()));

  // Test the old handlers are not used.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_EQ("about:blank", web_contents->GetLastCommittedURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  ASSERT_EQ("about:blank", web_contents->GetLastCommittedURL());

  // Test the new handlers.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url3));
  ASSERT_EQ("https://www.bing.com/search?q=ipfs%3Adogs",
            web_contents->GetLastCommittedURL());
}

// Registration via the Extension API when there is a protocol handler for the
// same 'scheme' already registered through the Web API.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       ExtensionRegistrationConflictSameScheme) {
  constexpr char kScheme1[] = "web+ecsearch";
  constexpr char kScheme2[] = "web+ducksearch";
  custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();
  auto webAPI_handler =
      CreateProtocolHandler(kScheme1, GURL("https://www.google.com/%s"));

  // Simulate a protocol handler registration through the Web API.
  registry->OnAcceptRegisterProtocolHandler(webAPI_handler);
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(0u, registry->GetHandlersFor(kScheme2).size());
  ASSERT_TRUE(registry->IsDefault(webAPI_handler));

  // Protocol handler registration through the Extension API.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));
  ASSERT_EQ(2u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  ASSERT_TRUE(registry->IsDefault(webAPI_handler));

  // The handler created via the Web API is removed, but there is still the one
  // registered via the Extension API.
  registry->RemoveHandler(webAPI_handler);
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  auto extAPI_handler = CreateExtensionProtocolHandler(
      kScheme1, GURL("https://www.ecosia.org/search?q=%s"),
      last_loaded_extension_id());
  ASSERT_TRUE(registry->IsDefault(extAPI_handler));
}

// Registration via the Web API when there is a protocol handler for the same
// 'scheme' already registered through the Extension API.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       WebAPIRegistrationConflictSameScheme) {
  constexpr char kScheme1[] = "web+ecsearch";
  constexpr char kScheme2[] = "web+ducksearch";
  custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();

  // Protocol handler registration through the Extension API.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  auto extAPI_handler = CreateExtensionProtocolHandler(
      kScheme1, GURL("https://www.ecosia.org/search?q=%s"),
      last_loaded_extension_id());
  ASSERT_TRUE(registry->IsDefault(extAPI_handler));

  // Simulate a protocol handler registration through the Web API.
  auto webAPI_handler =
      CreateProtocolHandler(kScheme1, GURL("https://www.google.com/%s"));
  registry->OnAcceptRegisterProtocolHandler(webAPI_handler);
  ASSERT_EQ(2u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  ASSERT_TRUE(registry->IsDefault(webAPI_handler));

  // The handler created via the Web API is removed, but there is still the one
  // registered via the Extension API.
  registry->RemoveHandler(webAPI_handler);
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  ASSERT_TRUE(registry->IsDefault(extAPI_handler));
}

// This test verifies that a protocol handler registered through the
// Extension API is ignored if the same handler has been registered
// by the Web API.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       ExtentionRegistrationConflictSameHandler) {
  constexpr char kScheme1[] = "web+ecsearch";
  constexpr char kScheme2[] = "web+ducksearch";
  custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();
  auto handler = CreateProtocolHandler(
      kScheme1, GURL("https://www.ecosia.org/search?q=%s"));

  // Simulate a protocol handler registration through the Web API.
  registry->OnAcceptRegisterProtocolHandler(handler);
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(0u, registry->GetHandlersFor(kScheme2).size());
  ASSERT_TRUE(registry->IsDefault(handler));

  // The registration through the Extension API of the 'web+ecsearch' scheme
  // handler should be ignored because the WebAPI registered the same handler.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  ASSERT_TRUE(registry->IsDefault(handler));

  // The handler created via the Web API is removed, so given that the one
  // registered via the Extension API has been ignored, the 'web+ecsearch'
  // scheme is not handled anymore.
  // TODO(crbug.com/40482153): We need a more robust logic here; unlike the
  // Web API, other installed extensions still expect their handlers to take
  // over when the default one has been removed.
  registry->RemoveHandler(handler);
  ASSERT_FALSE(registry->HasDefaultHandler(kScheme1));
  ASSERT_TRUE(registry->HasDefaultHandler(kScheme2));
}

// Test the conflict resolution between 2 extensions registering different
// handlers for the same scheme.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       TwoExtensionRegistrationConflictSameScheme) {
  constexpr char kScheme1[] = "web+ecsearch";
  constexpr char kScheme2[] = "web+ducksearch";
  custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();

  // Protocol handler registration through the Extension API.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  auto extAPI_handler1 = CreateExtensionProtocolHandler(
      kScheme1, GURL("https://www.ecosia.org/search?q=%s"),
      last_loaded_extension_id());
  ASSERT_TRUE(registry->IsDefault(extAPI_handler1));

  // Another extension declaring a protocol handler for the same scheme, which
  // should be registered as default and queueing the one previously registered.
  constexpr char kManifest[] =
      R"({
        "name": "Another extension",
        "version": "1",
        "manifest_version": 3,
        "protocol_handlers": [
          {
            "name": "Ecosia Search experimental handler",
            "protocol": "web+ecsearch",
            "uriTemplate": "https://www.example.org/search?q=%s"
          }
        ]
      })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  base::FilePath crx_path = test_dir.Pack("test.crx");
  const Extension* extension =
      InstallExtensionWithUIAutoConfirm(crx_path, 1 /*+1 installed extension*/);
  ASSERT_TRUE(extension);
  ASSERT_EQ(2u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  auto extAPI_handler2 = CreateExtensionProtocolHandler(
      kScheme1, GURL("https://www.example.org/search?q=%s"),
      last_loaded_extension_id());
  ASSERT_TRUE(registry->IsDefault(extAPI_handler2));

  // The handler registered by the second extension is removed, so the
  // previously registered one is promoted to default.
  registry->RemoveHandler(extAPI_handler2);
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  ASSERT_TRUE(registry->IsDefault(extAPI_handler1));
}

// Test the conflict resolution between 2 extensions registering the same
// handler, so that the new handler will replace the old one.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       TwoExtensionRegistrationConflictSameHandler) {
  constexpr char kScheme1[] = "web+ecsearch";
  constexpr char kScheme2[] = "web+ducksearch";
  const GURL url("https://www.ecosia.org/search?q=%s");
  custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();

  // Protocol handler registration through the Extension API.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  auto extAPI_handler1 =
      CreateExtensionProtocolHandler(kScheme1, url, last_loaded_extension_id());
  ASSERT_TRUE(registry->IsDefault(extAPI_handler1));

  // Another extension declaring a the same protocol handler, which
  // should replace the one registered by the previous extension.
  constexpr char kManifest[] =
      R"({
        "name": "Another extension",
        "version": "1",
        "manifest_version": 3,
        "protocol_handlers": [
          {
            "name": "Ecosia Search experimental handler",
            "protocol": "web+ecsearch",
            "uriTemplate": "https://www.ecosia.org/search?q=%s"
          }
        ]
      })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  base::FilePath crx_path = test_dir.Pack("test.crx");
  const Extension* extension =
      InstallExtensionWithUIAutoConfirm(crx_path, 1 /*+1 installed extension*/);
  ASSERT_TRUE(extension);
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  auto extAPI_handler2 =
      CreateExtensionProtocolHandler(kScheme1, url, last_loaded_extension_id());
  ASSERT_TRUE(registry->IsDefault(extAPI_handler2));

  // The handler registered by the last extension is removed, so given that the
  // one registered via the previous extension has been ignored, the
  // 'web+ecsearch' scheme is not handled anymore.
  // TODO(crbug.com/40482153): We need a more robust logic here; unlike the
  // Web API, other installed extensions still expect their handlers to take
  // over when the default one has been removed.
  registry->RemoveHandler(extAPI_handler2);
  ASSERT_FALSE(registry->HasDefaultHandler(kScheme1));
  ASSERT_TRUE(registry->HasDefaultHandler(kScheme2));
}

class ProtocolHandlersManagerOTRBrowserTest
    : public ProtocolHandlersManagerBrowserTest {
 protected:
  Profile* GetOTRProfile() {
    return browser()->profile()->GetPrimaryOTRProfile(
        /*create_if_needed=*/true);
  }

  custom_handlers::ProtocolHandlerRegistry* GetOTRProtocolHandlersRegistry() {
    return ProtocolHandlerRegistryFactory::GetForBrowserContext(
        GetOTRProfile());
  }
};

class ProtocolHandlersManagerOTRAllowIncognitoBrowserTest
    : public ProtocolHandlersManagerOTRBrowserTest,
      public ::testing::WithParamInterface<bool> {};

// Extension protocol handlers appear in the OTR registry only when the
// extension is allowed in incognito. The OTR registry reads from the regular
// profile's prefs via OverlayUserPrefStore and filters handlers by
// is_allowed_in_incognito; no ProtocolHandlersManager runs in OTR.
IN_PROC_BROWSER_TEST_P(ProtocolHandlersManagerOTRAllowIncognitoBrowserTest,
                       ExtensionHandlersNotInOTRRegistry) {
  const bool allow_in_incognito = GetParam();

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kExtensionPath),
                    {.allow_in_incognito = allow_in_incognito});
  ASSERT_TRUE(extension);

  const custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();
  EXPECT_EQ(1u, registry->GetHandlersFor("web+ecsearch").size());
  EXPECT_EQ(1u, registry->GetHandlersFor("web+ducksearch").size());

  const custom_handlers::ProtocolHandlerRegistry* otr_registry =
      GetOTRProtocolHandlersRegistry();
  EXPECT_NE(registry, otr_registry);
  const size_t expected = allow_in_incognito ? 1u : 0u;
  EXPECT_EQ(expected, otr_registry->GetHandlersFor("web+ecsearch").size());
  EXPECT_EQ(expected, otr_registry->GetHandlersFor("web+ducksearch").size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ProtocolHandlersManagerOTRAllowIncognitoBrowserTest,
                         ::testing::Bool());

// When the extension is not allowed in incognito, navigating to one of its
// protocol URLs in an incognito browser must NOT resolve through the handler.
// The OTR registry loads handlers from prefs but filters them by
// is_allowed_in_incognito; handlers with is_allowed_in_incognito=false are
// invisible to GetHandlerFor and therefore do not intercept navigation.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerOTRBrowserTest,
                       ExtensionHandlerNavigationNotInIncognito) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));

  GURL url1("web+ecsearch:cats");
  GURL url2("web+ducksearch:dogs");

  // Navigation works in the regular browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  EXPECT_EQ("https://www.ecosia.org/search?q=web%2Becsearch%3Acats",
            GetWebContents()->GetLastCommittedURL());

  // Navigation should not work in the incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser();
  content::WebContents* incognito_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url1));
  EXPECT_EQ("about:blank", incognito_web_contents->GetLastCommittedURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url2));
  EXPECT_EQ("about:blank", incognito_web_contents->GetLastCommittedURL());
}

// When the extension is allowed in incognito, navigating to one of its
// protocol URLs in an incognito browser should resolve through the handler.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerOTRBrowserTest,
                       ExtensionHandlerNavigationWorksInIncognitoWhenAllowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath),
                            {.allow_in_incognito = true}));

  Browser* incognito_browser = CreateIncognitoBrowser();
  content::WebContents* incognito_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser,
                                           GURL("web+ecsearch:cats")));
  EXPECT_EQ("https://www.ecosia.org/search?q=web%2Becsearch%3Acats",
            incognito_web_contents->GetLastCommittedURL());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser,
                                           GURL("web+ducksearch:dogs")));
  EXPECT_EQ("https://duckduckgo.com/?q=web%2Bducksearch%3Adogs",
            incognito_web_contents->GetLastCommittedURL());
}

// Disabling an extension must remove its handlers from the regular registry's
// in-memory state (not just from prefs). The OTR registry, which loaded the
// handler with is_allowed_in_incognito=false (extension not allowed in
// incognito), must continue to return empty from GetHandlersFor throughout.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerOTRBrowserTest,
                       DisableExtensionClearsInMemoryHandlers) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));

  const custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();
  ASSERT_EQ(1u, registry->GetHandlersFor("web+ecsearch").size());
  ASSERT_EQ(1u, registry->GetHandlersFor("web+ducksearch").size());

  const custom_handlers::ProtocolHandlerRegistry* otr_registry =
      GetOTRProtocolHandlersRegistry();
  ASSERT_TRUE(otr_registry->GetHandlersFor("web+ecsearch").empty());
  ASSERT_TRUE(otr_registry->GetHandlersFor("web+ducksearch").empty());

  DisableExtension(last_loaded_extension_id());

  EXPECT_TRUE(registry->GetHandlersFor("web+ecsearch").empty());
  EXPECT_TRUE(registry->GetHandlersFor("web+ducksearch").empty());
  EXPECT_TRUE(otr_registry->GetHandlersFor("web+ecsearch").empty());
  EXPECT_TRUE(otr_registry->GetHandlersFor("web+ducksearch").empty());
}

// Extension handlers are not available in incognito by default, because
// ExtensionPrefs::IsIncognitoEnabled() defaults to false.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       ExtensionHandlerNotAllowedInIncognitoByDefault) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));

  std::string_view scheme = "web+ecsearch";
  const custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlersRegistry();
  // Extension handler is registered in normal mode.
  ASSERT_FALSE(registry->GetHandlerFor(scheme).IsEmpty());

  // Verify is handled in incognito.
  Browser* incognito_browser = CreateIncognitoBrowser();
  custom_handlers::ProtocolHandlerRegistry* incognito_registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          incognito_browser->profile());
  ASSERT_TRUE(incognito_registry->GetHandlerFor(scheme).IsEmpty());
}

// Enabling incognito for an extension reloads it, causing OnExtensionLoaded to
// re-register its handlers with is_allowed_in_incognito=true, making them
// available in the incognito profile's registry.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       ExtensionHandlerAllowedAfterIncognitoSettingEnabled) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));
  const ExtensionId& extension_id = last_loaded_extension_id();
  std::string_view scheme = "web+ecsearch";

  // Enable incognito and wait for the resulting extension reload to settle so
  // the regular-profile registry has saved the updated flag to prefs before
  // the new incognito profile loads from those prefs.
  SetExtensionIncognitoEnabledAndWait(extension_id, true);
  // Verify is handled in incognito.
  Browser* incognito_browser = CreateIncognitoBrowser();
  custom_handlers::ProtocolHandlerRegistry* incognito_registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          incognito_browser->profile());
  ASSERT_FALSE(incognito_registry->GetHandlerFor(scheme).IsEmpty());
}

// Toggling the incognito setting off after it was enabled re-registers the
// handlers with is_allowed_in_incognito=false, hiding them from incognito.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       ExtensionHandlerBlockedAfterIncognitoSettingDisabled) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));
  const ExtensionId& extension_id = last_loaded_extension_id();
  std::string_view scheme = "web+ecsearch";

  // Enable incognito. Wait for the reload so prefs reflect the new flag
  // before the incognito profile loads from them.
  SetExtensionIncognitoEnabledAndWait(extension_id, true);
  // Verify is handled in incognito.
  {
    Browser* incognito_browser = CreateIncognitoBrowser();
    custom_handlers::ProtocolHandlerRegistry* incognito_registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            incognito_browser->profile());
    ASSERT_FALSE(incognito_registry->GetHandlerFor(scheme).IsEmpty());
    CloseBrowserSynchronously(incognito_browser);
  }

  // Disable incognito. Same wait — otherwise the new incognito profile's
  // pref-load can race with the regular-profile re-registration and the
  // OTR registry would load the still-allowed handler.
  SetExtensionIncognitoEnabledAndWait(extension_id, false);
  // Verify no longer available in incognito.
  Browser* incognito_browser2 = CreateIncognitoBrowser();
  custom_handlers::ProtocolHandlerRegistry* incognito_registry2 =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          incognito_browser2->profile());
  ASSERT_TRUE(incognito_registry2->GetHandlerFor(scheme).IsEmpty());
}

}  // namespace extensions
