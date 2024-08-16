// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {
namespace {

void RestrictProcessCount() {
  // Set the maximum number of processes to 1.  This is a soft limit that
  // we're allowed to exceed if processes *must* not share, which is the case
  // for cross-origin-isolated contexts vs non-cross-origin-isolated
  // contexts.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
}

class CrossOriginIsolationTest : public ExtensionBrowserTest {
 public:
  CrossOriginIsolationTest() = default;
  ~CrossOriginIsolationTest() override = default;
  CrossOriginIsolationTest(const CrossOriginIsolationTest&) = delete;
  CrossOriginIsolationTest& operator=(const CrossOriginIsolationTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  struct Options {
    const char* coep_value = nullptr;
    const char* coop_value = nullptr;
    bool use_service_worker = false;
    const char* background_script = "";
    const char* test_js = "";
    bool is_platform_app = false;
  };
  const Extension* LoadExtension(TestExtensionDir& dir,
                                 const Options& options) {
    CHECK(options.coep_value);
    CHECK(options.coop_value);
    CHECK(!options.is_platform_app || !options.use_service_worker)
        << "Platform apps cannot use 'service_worker' key.";

    static constexpr char kManifestTemplate[] = R"(
      {
        %s,
        %s
        "manifest_version": 2,
        "name": "CrossOriginIsolation",
        "version": "1.1",
        "cross_origin_embedder_policy": {
          "value": "%s"
        },
        "cross_origin_opener_policy": {
          "value": "%s"
        },
        "permissions": ["http://foo.test:*/*"]
      }
    )";

    const char* background_script = nullptr;
    const char* extension_only_keys = R"(
      "web_accessible_resources": ["test.html"],
      "browser_action": {
          "default_title": "foo"
      },
    )";
    if (options.is_platform_app) {
      background_script = R"(
        "app": {
          "background": {
            "scripts": ["background.js"]
          }
        }
      )";
      extension_only_keys = "";
    } else if (options.use_service_worker) {
      background_script = R"(
        "background" : {
          "service_worker": "background.js"
        }
      )";
    } else {
      background_script = R"(
        "background": {
          "scripts": ["background.js"]
        }
      )";
    }

    CHECK(background_script);
    std::string manifest = base::StringPrintf(
        kManifestTemplate, background_script, extension_only_keys,
        options.coep_value, options.coop_value);
    dir.WriteManifest(manifest);
    dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                  options.background_script);
    dir.WriteFile(FILE_PATH_LITERAL("test.html"),
                  "<script src='test.js'></script>");
    dir.WriteFile(FILE_PATH_LITERAL("test.js"), options.test_js);
    return ExtensionBrowserTest::LoadExtension(dir.UnpackedPath());
  }

  bool IsCrossOriginIsolated(content::RenderFrameHost* host) {
    return content::EvalJs(host, "crossOriginIsolated").ExtractBool();
  }

  content::RenderFrameHost* GetBackgroundRenderFrameHost(
      const Extension& extension) {
    ExtensionHost* host =
        ProcessManager::Get(profile())->GetBackgroundHostForExtension(
            extension.id());
    return host ? host->main_frame_host() : nullptr;
  }
};

// Tests that extensions can opt into cross origin isolation.
IN_PROC_BROWSER_TEST_F(CrossOriginIsolationTest, CrossOriginIsolation) {
  RestrictProcessCount();

  TestExtensionDir coi_test_dir;
  const Extension* coi_extension = LoadExtension(
      coi_test_dir,
      {.coep_value = "require-corp", .coop_value = "same-origin"});
  ASSERT_TRUE(coi_extension);
  content::RenderFrameHost* coi_background_render_frame_host =
      GetBackgroundRenderFrameHost(*coi_extension);
  ASSERT_TRUE(coi_background_render_frame_host);
  EXPECT_TRUE(IsCrossOriginIsolated(coi_background_render_frame_host));

  TestExtensionDir non_coi_test_dir;
  const Extension* non_coi_extension =
      LoadExtension(non_coi_test_dir,
                    {.coep_value = "unsafe-none", .coop_value = "same-origin"});
  ASSERT_TRUE(non_coi_extension);
  content::RenderFrameHost* non_coi_background_render_frame_host =
      GetBackgroundRenderFrameHost(*non_coi_extension);
  ASSERT_TRUE(non_coi_background_render_frame_host);
  EXPECT_FALSE(IsCrossOriginIsolated(non_coi_background_render_frame_host));

  // A cross-origin-isolated extension should not share a process with a
  // non-cross-origin-isolated one.
  EXPECT_NE(coi_background_render_frame_host->GetProcess(),
            non_coi_background_render_frame_host->GetProcess());
}

// Tests the interaction of Cross-Origin-Embedder-Policy with extension host
// permissions. See crbug.com/1246109.
IN_PROC_BROWSER_TEST_F(CrossOriginIsolationTest,
                       CrossOriginEmbedderPolicy_HostPermissions) {
  TestExtensionDir test_dir_1;
  const Extension* coep_strict_extension = LoadExtension(
      test_dir_1, {.coep_value = "require-corp", .coop_value = "unsafe-none"});
  ASSERT_TRUE(coep_strict_extension);
  content::RenderFrameHost* coep_strict_background_render_frame_host =
      GetBackgroundRenderFrameHost(*coep_strict_extension);
  ASSERT_TRUE(coep_strict_background_render_frame_host);

  TestExtensionDir test_dir_2;
  const Extension* coep_lax_extension = LoadExtension(
      test_dir_2, {.coep_value = "unsafe-none", .coop_value = "unsafe-none"});
  ASSERT_TRUE(coep_lax_extension);
  content::RenderFrameHost* coep_lax_background_render_frame_host =
      GetBackgroundRenderFrameHost(*coep_lax_extension);
  ASSERT_TRUE(coep_lax_background_render_frame_host);

  auto test_image_load = [](content::RenderFrameHost* render_frame_host,
                            const GURL& image_url) -> std::string {
    static constexpr char kScript[] = R"(
      (() => {
        let img = document.createElement('img');
        return new Promise(resolve => {
          img.addEventListener('load', () => {
            resolve('Success');
          });
          img.addEventListener('error', (e) => {
            resolve('Load failed');
          });
          img.src = $1;
          document.body.appendChild(img);
        });
      })();
    )";

    return content::EvalJs(render_frame_host,
                           content::JsReplace(kScript, image_url))
        .ExtractString();
  };

  GURL image_url_with_host_permissions =
      embedded_test_server()->GetURL("foo.test", "/load_image/image.png");
  GURL image_url_without_host_permissions =
      embedded_test_server()->GetURL("bar.test", "/load_image/image.png");

  // Allowed since cross origin embedding is allowed unless COEP: require-corp.
  EXPECT_EQ("Success", test_image_load(coep_lax_background_render_frame_host,
                                       image_url_with_host_permissions));
  EXPECT_EQ("Success", test_image_load(coep_lax_background_render_frame_host,
                                       image_url_without_host_permissions));

  // Disallowed due to COEP: require-corp.
  // TODO(crbug.com/40789023): Should host permissions override behavior here?
  EXPECT_EQ("Load failed",
            test_image_load(coep_strict_background_render_frame_host,
                            image_url_with_host_permissions));
  EXPECT_EQ("Load failed",
            test_image_load(coep_strict_background_render_frame_host,
                            image_url_without_host_permissions));
}

// Tests that platform apps can opt into cross origin isolation.
IN_PROC_BROWSER_TEST_F(CrossOriginIsolationTest,
                       CrossOriginIsolation_PlatformApps) {
  RestrictProcessCount();

  TestExtensionDir coi_test_dir;
  const Extension* coi_app =
      LoadExtension(coi_test_dir, {.coep_value = "require-corp",
                                   .coop_value = "same-origin",
                                   .is_platform_app = true});
  ASSERT_TRUE(coi_app);
  ASSERT_TRUE(coi_app->is_platform_app());
  content::RenderFrameHost* coi_app_background_render_frame_host =
      GetBackgroundRenderFrameHost(*coi_app);
  ASSERT_TRUE(coi_app_background_render_frame_host);
  EXPECT_TRUE(IsCrossOriginIsolated(coi_app_background_render_frame_host));

  TestExtensionDir non_coi_test_dir;
  const Extension* non_coi_extension =
      LoadExtension(non_coi_test_dir,
                    {.coep_value = "unsafe-none", .coop_value = "same-origin"});
  ASSERT_TRUE(non_coi_extension);
  content::RenderFrameHost* non_coi_background_render_frame_host =
      GetBackgroundRenderFrameHost(*non_coi_extension);
  ASSERT_TRUE(non_coi_background_render_frame_host);
  EXPECT_FALSE(IsCrossOriginIsolated(non_coi_background_render_frame_host));

  // A cross-origin-isolated platform app should not share a process with a
  // non-cross-origin-isolated extension.
  EXPECT_NE(coi_app_background_render_frame_host->GetProcess(),
            non_coi_background_render_frame_host->GetProcess());
}

// Tests that a web accessible frame from a cross origin isolated extension is
// not cross origin isolated.
IN_PROC_BROWSER_TEST_F(CrossOriginIsolationTest, WebAccessibleFrame) {
  RestrictProcessCount();

  TestExtensionDir coi_test_dir;
  const Extension* coi_extension = LoadExtension(
      coi_test_dir,
      {.coep_value = "require-corp", .coop_value = "same-origin"});
  ASSERT_TRUE(coi_extension);
  content::RenderFrameHost* coi_background_render_frame_host =
      GetBackgroundRenderFrameHost(*coi_extension);
  ASSERT_TRUE(coi_background_render_frame_host);
  EXPECT_TRUE(IsCrossOriginIsolated(coi_background_render_frame_host));

  GURL extension_test_url = coi_extension->GetResourceURL("test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_test_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsCrossOriginIsolated(web_contents->GetPrimaryMainFrame()));
  EXPECT_EQ(web_contents->GetPrimaryMainFrame()->GetProcess(),
            coi_background_render_frame_host->GetProcess());

  // Load test.html as a web accessible resource inside a web frame.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html")));
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, "test", extension_test_url));

  content::RenderFrameHost* extension_iframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(extension_iframe);
  EXPECT_EQ(extension_test_url, extension_iframe->GetLastCommittedURL());

  // The extension iframe is embedded within a web frame and won't be cross
  // origin isolated. It should also not share a process with the extension's
  // cross origin isolated context, nor with the web frame it's embedded in.
  EXPECT_FALSE(IsCrossOriginIsolated(extension_iframe));
  EXPECT_NE(extension_iframe->GetProcess(),
            coi_background_render_frame_host->GetProcess());
  EXPECT_NE(extension_iframe->GetProcess(),
            web_contents->GetPrimaryMainFrame()->GetProcess());

  // Check ProcessManager APIs to ensure they work correctly for the case where
  // an extension has multiple processes for the same profile.
  {
    ProcessManager* process_manager = ProcessManager::Get(profile());
    ASSERT_TRUE(process_manager);
    std::set<content::RenderFrameHost*> extension_hosts =
        process_manager->GetRenderFrameHostsForExtension(coi_extension->id());
    EXPECT_THAT(extension_hosts,
                ::testing::UnorderedElementsAre(
                    coi_background_render_frame_host, extension_iframe));

    EXPECT_EQ(coi_extension, process_manager->GetExtensionForRenderFrameHost(
                                 coi_background_render_frame_host));
    EXPECT_EQ(coi_extension, process_manager->GetExtensionForRenderFrameHost(
                                 extension_iframe));
  }

  // Check ProcessMap APIs to ensure they work correctly for the case where an
  // extension has multiple processes for the same profile.
  {
    ProcessMap* process_map = ProcessMap::Get(profile());
    ASSERT_TRUE(process_map);
    EXPECT_TRUE(process_map->Contains(
        coi_extension->id(),
        coi_background_render_frame_host->GetProcess()->GetID()));
    EXPECT_TRUE(process_map->Contains(coi_extension->id(),
                                      extension_iframe->GetProcess()->GetID()));

    GURL* url = nullptr;
    EXPECT_EQ(
        mojom::ContextType::kPrivilegedExtension,
        process_map->GetMostLikelyContextType(
            coi_extension,
            coi_background_render_frame_host->GetProcess()->GetID(), url));
    EXPECT_EQ(mojom::ContextType::kPrivilegedExtension,
              process_map->GetMostLikelyContextType(
                  coi_extension, extension_iframe->GetProcess()->GetID(), url));
  }

  // Ensure both cross-origin-isolated and non-cross-origin-isolated extension
  // contexts inherit extension's cross-origin privileges.
  {
    auto execute_fetch = [](content::RenderFrameHost* host, const GURL& url) {
      static constexpr char kScript[] = R"(
        fetch('%s')
          .then(response => response.text())
          .catch(err => "Fetch error: " + err);
      )";
      std::string script = base::StringPrintf(kScript, url.spec().c_str());
      return content::EvalJs(host, script).ExtractString();
    };
    // Sanity check that fetching a url the extension doesn't have access to,
    // leads to a fetch error.
    const char* kPath = "/extensions/test_file.txt";
    GURL disallowed_url = embedded_test_server()->GetURL("bar.test", kPath);
    EXPECT_THAT(execute_fetch(coi_background_render_frame_host, disallowed_url),
                ::testing::HasSubstr("Fetch error:"));

    GURL allowed_url = embedded_test_server()->GetURL("foo.test", kPath);
    EXPECT_EQ("Hello!",
              execute_fetch(coi_background_render_frame_host, allowed_url));
    EXPECT_EQ("Hello!", execute_fetch(extension_iframe, allowed_url));
  }

  // Finally make some extension API calls to ensure both cross-origin-isolated
  // and non-cross-origin-isolated extension contexts are considered
  // "privileged".
  {
    auto verify_is_privileged_context = [](content::RenderFrameHost* host) {
      const char* kScript = R"(
        new Promise(resolve => {
          chrome.browserAction.getTitle({}, title => {
            resolve(title);
          });
        });
      )";
      EXPECT_EQ("foo", content::EvalJs(host, kScript));
    };

    {
      SCOPED_TRACE(
          "Verifying coi extension background is a privileged context.");
      verify_is_privileged_context(coi_background_render_frame_host);
    }
    {
      SCOPED_TRACE(
          "Verifying non-coi extension iframe is a privileged context.");
      verify_is_privileged_context(extension_iframe);
    }
  }
}

// Test that an extension service worker for a cross origin isolated extension
// is not cross origin isolated. See crbug.com/1131404.
IN_PROC_BROWSER_TEST_F(CrossOriginIsolationTest, ServiceWorker) {
  RestrictProcessCount();

  static constexpr char kServiceWorkerScript[] = R"(
    const readyMessage = crossOriginIsolated ?
        'crossOriginIsolated' : 'notCrossOriginIsolated';
    chrome.test.sendMessage(readyMessage, () => {});
  )";

  ExtensionTestMessageListener ready_listener("notCrossOriginIsolated");
  TestExtensionDir coi_test_dir;
  const Extension* coi_extension =
      LoadExtension(coi_test_dir, {.coep_value = "require-corp",
                                   .coop_value = "same-origin",
                                   .use_service_worker = true,
                                   .background_script = kServiceWorkerScript});
  ASSERT_TRUE(coi_extension);
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

  GURL extension_test_url = coi_extension->GetResourceURL("test.html");
  content::RenderFrameHost* extension_tab =
      ui_test_utils::NavigateToURL(browser(), extension_test_url);
  ASSERT_TRUE(extension_tab);

  // The service worker should be active since it's waiting for a response to
  // chrome.test.sendMessage call.
  std::vector<WorkerId> service_workers =
      ProcessManager::Get(profile())->GetServiceWorkersForExtension(
          coi_extension->id());
  ASSERT_EQ(1u, service_workers.size());

  // Sanity checking that the service worker (non-cross-origin-isolated) and the
  // extension tab (cross-origin-isolated) don't share a process.
  content::RenderProcessHost* service_worker_process =
      content::RenderProcessHost::FromID(service_workers[0].render_process_id);
  ASSERT_TRUE(service_worker_process);
  EXPECT_NE(service_worker_process, extension_tab->GetProcess());

  // Check ProcessMap APIs to ensure they work correctly for the case where an
  // extension has multiple processes for the same profile.
  ProcessMap* process_map = ProcessMap::Get(profile());
  ASSERT_TRUE(process_map);
  EXPECT_TRUE(process_map->Contains(coi_extension->id(),
                                    extension_tab->GetProcess()->GetID()));
  EXPECT_TRUE(process_map->Contains(coi_extension->id(),
                                    service_worker_process->GetID()));

  GURL* url = nullptr;
  EXPECT_EQ(mojom::ContextType::kPrivilegedExtension,
            process_map->GetMostLikelyContextType(
                coi_extension, extension_tab->GetProcess()->GetID(), url));
  EXPECT_EQ(mojom::ContextType::kPrivilegedExtension,
            process_map->GetMostLikelyContextType(
                coi_extension, service_worker_process->GetID(), url));
}

// Tests certain extension APIs which retrieve in-process extension windows.
// Test these for a cross origin isolated extension with non-cross origin
// isolated contexts.
IN_PROC_BROWSER_TEST_F(CrossOriginIsolationTest,
                       WebAccessibleFrame_WindowApis) {
  TestExtensionDir coi_test_dir;
  const Extension* coi_extension = LoadExtension(
      coi_test_dir,
      {.coep_value = "require-corp", .coop_value = "same-origin"});
  ASSERT_TRUE(coi_extension);
  content::RenderFrameHost* coi_background_render_frame_host =
      GetBackgroundRenderFrameHost(*coi_extension);
  ASSERT_TRUE(coi_background_render_frame_host);

  GURL extension_test_url = coi_extension->GetResourceURL("test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, "test", extension_test_url));
  content::RenderFrameHost* extension_iframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(extension_iframe);

  content::RenderFrameHost* extension_tab =
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), extension_test_url,
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(extension_tab);

  // getBackgroundPage API.
  {
    auto test_get_background_page = [](content::RenderFrameHost* host,
                                       bool expect_background_page) {
      static constexpr char kScript[] = R"(
        const expectBackgroundPage = %s;
        const hasBackgroundPage = !!chrome.extension.getBackgroundPage();
        hasBackgroundPage === expectBackgroundPage;
      )";
      EXPECT_EQ(true, content::EvalJs(host, base::StringPrintf(
                                                kScript, expect_background_page
                                                             ? "true"
                                                             : "false")));
    };

    test_get_background_page(coi_background_render_frame_host, true);
    test_get_background_page(extension_tab, true);

    // The extension iframe should be non-cross origin isolated and hence in a
    // different process than the extension background page. Since the API can
    // only retrieve the background page if it's in the same process,
    // getBackgroundPage should return null here.
    test_get_background_page(extension_iframe, false);
  }

  // getViews API.
  {
    auto verify_get_tabs = [](content::RenderFrameHost* host,
                              int num_tabs_expected) {
      static constexpr char kScript[] = R"(
        const numTabsExpected = %d;
        const tabs = chrome.extension.getViews({type: 'tab'});
        tabs.length === numTabsExpected;
      )";
      EXPECT_EQ(true, content::EvalJs(host, base::StringPrintf(
                                                kScript, num_tabs_expected)));
    };

    verify_get_tabs(coi_background_render_frame_host, 1);
    verify_get_tabs(extension_tab, 1);

    // The extension iframe should be non-cross origin isolated and hence in a
    // different process than the background page. Since the API can only
    // retrieve windows in the same process, no windows will be returned.
    verify_get_tabs(extension_iframe, 0);
  }
}

// Tests extension messaging between cross origin isolated and
// non-cross-origin-isolated frames of an extension.
IN_PROC_BROWSER_TEST_F(CrossOriginIsolationTest, ExtensionMessaging_Frames) {
  RestrictProcessCount();

  static constexpr char kTestJs[] = R"(
      function inIframe () {
        try {
          // Accessing `window.top` may raise an error due to the same origin
          // policy.
          return window.self !== window.top;
        } catch (e) {
          return true;
        }
      }

      chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
        if (message !== 'hello') {
          sendResponse('Unexpected message in test script ' + message);
          return;
        }

        if (inIframe())
          sendResponse('ack-from-iframe');
        else
          sendResponse('ack-from-tab');
      });
  )";

  TestExtensionDir coi_test_dir;
  const Extension* coi_extension =
      LoadExtension(coi_test_dir, {.coep_value = "require-corp",
                                   .coop_value = "same-origin",
                                   .test_js = kTestJs});
  ASSERT_TRUE(coi_extension);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL extension_test_url = coi_extension->GetResourceURL("test.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, "test", extension_test_url));
  content::RenderFrameHost* extension_iframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(extension_iframe);

  content::RenderFrameHost* extension_tab =
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), extension_test_url,
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(extension_tab);

  // `extension_iframe` and `extension_tab` should not share a process as they
  // are non-cross-origin-isolated and cross-origin-isolated respectively.
  EXPECT_NE(extension_iframe->GetProcess(), extension_tab->GetProcess());

  // However they should be able to use extension messaging to communicate.
  auto test_messaging = [](content::RenderFrameHost* source,
                           content::RenderFrameHost* destination,
                           const char* expected_response) {
    static constexpr char kScript[] = R"(
      chrome.runtime.sendMessage('hello', response => {
        chrome.test.assertNoLastError();
        chrome.test.assertEq($1, response);
        chrome.test.succeed();
      });
    )";

    ResultCatcher catcher;
    ASSERT_TRUE(content::ExecJs(
        source, content::JsReplace(kScript, expected_response)));
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  };

  {
    SCOPED_TRACE("Message from iframe to tab.");
    test_messaging(extension_iframe, extension_tab, "ack-from-tab");
  }

  {
    SCOPED_TRACE("Message from tab to iframe.");
    test_messaging(extension_tab, extension_iframe, "ack-from-iframe");
  }
}

// Tests extension messaging between a cross origin isolated extension frame and
// the extension service worker which is not cross origin isolated (and hence in
// a different process).
IN_PROC_BROWSER_TEST_F(CrossOriginIsolationTest,
                       ExtensionMessaging_ServiceWorker) {
  RestrictProcessCount();

  static constexpr char kTestJs[] = R"(
    chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
      console.log('message received');
      if (message !== 'hello-from-service-worker') {
        sendResponse('Invalid message received by tab ' + message);
        return;
      }

      sendResponse('ack-from-tab');
    });
  )";

  static constexpr char kServiceWorkerScript[] = R"(
    chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
      if (message !== 'hello-from-tab') {
        sendResponse('Invalid message received by service worker ' + message);
        return;
      }

      sendResponse('ack-from-service-worker');
    });

    chrome.test.sendMessage('ready', () => {
      chrome.runtime.sendMessage(
          'hello-from-service-worker', response => {
            chrome.test.assertNoLastError();
            chrome.test.assertEq('ack-from-tab', response);
            chrome.test.succeed();
          });
    });
  )";

  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  TestExtensionDir coi_test_dir;
  const Extension* coi_extension =
      LoadExtension(coi_test_dir, {.coep_value = "require-corp",
                                   .coop_value = "same-origin",
                                   .use_service_worker = true,
                                   .background_script = kServiceWorkerScript,
                                   .test_js = kTestJs});
  ASSERT_TRUE(coi_extension);
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

  GURL extension_test_url = coi_extension->GetResourceURL("test.html");
  content::RenderFrameHost* extension_tab =
      ui_test_utils::NavigateToURL(browser(), extension_test_url);
  ASSERT_TRUE(extension_tab);

  {
    SCOPED_TRACE("Message from service worker to tab.");
    ResultCatcher catcher;
    ready_listener.Reply("");
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  {
    SCOPED_TRACE("Message from tab to service worker.");
    static constexpr char kScript[] = R"(
      chrome.runtime.sendMessage('hello-from-tab', response => {
        chrome.test.assertNoLastError();
        chrome.test.assertEq('ack-from-service-worker', response);
        chrome.test.succeed();
      });
    )";
    ResultCatcher catcher;
    ASSERT_TRUE(content::ExecJs(extension_tab, kScript));
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}

// Verify extension resource access if it's in an iframe. Regression test for
// crbug.com/1343610.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ExtensionResourceInIframe) {
  EXPECT_TRUE(embedded_test_server()->Start());

  // Load an extension that has one web accessible resource.
  TestExtensionDir extension_dir;
  static constexpr char kManifestStub[] = R"({
    "name": "Test",
    "version": "0.1",
    "manifest_version": 3,
    "web_accessible_resources": [
      {
        "resources": [ "accessible_resource.html" ],
        "matches": [ "<all_urls>" ]
      }
    ]
  })";
  extension_dir.WriteManifest(kManifestStub);
  extension_dir.WriteFile(FILE_PATH_LITERAL("accessible_resource.html"), "");
  extension_dir.WriteFile(FILE_PATH_LITERAL("inaccessible_resource.html"), "");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  EXPECT_TRUE(extension);

  // Allow navigation from a web frame to a web accessible resource.
  {
    // Navigate the main frame with a renderer initiated navigation to a blank
    // web page. This should succeed.
    const GURL gurl = embedded_test_server()->GetURL("/iframe_blank.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
    content::RenderFrameHost* iframe = content::ChildFrameAt(main_frame, 0);
    EXPECT_TRUE(iframe);

    // Navigate the iframe with a renderer initiated navigation to a web
    // accessible resource. This should succeed.
    GURL target = extension->GetResourceURL("accessible_resource.html");
    content::TestNavigationObserver nav_observer(web_contents);
    EXPECT_TRUE(content::NavigateIframeToURL(web_contents, "test", target));
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(net::OK, nav_observer.last_net_error_code());
    iframe = content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
    EXPECT_EQ(target, iframe->GetLastCommittedURL());
  }

  // Prevent navigation from a web frame to a non-web accessible resource.
  {
    GURL invalid_request_url = GURL(kExtensionInvalidRequestURL);
    net::Error err_blocked_by_client = net::ERR_BLOCKED_BY_CLIENT;

    // Navigate the main frame with a renderer initiated navigation to a blank
    // web page. This should succeed.
    const GURL gurl = embedded_test_server()->GetURL("/iframe_blank.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
    content::RenderFrameHost* iframe = content::ChildFrameAt(main_frame, 0);
    EXPECT_TRUE(iframe);

    // Navigate the iframe with a renderer initiated navigation to an extension
    // resource that isn't a web accessible resource. This should be blocked.
    GURL target = extension->GetResourceURL("inaccessible_resource.html");
    content::TestNavigationObserver nav_observer(web_contents);
    EXPECT_TRUE(content::NavigateIframeToURL(web_contents, "test", target));
    nav_observer.Wait();
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(err_blocked_by_client, nav_observer.last_net_error_code());
    EXPECT_EQ(invalid_request_url, iframe->GetLastCommittedURL());

    // Navigate the iframe with a browser initiated navigation to an extension
    // resource. This should be blocked because the origin is not opaque, as
    // it's embedded in a web context.
    content::TestNavigationObserver reload_observer(web_contents);
    EXPECT_TRUE(iframe->Reload());
    reload_observer.Wait();
    EXPECT_EQ(err_blocked_by_client, reload_observer.last_net_error_code());
    iframe = content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());
    EXPECT_EQ(invalid_request_url, iframe->GetLastCommittedURL());

    // Verify iframe browser initiated navigation (to test real UI behavior).
    iframe = content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
    content::TestNavigationObserver browser_initiated_observer(target);
    NavigateParams params(browser(), target, ui::PAGE_TRANSITION_RELOAD);
    params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
    params.is_renderer_initiated = false;
    params.initiator_origin = embedded_test_server()->GetOrigin();
    browser_initiated_observer.WatchExistingWebContents();
    ui_test_utils::NavigateToURL(&params);
    browser_initiated_observer.Wait();
    EXPECT_EQ(err_blocked_by_client,
              browser_initiated_observer.last_net_error_code());
    EXPECT_FALSE(browser_initiated_observer.last_navigation_succeeded());
    iframe = content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
    EXPECT_EQ(target, iframe->GetLastCommittedURL());
  }
}

}  // namespace
}  // namespace extensions
