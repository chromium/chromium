// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/protocol_handlers/protocol_handlers_manager.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
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
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

#if BUILDFLAG(IS_MAC)
#include "components/custom_handlers/simple_protocol_handler_registry_factory.h"
#endif

namespace {

static constexpr const char kExtensionPath[] =
    "protocol_handlers_api/Extensions";

custom_handlers::ProtocolHandler CreateProtocolHandler(
    const std::string& protocol,
    const GURL& url) {
  return custom_handlers::ProtocolHandler::CreateProtocolHandler(protocol, url);
}

custom_handlers::ProtocolHandler CreateExtensionProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    const std::string& extension_id) {
  return custom_handlers::ProtocolHandler::CreateExtensionProtocolHandler(
      protocol, url, extension_id);
}

}  // namespace

namespace extensions {

class ProtocolHandlersManagerBrowserTest : public ExtensionBrowserTest {
 public:
  ProtocolHandlersManagerBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionProtocolHandlers);
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

  // TODO(crbug.com/40482153): Figure out why we need to add a Testing Factory
  // only for Mac and eventually solve it so that we can get rid of this
  // mac-specific code.
#if BUILDFLAG(IS_MAC)
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
    Profile* profile = Profile::FromBrowserContext(context);
    ProtocolHandlerRegistryFactory::GetInstance()->SetTestingFactory(
        profile, custom_handlers::SimpleProtocolHandlerRegistryFactory::
                     GetDefaultFactory());
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
  const auto* registry = GetProtocolHandlersRegistry();
  content::WebContents* web_contents = GetWebContents();

  // Check the registry.
  ASSERT_EQ(1u, registry->GetHandlersFor(url1.GetScheme()).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(url2.GetScheme()).size());

  // Test the handlers.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_EQ(GURL("https://www.ecosia.org/search?q=web%2Becsearch%3Acats"),
            web_contents->GetLastCommittedURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  ASSERT_EQ(GURL("https://duckduckgo.com/?q=web%2Bducksearch%3Adogs"),
            web_contents->GetLastCommittedURL());
}

// Uninstalling an extension deregisters its associated protocol handlers.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest, UnregisterHandlers) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));

  GURL url1("web+ecsearch:cats");
  GURL url2("web+ducksearch:dogs");
  const auto* registry = GetProtocolHandlersRegistry();
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
  ASSERT_EQ(GURL("about:blank"), web_contents->GetLastCommittedURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  ASSERT_EQ(GURL("about:blank"), web_contents->GetLastCommittedURL());
}

// The extension's associated handlers are deregistered when it's disabled and
// registered again when enabled.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       DisableAndEnableExtension) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));

  GURL url1("web+ecsearch:cats");
  GURL url2("web+ducksearch:dogs");
  const ExtensionId& extension_id = last_loaded_extension_id();
  const auto* registry = GetProtocolHandlersRegistry();
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
  ASSERT_EQ(GURL("about:blank"), web_contents->GetLastCommittedURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  ASSERT_EQ(GURL("about:blank"), web_contents->GetLastCommittedURL());

  // Check handlers are registered when enabling.
  EnableExtension(extension_id);

  // Test the handlers.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_EQ(GURL("https://www.ecosia.org/search?q=web%2Becsearch%3Acats"),
            web_contents->GetLastCommittedURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  ASSERT_EQ(GURL("https://duckduckgo.com/?q=web%2Bducksearch%3Adogs"),
            web_contents->GetLastCommittedURL());
}

// An extension update with changes in the protocol handlers declared in the
// Manifest causes the old ones to be deregistered and triggers the registration
// of the new handlers.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest, UpdateExtension) {
  GURL url1("web+ecsearch:cats");
  GURL url2("web+ducksearch:dogs");
  GURL url3("ipfs:dogs");
  const auto* registry = GetProtocolHandlersRegistry();
  content::WebContents* web_contents = GetWebContents();

  constexpr const char kManifestV1[] =
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

  constexpr const char kManifestV2[] =
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
  ASSERT_FALSE(registry->IsHandledProtocol(url1.GetScheme()));
  ASSERT_FALSE(registry->IsHandledProtocol(url2.GetScheme()));
  ASSERT_TRUE(registry->IsHandledProtocol(url3.GetScheme()));

  // Test the old handlers are not used.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_EQ(GURL("about:blank"), web_contents->GetLastCommittedURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  ASSERT_EQ(GURL("about:blank"), web_contents->GetLastCommittedURL());

  // Test the new handlers.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url3));
  ASSERT_EQ(GURL("https://www.bing.com/search?q=ipfs%3Adogs"),
            web_contents->GetLastCommittedURL());
}

// Registration via the Extension API when there is a protocol handler for the
// same 'scheme' already registered through the Web API.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       ExtensionRegistrationConflictSameScheme) {
  constexpr const char kScheme1[] = "web+ecsearch";
  constexpr const char kScheme2[] = "web+ducksearch";
  auto* registry = GetProtocolHandlersRegistry();
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
  constexpr const char kScheme1[] = "web+ecsearch";
  constexpr const char kScheme2[] = "web+ducksearch";
  auto* registry = GetProtocolHandlersRegistry();

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
  constexpr const char kScheme1[] = "web+ecsearch";
  constexpr const char kScheme2[] = "web+ducksearch";
  auto* registry = GetProtocolHandlersRegistry();
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
  ASSERT_FALSE(registry->IsHandledProtocol(kScheme1));
  ASSERT_TRUE(registry->IsHandledProtocol(kScheme2));
}

// Test the conflict resolution between 2 extensions registering different
// handlers for the same scheme.
IN_PROC_BROWSER_TEST_F(ProtocolHandlersManagerBrowserTest,
                       TwoExtensionRegistrationConflictSameScheme) {
  constexpr const char kScheme1[] = "web+ecsearch";
  constexpr const char kScheme2[] = "web+ducksearch";
  auto* registry = GetProtocolHandlersRegistry();

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
  constexpr const char kManifest[] =
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
  auto* registry = GetProtocolHandlersRegistry();

  // Protocol handler registration through the Extension API.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kExtensionPath)));
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme1).size());
  ASSERT_EQ(1u, registry->GetHandlersFor(kScheme2).size());
  auto extAPI_handler1 =
      CreateExtensionProtocolHandler(kScheme1, url, last_loaded_extension_id());
  ASSERT_TRUE(registry->IsDefault(extAPI_handler1));

  // Another extension declaring a the same protocol handler, which
  // should replace the one registered by the previous extension.
  constexpr const char kManifest[] =
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
  ASSERT_FALSE(registry->IsHandledProtocol(kScheme1));
  ASSERT_TRUE(registry->IsHandledProtocol(kScheme2));
}

}  // namespace extensions
