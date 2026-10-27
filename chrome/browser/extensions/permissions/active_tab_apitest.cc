// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/network_permissions_updater.h"
#include "extensions/browser/permissions/active_tab_permission_granter.h"
#include "extensions/browser/permissions/scripting_permissions_modifier.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {
namespace {

// TODO(crbug.com/393179880): Port to desktop Android when ExtensionActionRunner
// is ported.
class ExtensionActiveTabTest : public ExtensionApiTest {
 public:
  ExtensionActiveTabTest() = default;

  ExtensionActiveTabTest(const ExtensionActiveTabTest&) = delete;
  ExtensionActiveTabTest& operator=(const ExtensionActiveTabTest&) = delete;

  // ExtensionApiTest override:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    // Map all hosts to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// TODO(crbug.com/40876361): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(ExtensionActiveTabTest, DISABLED_ActiveTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener background_page_ready("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("active_tab"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(background_page_ready.WaitUntilSatisfied());

  auto* web_contents = GetActiveWebContents();

  // Shouldn't be initially granted based on activeTab.
  {
    ExtensionTestMessageListener navigation_count_listener("1");
    ResultCatcher catcher;
    ASSERT_TRUE(NavigateToURL(
        web_contents,
        embedded_test_server()->GetURL(
            "google.com", "/extensions/api_test/active_tab/page.html")));
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
    EXPECT_TRUE(navigation_count_listener.WaitUntilSatisfied());
  }

  // Do one pass of BrowserAction without granting activeTab permission,
  // extension shouldn't have access to tab.url.
  {
    ResultCatcher catcher;
    ExtensionActionRunner::GetForWebContents(web_contents)
        ->RunAction(extension, false);
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }

  // Granting to the extension should give it access to page.html.
  {
    ResultCatcher catcher;
    {
      PermissionsManagerWaiter waiter(PermissionsManager::Get(profile()));
      ExtensionActionRunner::GetForWebContents(web_contents)
          ->RunAction(extension, true);
      waiter.WaitForActiveTabPermissionGranted(extension->id());
    }
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }

  // Navigating to a different page on the same origin should revoke extension's
  // access to the tab, unless the runtime host permissions feature is enabled.
  {
    ExtensionTestMessageListener navigation_count_listener("2");
    ResultCatcher catcher;
    ASSERT_TRUE(NavigateToURL(
        web_contents,
        embedded_test_server()->GetURL(
            "google.com", "/extensions/api_test/active_tab/final_page.html")));
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
    EXPECT_TRUE(navigation_count_listener.WaitUntilSatisfied());
  }

  // Navigating to a different origin should revoke extension's access to the
  // tab.
  {
    ExtensionTestMessageListener navigation_count_listener("3");
    ResultCatcher catcher;
    ASSERT_TRUE(NavigateToURL(
        web_contents,
        embedded_test_server()->GetURL(
            "example.com", "/extensions/api_test/active_tab/final_page.html")));
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
    EXPECT_TRUE(navigation_count_listener.WaitUntilSatisfied());
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionActiveTabTest, ActiveTabCors) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener background_page_ready("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("active_tab_cors"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(background_page_ready.WaitUntilSatisfied());
  auto* web_contents = GetActiveWebContents();

  {
    ASSERT_TRUE(NavigateToURL(
        web_contents,
        embedded_test_server()->GetURL(
            "google.com", "/extensions/api_test/active_tab_cors/page.html")));
    std::u16string title = u"page";
    content::TitleWatcher watcher(web_contents, title);
    ASSERT_EQ(title, watcher.WaitAndGetTitle());
  }

  {
    // The injected content script has an access to page's origin without
    // explicit permissions other than "activeTab".
    ResultCatcher catcher;
    {
      PermissionsManagerWaiter waiter(PermissionsManager::Get(profile()));
      ExtensionActionRunner::GetForWebContents(web_contents)
          ->RunAction(extension, true);
      waiter.WaitForActiveTabPermissionGranted(extension->id());
    }
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }
}

// Tests for the order of the activeTab grant vs other events, when the
// network service is slow to process the activeTab-related update of
// extension permissions.
//
// Note that, unlike other tests in this file, these tests intentionally do
// *not* wait for `PermissionsManagerWaiter` - real extensions act on the
// activeTab grant as soon as they receive the `action.onClicked` event.
class ExtensionActiveTabWithSlowNetworkServiceTest
    : public ExtensionActiveTabTest {
 protected:
  void TearDownOnMainThread() override {
    // Don't leak a held update (or the hook that captures `this`) if a test
    // fails before it releases the update.
    NetworkPermissionsUpdater::HoldNextUpdateForTesting(base::NullCallback());
    held_update_.Reset();
    ExtensionActiveTabTest::TearDownOnMainThread();
  }
  // Loads an extension with an action and a service worker that runs
  // `background_js`.  Waits until the service worker sends a "ready" message.
  const Extension* LoadExtensionWithAction(std::string_view permissions,
                                           std::string_view background_js) {
    test_dir_.WriteManifest(base::StrCat({R"(
        {
          "name": "ExtensionActiveTabWithSlowNetworkServiceTest",
          "version": "1.0",
          "manifest_version": 3,
          "background": {"service_worker": "background.js"},
          "action": {},
          "permissions": )",
                                          permissions, "}"}));
    test_dir_.WriteFile(FILE_PATH_LITERAL("background.js"),
                        base::StrCat({background_js, R"(
        chrome.test.sendMessage('ready');
    )"}));
    ExtensionTestMessageListener ready_listener("ready");
    const Extension* extension = LoadExtension(test_dir_.UnpackedPath());
    if (!ready_listener.WaitUntilSatisfied()) {
      return nullptr;
    }
    return extension;
  }

  // Holds the next update of extension permissions in the network service
  // (until `ReleaseHeldNetworkServiceUpdate` is called).
  void HoldNextNetworkServiceUpdate() {
    NetworkPermissionsUpdater::HoldNextUpdateForTesting(
        base::BindLambdaForTesting([this](base::OnceClosure send_update) {
          held_update_ = std::move(send_update);
        }));
  }

  bool HasHeldNetworkServiceUpdate() const { return !held_update_.is_null(); }

  void ReleaseHeldNetworkServiceUpdate() {
    CHECK(held_update_);
    std::move(held_update_).Run();
  }

  // Simulates the user clicking the action of `extension`, while holding the
  // resulting update of extension permissions in the network service.  Waits
  // for `result_listener` (e.g. for a reply from the `action.onClicked`
  // handler of the extension).
  void ClickActionWithSlowNetworkService(
      const Extension& extension,
      ExtensionTestMessageListener& result_listener) {
    HoldNextNetworkServiceUpdate();
    TestEventRouterObserver event_observer(EventRouter::Get(profile()));
    ExtensionActionRunner::GetForWebContents(GetActiveWebContents())
        ->RunAction(&extension, /*grant_tab_permissions=*/true);
    ASSERT_TRUE(HasHeldNetworkServiceUpdate());

    // If the `action.onClicked` event has already been dispatched, then keep
    // holding the update until the extension replies (this is the problematic
    // ordering that the tests want to cover).  Otherwise, release the update
    // right away: the event waits for the update, so holding the update until
    // the extension replies would never finish.
    const bool on_clicked_dispatched_before_update =
        event_observer.events().contains("action.onClicked");
    if (!on_clicked_dispatched_before_update) {
      ReleaseHeldNetworkServiceUpdate();
    }
    ASSERT_TRUE(result_listener.WaitUntilSatisfied());
    if (on_clicked_dispatched_before_update) {
      ReleaseHeldNetworkServiceUpdate();
    }
  }

 private:
  TestExtensionDir test_dir_;
  base::OnceClosure held_update_;
};

// JavaScript code that defines a `fetchAndReport(url)` function.  The function
// fetches `url` in 'cors' and in 'no-cors' mode, and sends a message that
// describes the results (see `kExpectedFetchResults`).
constexpr char kFetchAndReportJs[] = R"(
    async function tryFetch(url, mode) {
      try {
        const response = await fetch(url, {mode});
        const text = await response.text();
        const hasTitle = text.includes('Title Of Awesomeness');
        return `${mode}: type=${response.type} hasTitle=${hasTitle}`;
      } catch (e) {
        return `${mode}: ${e.name}`;
      }
    }

    async function fetchAndReport(url) {
      const results = [
        await tryFetch(url, 'cors'),
        await tryFetch(url, 'no-cors'),
      ];
      chrome.test.sendMessage(results.join('; '));
    }
)";

// The message that `fetchAndReport` sends if the extension can read the
// response from `title2.html`.
constexpr char kExpectedFetchResults[] =
    "cors: type=basic hasTitle=true; no-cors: type=basic hasTitle=true";

// Tests that the `action.onClicked` handler can use the activeTab permission
// to inject a script into the tab right away.  Regression test for
// https://crbug.com/570354836.
IN_PROC_BROWSER_TEST_F(ExtensionActiveTabWithSlowNetworkServiceTest,
                       ExecuteScriptFromActionOnClicked) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension =
      LoadExtensionWithAction(R"(["activeTab", "scripting"])", R"(
        chrome.action.onClicked.addListener(async (tab) => {
          try {
            const results = await chrome.scripting.executeScript({
              target: {tabId: tab.id},
              func: () => document.title,
            });
            chrome.test.sendMessage('title: ' + results[0].result);
          } catch (e) {
            chrome.test.sendMessage('error: ' + e.message);
          }
        });
      )");
  ASSERT_TRUE(extension);
  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(),
                    embedded_test_server()->GetURL("a.com", "/title2.html")));

  ExtensionTestMessageListener result_listener;
  ClickActionWithSlowNetworkService(*extension, result_listener);
  EXPECT_EQ("title: Title Of Awesomeness", result_listener.message());
}

// Tests that the `action.onClicked` handler can use the activeTab permission
// to `fetch` from the tab's origin right away.  This requires that CORS and ORB
// in the network service already know about the activeTab grant.
IN_PROC_BROWSER_TEST_F(ExtensionActiveTabWithSlowNetworkServiceTest,
                       FetchFromActionOnClicked) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = LoadExtensionWithAction(
      R"(["activeTab"])", base::StrCat({kFetchAndReportJs, R"(
        chrome.action.onClicked.addListener((tab) => fetchAndReport(tab.url));
      )"}));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(),
                    embedded_test_server()->GetURL("a.com", "/title2.html")));

  ExtensionTestMessageListener result_listener;
  ClickActionWithSlowNetworkService(*extension, result_listener);
  EXPECT_EQ(kExpectedFetchResults, result_listener.message());
}

// Tests that an action popup can use the activeTab permission to `fetch` from
// the tab's origin right away.  Unlike `action.onClicked`, the popup doesn't
// need to wait for the network service update of the activeTab grant: the
// update is sent before the popup frame (and its `URLLoaderFactory`) is
// created, and the network service processes both requests in order.  In this
// test the update is held (i.e. sent too late), but the popup still works,
// because `RendererStartupHelper::ActivateExtensionInProcess` sends another
// update (which already includes the grant) before the network service
// creates a `URLLoaderFactory` for the popup frame.
IN_PROC_BROWSER_TEST_F(ExtensionActiveTabWithSlowNetworkServiceTest,
                       FetchFromActionPopup) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"(
      {
        "name": "FetchFromActionPopup",
        "version": "1.0",
        "manifest_version": 3,
        "action": {"default_popup": "popup.html"},
        "permissions": ["activeTab"]
      } )");
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"),
                     "<script src='popup.js'></script>");
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.js"),
                     base::StrCat({kFetchAndReportJs, R"(
      (async () => {
        const [tab] =
            await chrome.tabs.query({active: true, currentWindow: true});
        await fetchAndReport(tab.url);
      })();
  )"}));
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(),
                    embedded_test_server()->GetURL("a.com", "/title2.html")));

  // Hold the network service update of the activeTab grant until the popup
  // replies.
  HoldNextNetworkServiceUpdate();
  ExtensionTestMessageListener result_listener;
  ExtensionActionTestHelper::Create(browser())->Press(extension->id());
  ASSERT_TRUE(HasHeldNetworkServiceUpdate());
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  ReleaseHeldNetworkServiceUpdate();

  EXPECT_EQ(kExpectedFetchResults, result_listener.message());
}

// Tests that revoking an activeTab grant also revokes it in renderers, even if
// the revocation happens before the network service has processed the grant.
// Regression test for https://crbug.com/513512147.
IN_PROC_BROWSER_TEST_F(ExtensionActiveTabWithSlowNetworkServiceTest,
                       RevokeBeforeNetworkServiceUpdate) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // The renderer decides on its own (based on its copy of the extension
  // permissions) whether to run content scripts from the manifest.  This lets
  // the test check renderer-side permissions.  The content script only
  // matches `title2.html` (used for a subframe below) so that it isn't
  // blocked in the main frame (`title1.html`).
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"(
      {
        "name": "RevokeBeforeNetworkServiceUpdate",
        "version": "1.0",
        "manifest_version": 3,
        "host_permissions": ["*://a.com/*"],
        "content_scripts": [{
          "matches": ["*://a.com/title2.html"],
          "all_frames": true,
          "run_at": "document_end",
          "js": ["content_script.js"]
        }]
      } )");
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"),
                     "document.documentElement.dataset.injected = 'yes';");
  scoped_refptr<const Extension> extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Grant and then revoke activeTab permission, while the network service
  // update of the grant is held.
  HoldNextNetworkServiceUpdate();
  auto* granter = ActiveTabPermissionGranter::FromWebContents(web_contents);
  granter->GrantIfRequested(extension.get(), base::DoNothing());
  ASSERT_TRUE(HasHeldNetworkServiceUpdate());
  granter->ClearActiveExtensionAndNotify(extension->id());

  // Release the held update, and wait until the network service has processed
  // it (network service updates are processed in FIFO order).
  ReleaseHeldNetworkServiceUpdate();
  base::test::TestFuture<void> update_done;
  NetworkPermissionsUpdater::UpdateExtension(
      *profile(), *extension,
      NetworkPermissionsUpdater::ContextSet::kAllRelatedContexts,
      update_done.GetCallback());
  ASSERT_TRUE(update_done.Wait());

  // Add a same-origin subframe (hosted in the same renderer process), and
  // verify that the content script didn't run there.  (The script would run
  // if the renderer still had the revoked activeTab grant.)
  ASSERT_TRUE(content::ExecJs(web_contents, R"(
      new Promise(resolve => {
        const iframe = document.createElement('iframe');
        iframe.src = '/title2.html';
        iframe.onload = () => resolve();
        document.body.appendChild(iframe);
      }) )"));
  content::RenderFrameHost* subframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe);
  EXPECT_EQ(false, content::EvalJs(
                       subframe,
                       "document.documentElement.dataset.injected === 'yes'"));
}

// Tests the behavior of activeTab and its relation to an extension's ability to
// xhr file urls and inject scripts in file frames.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FileURLs) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener background_page_ready("ready");
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("active_tab_file_urls"),
                    {.allow_file_access = true});
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  // Ensure the extension's background page is ready.
  EXPECT_TRUE(background_page_ready.WaitUntilSatisfied());

  auto can_xhr_file_urls = [this, &extension_id]() {
    static constexpr char script[] = R"(
      var req = new XMLHttpRequest();
      var url = '%s';
      req.open('GET', url, true);
      req.onload = function() {
        if (req.responseText === 'Hello!')
          chrome.test.sendScriptResult('true');

        // Even for a successful request, the status code might be 0. Ensure
        // that onloadend is not subsequently called if the request is
        // successful.
        req.onloadend = null;
      };

      // We track 'onloadend' to detect failures instead of 'onerror', since for
      // access check violations 'abort' event may be raised (instead of the
      // 'error' event).
      req.onloadend = function() {
        if (req.status === 0)
          chrome.test.sendScriptResult('false');
      };
      req.send();
    )";

    base::FilePath test_file =
        test_data_dir_.DirName().AppendASCII("test_file.txt");
    base::Value result = ExecuteScriptInBackgroundPage(
        extension_id,
        base::StringPrintf(script,
                           net::FilePathToFileURL(test_file).spec().c_str()));

    EXPECT_TRUE(result.is_string() && (result == "true" || result == "false"));
    return result.is_string() && result == "true";
  };

  auto can_load_file_iframe = [this, &extension_id]() {
    const Extension* extension =
        extension_registry()->enabled_extensions().GetByID(extension_id);

    // Load an extension page with a file iframe.
    GURL page = extension->GetResourceURL("file_iframe.html");
    ExtensionTestMessageListener listener;
    NavigateToURLInNewTab(page);
    EXPECT_TRUE(listener.WaitUntilSatisfied());

    EXPECT_TRUE(listener.message() == "allowed" ||
                listener.message() == "denied")
        << "Unexpected message " << listener.message();
    bool allowed = listener.message() == "allowed";

    // Sanity check the last committed url on the |file_iframe|.
    content::RenderFrameHost* file_iframe = content::FrameMatchingPredicate(
        GetActiveWebContents()->GetPrimaryPage(),
        base::BindRepeating(&content::FrameMatchesName, "file_iframe"));
    bool is_file_url = file_iframe->GetLastCommittedURL() == GURL("file:///");
    EXPECT_EQ(allowed, is_file_url)
        << "Unexpected committed url: "
        << file_iframe->GetLastCommittedURL().spec();

    browser()->GetTabStripModel()->CloseSelectedTabs();
    return allowed;
  };

  auto can_script_tab = [this, &extension_id](int tab_id) {
    static constexpr char script[] = R"(
      var tabID = %d;
      chrome.tabs.executeScript(
          tabID, {code: 'console.log("injected");'}, function() {
            const expectedError = 'Cannot access contents of the page. ' +
                'Extension manifest must request permission to access the ' +
                'respective host.';

            if (chrome.runtime.lastError &&
                expectedError != chrome.runtime.lastError.message) {
              chrome.test.sendScriptResult(
                  'unexpected error: ' + chrome.runtime.lastError.message);
            } else {
              chrome.test.sendScriptResult(
                  chrome.runtime.lastError ? 'false' : 'true');
            }
          });
    )";

    base::Value result = ExecuteScriptInBackgroundPage(
        extension_id, base::StringPrintf(script, tab_id));
    EXPECT_TRUE(result.is_string());
    EXPECT_TRUE(result == "true" || result == "false");
    return result.is_string() && result == "true";
  };

  auto get_active_tab_id = [this]() {
    sessions::SessionTabHelper* session_tab_helper =
        sessions::SessionTabHelper::FromWebContents(GetActiveWebContents());
    if (!session_tab_helper) {
      ADD_FAILURE();
      return extension_misc::kUnknownTabId;
    }
    return session_tab_helper->session_id().id();
  };

  // Navigate to two file urls (the extension's manifest.json and background.js
  // in this case).
  GURL file_url_1 =
      net::FilePathToFileURL(extension->path().AppendASCII("manifest.json"));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), file_url_1));

  // Assigned to |inactive_tab_id| since we open another foreground tab
  // subsequently.
  int inactive_tab_id = get_active_tab_id();
  EXPECT_NE(extension_misc::kUnknownTabId, inactive_tab_id);

  GURL file_url_2 =
      net::FilePathToFileURL(extension->path().AppendASCII("background.js"));
  NavigateToURLInNewTab(file_url_2);
  int active_tab_id = get_active_tab_id();
  EXPECT_NE(extension_misc::kUnknownTabId, active_tab_id);

  EXPECT_NE(inactive_tab_id, active_tab_id);

  // By default the extension should have file access enabled. However, since it
  // does not have host permissions to the localhost on the file scheme, it
  // should not be able to xhr file urls. For the same reason, it should not be
  // able to execute script in the two tabs or embed file iframes.
  EXPECT_TRUE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(can_xhr_file_urls());
  EXPECT_FALSE(can_script_tab(active_tab_id));
  EXPECT_FALSE(can_script_tab(inactive_tab_id));
  EXPECT_FALSE(can_load_file_iframe());

  // First don't grant the tab permission. Verify that the extension can't xhr
  // file urls, can't script the two tabs and can't embed file iframes.
  content::WebContents* web_contents = GetActiveWebContents();
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension.get(), false /*grant_tab_permissions*/);
  EXPECT_FALSE(can_xhr_file_urls());
  EXPECT_FALSE(can_script_tab(active_tab_id));
  EXPECT_FALSE(can_script_tab(inactive_tab_id));
  EXPECT_FALSE(can_load_file_iframe());

  // Now grant the tab permission. Ensure the extension can now xhr file urls ,
  // script the active tab and embed file iframes. It should still not be able
  // to script the background tab.
  {
    PermissionsManagerWaiter waiter(PermissionsManager::Get(profile()));
    ExtensionActionRunner::GetForWebContents(web_contents)
        ->RunAction(extension.get(), true /*grant_tab_permissions*/);
    waiter.WaitForActiveTabPermissionGranted(extension->id());
  }
  EXPECT_TRUE(can_xhr_file_urls());
  EXPECT_TRUE(can_script_tab(active_tab_id));
  EXPECT_TRUE(can_load_file_iframe());
  EXPECT_FALSE(can_script_tab(inactive_tab_id));

  // Revoke extension's access to file urls. This will cause the extension to
  // reload, invalidating the |extension| pointer. Re-initialize the |extension|
  // pointer.
  background_page_ready.Reset();
  util::SetAllowFileAccess(extension_id, profile(), false /*allow*/);
  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  extension = TestExtensionRegistryObserver(ExtensionRegistry::Get(profile()))
                  .WaitForExtensionLoaded();
  EXPECT_TRUE(extension);

  // Ensure the extension's background page is ready.
  EXPECT_TRUE(background_page_ready.WaitUntilSatisfied());

  // Grant the tab permission for the active url to the extension. Ensure it
  // still can't xhr file urls, script the active tab or embed file iframes
  // (since it does not have file access).
  {
    PermissionsManagerWaiter waiter(PermissionsManager::Get(profile()));
    ExtensionActionRunner::GetForWebContents(web_contents)
        ->RunAction(extension.get(), true /*grant_tab_permissions*/);
    waiter.WaitForActiveTabPermissionGranted(extension->id());
  }
  EXPECT_FALSE(can_xhr_file_urls());
  EXPECT_FALSE(can_script_tab(active_tab_id));
  EXPECT_FALSE(can_script_tab(inactive_tab_id));
  EXPECT_FALSE(can_load_file_iframe());
}

}  // namespace
}  // namespace extensions
