// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/features/feature.h"
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

    constexpr char kManifestTemplate[] = R"(
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
    bool result = false;
    if (!content::ExecuteScriptAndExtractBool(
            host, "window.domAutomationController.send(crossOriginIsolated)",
            &result)) {
      ADD_FAILURE() << "Script execution failed";
      return false;
    }

    return result;
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
  content::RenderFrameHost* coi_background_rfh =
      GetBackgroundRenderFrameHost(*coi_extension);
  ASSERT_TRUE(coi_background_rfh);
  EXPECT_TRUE(IsCrossOriginIsolated(coi_background_rfh));

  TestExtensionDir non_coi_test_dir;
  const Extension* non_coi_extension =
      LoadExtension(non_coi_test_dir,
                    {.coep_value = "unsafe-none", .coop_value = "same-origin"});
  ASSERT_TRUE(non_coi_extension);
  content::RenderFrameHost* non_coi_background_rfh =
      GetBackgroundRenderFrameHost(*non_coi_extension);
  ASSERT_TRUE(non_coi_background_rfh);
  EXPECT_FALSE(IsCrossOriginIsolated(non_coi_background_rfh));

  // A cross-origin-isolated extension should not share a process with a
  // non-cross-origin-isolated one.
  EXPECT_NE(coi_background_rfh->GetProcess(),
            non_coi_background_rfh->GetProcess());
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
  content::RenderFrameHost* coi_app_background_rfh =
      GetBackgroundRenderFrameHost(*coi_app);
  ASSERT_TRUE(coi_app_background_rfh);
  EXPECT_TRUE(IsCrossOriginIsolated(coi_app_background_rfh));

  TestExtensionDir non_coi_test_dir;
  const Extension* non_coi_extension =
      LoadExtension(non_coi_test_dir,
                    {.coep_value = "unsafe-none", .coop_value = "same-origin"});
  ASSERT_TRUE(non_coi_extension);
  content::RenderFrameHost* non_coi_background_rfh =
      GetBackgroundRenderFrameHost(*non_coi_extension);
  ASSERT_TRUE(non_coi_background_rfh);
  EXPECT_FALSE(IsCrossOriginIsolated(non_coi_background_rfh));

  // A cross-origin-isolated platform app should not share a process with a
  // non-cross-origin-isolated extension.
  EXPECT_NE(coi_app_background_rfh->GetProcess(),
            non_coi_background_rfh->GetProcess());
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
  content::RenderFrameHost* coi_background_rfh =
      GetBackgroundRenderFrameHost(*coi_extension);
  ASSERT_TRUE(coi_background_rfh);
  EXPECT_TRUE(IsCrossOriginIsolated(coi_background_rfh));

  GURL extension_test_url = coi_extension->GetResourceURL("test.html");
  ui_test_utils::NavigateToURL(browser(), extension_test_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsCrossOriginIsolated(web_contents->GetMainFrame()));
  EXPECT_EQ(web_contents->GetMainFrame()->GetProcess(),
            coi_background_rfh->GetProcess());

  // Load test.html as a web accessible resource inside a web frame.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html"));
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, "test", extension_test_url));

  content::RenderFrameHost* extension_iframe =
      content::ChildFrameAt(web_contents->GetMainFrame(), 0);
  ASSERT_TRUE(extension_iframe);
  EXPECT_EQ(extension_test_url, extension_iframe->GetLastCommittedURL());

  // The extension iframe is embedded within a web frame and won't be cross
  // origin isolated. It should also not share a process with the extension's
  // cross origin isolated context, nor with the web frame it's embedded in.
  EXPECT_FALSE(IsCrossOriginIsolated(extension_iframe));
  EXPECT_NE(extension_iframe->GetProcess(), coi_background_rfh->GetProcess());
  EXPECT_NE(extension_iframe->GetProcess(),
            web_contents->GetMainFrame()->GetProcess());

  // Check ProcessManager APIs to ensure they work correctly for the case where
  // an extension has multiple processes for the same profile.
  {
    ProcessManager* process_manager = ProcessManager::Get(profile());
    ASSERT_TRUE(process_manager);
    std::set<content::RenderFrameHost*> extension_hosts =
        process_manager->GetRenderFrameHostsForExtension(coi_extension->id());
    EXPECT_THAT(extension_hosts, ::testing::UnorderedElementsAre(
                                     coi_background_rfh, extension_iframe));

    EXPECT_EQ(coi_extension, process_manager->GetExtensionForRenderFrameHost(
                                 coi_background_rfh));
    EXPECT_EQ(coi_extension, process_manager->GetExtensionForRenderFrameHost(
                                 extension_iframe));
  }

  // Check ProcessMap APIs to ensure they work correctly for the case where an
  // extension has multiple processes for the same profile.
  {
    ProcessMap* process_map = ProcessMap::Get(profile());
    ASSERT_TRUE(process_map);
    EXPECT_TRUE(process_map->Contains(
        coi_extension->id(), coi_background_rfh->GetProcess()->GetID()));
    EXPECT_TRUE(process_map->Contains(coi_extension->id(),
                                      extension_iframe->GetProcess()->GetID()));

    GURL* url = nullptr;
    EXPECT_EQ(
        Feature::BLESSED_EXTENSION_CONTEXT,
        process_map->GetMostLikelyContextType(
            coi_extension, coi_background_rfh->GetProcess()->GetID(), url));
    EXPECT_EQ(Feature::BLESSED_EXTENSION_CONTEXT,
              process_map->GetMostLikelyContextType(
                  coi_extension, extension_iframe->GetProcess()->GetID(), url));
  }

  // Ensure both cross-origin-isolated and non-cross-origin-isolated extension
  // contexts inherit extension's cross-origin privileges.
  {
    auto execute_fetch = [](content::RenderFrameHost* host, const GURL& url) {
      const char* kScript = R"(
        fetch('%s')
          .then(response => response.text())
          .then(text => window.domAutomationController.send(text))
          .catch(err => window.domAutomationController.send(
            "Fetch error: " + err));
      )";
      std::string script = base::StringPrintf(kScript, url.spec().c_str());
      std::string result;
      if (!content::ExecuteScriptAndExtractString(host, script, &result))
        return std::string("Error executing script");
      return result;
    };
    // Sanity check that fetching a url the extension doesn't have access to,
    // leads to a fetch error.
    const char* kPath = "/extensions/test_file.txt";
    GURL disallowed_url = embedded_test_server()->GetURL("bar.test", kPath);
    EXPECT_THAT(execute_fetch(coi_background_rfh, disallowed_url),
                ::testing::HasSubstr("Fetch error:"));

    GURL allowed_url = embedded_test_server()->GetURL("foo.test", kPath);
    EXPECT_EQ("Hello!", execute_fetch(coi_background_rfh, allowed_url));
    EXPECT_EQ("Hello!", execute_fetch(extension_iframe, allowed_url));
  }

  // Finally make some extension API calls to ensure both cross-origin-isolated
  // and non-cross-origin-isolated extension contexts are considered "blessed".
  {
    auto verify_is_blessed_context = [](content::RenderFrameHost* host) {
      std::string result;
      const char* kScript = R"(
        chrome.browserAction.getTitle({}, title => {
          window.domAutomationController.send(title);
        });
      )";
      ASSERT_TRUE(
          content::ExecuteScriptAndExtractString(host, kScript, &result));
      EXPECT_EQ("foo", result);
    };

    {
      SCOPED_TRACE("Verifying coi extension background is a blessed context.");
      verify_is_blessed_context(coi_background_rfh);
    }
    {
      SCOPED_TRACE("Verifying non-coi extension iframe is a blessed context.");
      verify_is_blessed_context(extension_iframe);
    }
  }
}

// Test that an extension service worker for a cross origin isolated extension
// is not cross origin isolated. See crbug.com/1131404.
IN_PROC_BROWSER_TEST_F(CrossOriginIsolationTest, ServiceWorker) {
  RestrictProcessCount();

  constexpr char kServiceWorkerScript[] = R"(
    const readyMessage = crossOriginIsolated ?
        'crossOriginIsolated' : 'notCrossOriginIsolated';
    chrome.test.sendMessage(readyMessage, () => {});
  )";

  ExtensionTestMessageListener ready_listener("notCrossOriginIsolated",
                                              true /* will_reply */);
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
  EXPECT_EQ(Feature::BLESSED_EXTENSION_CONTEXT,
            process_map->GetMostLikelyContextType(
                coi_extension, extension_tab->GetProcess()->GetID(), url));
  EXPECT_EQ(Feature::BLESSED_EXTENSION_CONTEXT,
            process_map->GetMostLikelyContextType(
                coi_extension, service_worker_process->GetID(), url));

  // Reply to the service worker. This is not useful for the test but is
  // required by ExtensionTestMessageListener.
  ready_listener.Reply("");
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
  content::RenderFrameHost* coi_background_rfh =
      GetBackgroundRenderFrameHost(*coi_extension);
  ASSERT_TRUE(coi_background_rfh);

  GURL extension_test_url = coi_extension->GetResourceURL("test.html");
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, "test", extension_test_url));
  content::RenderFrameHost* extension_iframe =
      content::ChildFrameAt(web_contents->GetMainFrame(), 0);
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
      constexpr char kScript[] = R"(
        const expectBackgroundPage = %s;
        const hasBackgroundPage = !!chrome.extension.getBackgroundPage();
        window.domAutomationController.send(
            hasBackgroundPage === expectBackgroundPage);
      )";
      bool result = false;
      ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
          host,
          base::StringPrintf(kScript,
                             expect_background_page ? "true" : "false"),
          &result));
      EXPECT_TRUE(result);
    };

    test_get_background_page(coi_background_rfh, true);
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
      constexpr char kScript[] = R"(
        const numTabsExpected = %d;
        const tabs = chrome.extension.getViews({type: 'tab'});
        window.domAutomationController.send(tabs.length === numTabsExpected);
      )";
      bool result = false;
      ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
          host, base::StringPrintf(kScript, num_tabs_expected), &result));
      EXPECT_TRUE(result);
    };

    verify_get_tabs(coi_background_rfh, 1);
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

  constexpr char kTestJs[] = R"(
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

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL extension_test_url = coi_extension->GetResourceURL("test.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, "test", extension_test_url));
  content::RenderFrameHost* extension_iframe =
      content::ChildFrameAt(web_contents->GetMainFrame(), 0);
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
    constexpr char kScript[] = R"(
      chrome.runtime.sendMessage('hello', response => {
        chrome.test.assertNoLastError();
        chrome.test.assertEq($1, response);
        chrome.test.succeed();
      });
    )";

    ResultCatcher catcher;
    ASSERT_TRUE(content::ExecuteScript(
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

  constexpr char kTestJs[] = R"(
    chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
      console.log('message received');
      if (message !== 'hello-from-service-worker') {
        sendResponse('Invalid message received by tab ' + message);
        return;
      }

      sendResponse('ack-from-tab');
    });
  )";

  constexpr char kServiceWorkerScript[] = R"(
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

  ExtensionTestMessageListener ready_listener("ready", true /* will_reply */);
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
    constexpr char kScript[] = R"(
      chrome.runtime.sendMessage('hello-from-tab', response => {
        chrome.test.assertNoLastError();
        chrome.test.assertEq('ack-from-service-worker', response);
        chrome.test.succeed();
      });
    )";
    ResultCatcher catcher;
    ASSERT_TRUE(content::ExecuteScript(extension_tab, kScript));
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}

}  // namespace
}  // namespace extensions
