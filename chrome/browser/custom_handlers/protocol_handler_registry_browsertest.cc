// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/protocol_handler_registry.h"

#include <memory>
#include <string>
#include <vector>

#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/test/base/launchservices_utils_mac.h"
#endif

using content::WebContents;
using custom_handlers::ProtocolHandler;
using custom_handlers::ProtocolHandlerRegistry;

namespace {

std::string EncodeUrl(const std::string& not_encoded) {
  url::RawCanonOutputT<char> encoded;
  url::EncodeURIComponent(not_encoded, &encoded);

  return {encoded.data(), encoded.length()};
}

class ProtocolHandlerChangeWaiter : public ProtocolHandlerRegistry::Observer {
 public:
  explicit ProtocolHandlerChangeWaiter(ProtocolHandlerRegistry* registry) {
    registry_observation_.Observe(registry);
  }
  ProtocolHandlerChangeWaiter(const ProtocolHandlerChangeWaiter&) = delete;
  ProtocolHandlerChangeWaiter& operator=(const ProtocolHandlerChangeWaiter&) =
      delete;
  ~ProtocolHandlerChangeWaiter() override = default;

  void Wait() { run_loop_.Run(); }
  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override { run_loop_.Quit(); }

 private:
  base::ScopedObservation<custom_handlers::ProtocolHandlerRegistry,
                          custom_handlers::ProtocolHandlerRegistry::Observer>
      registry_observation_{this};
  base::RunLoop run_loop_;
};

}  // namespace

class ChromeRegisterProtocolHandlerBrowserTest : public InProcessBrowserTest {
 public:
  ChromeRegisterProtocolHandlerBrowserTest() = default;

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_MAC)
    ASSERT_TRUE(test::RegisterAppWithLaunchServices());
#endif

    // We might define browser tests for other embedders, so the test's data
    // files will be shared via //componennts
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/custom_handlers/");
  }

  TestRenderViewContextMenu* CreateContextMenu(GURL url) {
    content::ContextMenuParams params;
    params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
    params.link_url = url;
    params.unfiltered_link_url = url;
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    params.page_url =
        web_contents->GetController().GetLastCommittedEntry()->GetURL();
#if BUILDFLAG(IS_MAC)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif  // BUILDFLAG(IS_MAC)
    TestRenderViewContextMenu* menu =
        new TestRenderViewContextMenu(*browser()
                                           ->tab_strip_model()
                                           ->GetActiveWebContents()
                                           ->GetPrimaryMainFrame(),
                                      params);
    menu->Init();
    return menu;
  }

  void AddProtocolHandler(const std::string& protocol, const GURL& url) {
    ProtocolHandler handler =
        ProtocolHandler::CreateProtocolHandler(protocol, url);
    ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser()->profile());
    // Fake that this registration is happening on profile startup. Otherwise
    // it'll try to register with the OS, which causes DCHECKs on Windows when
    // running as admin on Windows 7.
    registry->SetIsLoading(true);
    registry->OnAcceptRegisterProtocolHandler(handler);
    registry->SetIsLoading(true);
    ASSERT_TRUE(registry->IsHandledProtocol(protocol));
  }

  void RemoveProtocolHandler(const std::string& protocol,
                             const GURL& url) {
    ProtocolHandler handler =
        ProtocolHandler::CreateProtocolHandler(protocol, url);
    ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser()->profile());
    registry->RemoveHandler(handler);
    ASSERT_FALSE(registry->IsHandledProtocol(protocol));
  }

 protected:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(ChromeRegisterProtocolHandlerBrowserTest,
                       ContextMenuEntryAppearsForHandledUrls) {
  std::unique_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(GURL("https://www.google.com/")));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));

  AddProtocolHandler(std::string("web+search"),
                     GURL("https://www.google.com/%s"));
  GURL url("web+search:testing");
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_EQ(1u, registry->GetHandlersFor(url.scheme()).size());
  menu.reset(CreateContextMenu(url));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));
}

IN_PROC_BROWSER_TEST_F(ChromeRegisterProtocolHandlerBrowserTest,
                       UnregisterProtocolHandler) {
  std::unique_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(GURL("https://www.google.com/")));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));

  AddProtocolHandler(std::string("web+search"),
                     GURL("https://www.google.com/%s"));
  GURL url("web+search:testing");
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_EQ(1u, registry->GetHandlersFor(url.scheme()).size());
  menu.reset(CreateContextMenu(url));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));
  RemoveProtocolHandler(std::string("web+search"),
                        GURL("https://www.google.com/%s"));
  ASSERT_EQ(0u, registry->GetHandlersFor(url.scheme()).size());
  menu.reset(CreateContextMenu(url));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));
}

IN_PROC_BROWSER_TEST_F(ChromeRegisterProtocolHandlerBrowserTest,
                       CustomHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL handler_url = embedded_test_server()->GetURL("/custom_handler.html");
  AddProtocolHandler("news", handler_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("news:test")));

  ASSERT_EQ(handler_url, browser()
                             ->tab_strip_model()
                             ->GetActiveWebContents()
                             ->GetLastCommittedURL());

  // Also check redirects.
  GURL redirect_url =
      embedded_test_server()->GetURL("/server-redirect?news:test");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), redirect_url));

  ASSERT_EQ(handler_url, browser()
                             ->tab_strip_model()
                             ->GetActiveWebContents()
                             ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ChromeRegisterProtocolHandlerBrowserTest,
                       IgnoreRequestWithoutUserGesture) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* content_settings =
      PageSpecificContentSettingsDelegate::FromWebContents(web_contents);

  // Ensure the registry is currently empty.
  GURL url("web+search:testing");
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_EQ(0u, registry->GetHandlersFor(url.scheme()).size());

  // Ensure there is no registration pending.
  ASSERT_TRUE(content_settings->pending_protocol_handler().IsEmpty());

  // Attempt to add an entry.
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "navigator.registerProtocolHandler('web+"
                              "search', 'test.html?%s', 'test');",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Verify the registration is ignored if no user gesture involved.
  ASSERT_EQ(0u, registry->GetHandlersFor(url.scheme()).size());

  // Verify the handler registration is pending.
  ASSERT_TRUE(content_settings->pending_protocol_handler().IsValid());
}

// FencedFrames can not register to handle any protocols.
IN_PROC_BROWSER_TEST_F(ChromeRegisterProtocolHandlerBrowserTest, FencedFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  // Create a FencedFrame.
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          browser()
              ->tab_strip_model()
              ->GetActiveWebContents()
              ->GetPrimaryMainFrame(),
          embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  ASSERT_TRUE(fenced_frame_host);

  // Ensure the registry is currently empty.
  GURL url("web+search:testing");
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_EQ(0u, registry->GetHandlersFor(url.scheme()).size());

  // Attempt to add an entry.
  ProtocolHandlerChangeWaiter waiter(registry);
  ASSERT_TRUE(content::ExecJs(fenced_frame_host,
                              "navigator.registerProtocolHandler('web+"
                              "search', 'test.html?%s', 'test');"));
  waiter.Wait();

  // Ensure the registry is still empty.
  ASSERT_EQ(0u, registry->GetHandlersFor(url.scheme()).size());
}

using RegisterProtocolHandlerExtensionBrowserTest =
    extensions::ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(RegisterProtocolHandlerExtensionBrowserTest, Basic) {
#if BUILDFLAG(IS_MAC)
  ASSERT_TRUE(test::RegisterAppWithLaunchServices());
#endif
  permissions::PermissionRequestManager::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("protocol_handler"));
  ASSERT_NE(nullptr, extension);

  std::string handler_url =
      "chrome-extension://" + extension->id() + "/test.html";

  // Register the handler.
  {
    ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser()->profile());
    ProtocolHandlerChangeWaiter waiter(registry);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(handler_url)));
    ASSERT_TRUE(content::ExecJs(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "navigator.registerProtocolHandler('geo', 'test.html?%s', 'test');"));
    waiter.Wait();
  }

  // Test the handler.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("geo:test")));
  ASSERT_EQ(GURL(handler_url + "?geo%3Atest"), browser()
                                                   ->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetLastCommittedURL());
}

class ChromeRegisterProtocolHandlerAndServiceWorkerInterceptor
    : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // We might define browser tests for other embedders, so the test's data
    // files will be shared via //componennts
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/custom_handlers/");

    ASSERT_TRUE(embedded_test_server()->Start());

    // Navigate to the test page.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/protocol_handler/service_workers/"
                       "test_protocol_handler_and_service_workers.html")));

    // Bypass permission dialogs for registering new protocol handlers.
    permissions::PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents())
        ->set_auto_response_for_test(
            permissions::PermissionRequestManager::ACCEPT_ALL);
  }
};

// TODO(crbug.com/40763886): Fix flakiness.
IN_PROC_BROWSER_TEST_F(ChromeRegisterProtocolHandlerAndServiceWorkerInterceptor,
                       DISABLED_RegisterFetchListenerForHTMLHandler) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Register a service worker intercepting requests to the HTML handler.
  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  "registerFetchListenerForHTMLHandler();"));

  {
    // Register a HTML handler with a user gesture.
    ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser()->profile());
    ProtocolHandlerChangeWaiter waiter(registry);
    ASSERT_TRUE(content::ExecJs(web_contents, "registerHTMLHandler();"));
    waiter.Wait();
  }

  // Verify that a page with the registered scheme is managed by the service
  // worker, not the HTML handler.
  EXPECT_EQ(true,
            content::EvalJs(web_contents,
                            "pageWithCustomSchemeHandledByServiceWorker();"));
}

using ChromeRegisterProtocolHandlerIsolatedWebAppsTest =
    web_app::IsolatedWebAppBrowserTestHarness;

IN_PROC_BROWSER_TEST_F(ChromeRegisterProtocolHandlerIsolatedWebAppsTest,
                       Basic) {
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));

  Browser* browser = LaunchWebAppBrowserAndWait(url_info.app_id());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);

  GURL app_url = url_info.origin().GetURL();

  struct TestCase {
    std::string scheme;
    std::string url;
    bool result;
  };

  std::vector<TestCase> test_cases = {
      // non-custom scheme, relative URL (same origin)
      {"geo", "/protocol_handler=", true},
      // non-custom scheme, full URL (same origin)
      {"geo", app_url.spec() + "protocol_handler=", true},
      // non-custom scheme, IWA URL (cross origin)
      {"geo",
       "isolated-app://"
       "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"
       "protocol_handler=",
       false},
      // non-custom scheme, HTTPS URL (cross origin)
      {"geo", "https://www.google.com/search?q=", false},
      //
      // custom scheme (web+), relative URL (same origin)
      {"web+foo", "/protocol_handler=", true},
      // custom scheme (web+), full URL (same origin)
      {"web+foo", app_url.spec() + "protocol_handler=", true},
      // custom scheme (web+), IWA URL (cross origin)
      {"web+foo",
       "isolated-app://"
       "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"
       "protocol_handler=",
       false},
      // custom scheme (web+), HTTPS URL (cross origin)
      {"web+foo", "https://www.google.com/search?q=", false},
      //
      // custom scheme (ext+), relative URL (same origin)
      {"ext+foo", "/protocol_handler=", false},
      // custom scheme (ext+), full URL (same origin)
      {"ext+foo", app_url.spec() + "protocol_handler=", false},
      // custom scheme (ext+), IWA URL (cross origin)
      {"ext+foo",
       "isolated-app://"
       "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"
       "protocol_handler=",
       false},
      // custom scheme (ext+), HTTPS URL (cross origin)
      {"ext+foo3", "https://www.google.com/search?q=", false},
  };

  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile());

  for (const auto& test_case : test_cases) {
    auto js = content::JsReplace("navigator.registerProtocolHandler($1, $2);",
                                 test_case.scheme, test_case.url + "%s");
    SCOPED_TRACE(testing::Message()
                 << "Registering protocol handler w/ " << js);
    registry->ClearUserDefinedHandlers(base::Time(), base::Time::Max());
    ProtocolHandlerChangeWaiter waiter(registry);

    auto result = content::ExecJs(web_contents->GetPrimaryMainFrame(), js);
    EXPECT_EQ(result, test_case.result);

    if (result) {
      // Wait for the registration to complete and test the handler.
      waiter.Wait();

      EXPECT_TRUE(ui_test_utils::NavigateToURL(
          this->browser(), GURL(test_case.scheme + ":test")));

      std::string expected_url_string =
          test_case.url + EncodeUrl(test_case.scheme + ":test");
      GURL expected_url(expected_url_string);
      // If `expected_url_string` is a relative URL, it will be resolved with
      // `app_url` as the base URL. If `expected_url_string` is an absolute URL,
      // it'll be returned as is.
      expected_url = app_url.Resolve(expected_url_string);
      EXPECT_EQ(expected_url, this->browser()
                                  ->tab_strip_model()
                                  ->GetActiveWebContents()
                                  ->GetLastCommittedURL());
    }
  }
}
