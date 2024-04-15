// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {
namespace {

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

  // Shouldn't be initially granted based on activeTab.
  {
    ExtensionTestMessageListener navigation_count_listener("1");
    ResultCatcher catcher;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "google.com", "/extensions/api_test/active_tab/page.html")));
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
    EXPECT_TRUE(navigation_count_listener.WaitUntilSatisfied());
  }

  // Do one pass of BrowserAction without granting activeTab permission,
  // extension shouldn't have access to tab.url.
  {
    ResultCatcher catcher;
    ExtensionActionRunner::GetForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents())
        ->RunAction(extension, false);
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }

  // Granting to the extension should give it access to page.html.
  {
    ResultCatcher catcher;
    ExtensionActionRunner::GetForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents())
        ->RunAction(extension, true);
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }

  // Navigating to a different page on the same origin should revoke extension's
  // access to the tab, unless the runtime host permissions feature is enabled.
  {
    ExtensionTestMessageListener navigation_count_listener("2");
    ResultCatcher catcher;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
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

  {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "google.com", "/extensions/api_test/active_tab_cors/page.html")));
    std::u16string title = u"page";
    content::TitleWatcher watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), title);
    ASSERT_EQ(title, watcher.WaitAndGetTitle());
  }

  {
    // The injected content script has an access to page's origin without
    // explicit permissions other than "activeTab".
    ResultCatcher catcher;
    ExtensionActionRunner::GetForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents())
        ->RunAction(extension, true);
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }
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
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), page, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    EXPECT_TRUE(listener.WaitUntilSatisfied());

    EXPECT_TRUE(listener.message() == "allowed" ||
                listener.message() == "denied")
        << "Unexpected message " << listener.message();
    bool allowed = listener.message() == "allowed";

    // Sanity check the last committed url on the |file_iframe|.
    content::RenderFrameHost* file_iframe = content::FrameMatchingPredicate(
        browser()->tab_strip_model()->GetActiveWebContents()->GetPrimaryPage(),
        base::BindRepeating(&content::FrameMatchesName, "file_iframe"));
    bool is_file_url = file_iframe->GetLastCommittedURL() == GURL("file:///");
    EXPECT_EQ(allowed, is_file_url)
        << "Unexpected committed url: "
        << file_iframe->GetLastCommittedURL().spec();

    browser()->tab_strip_model()->CloseSelectedTabs();
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
        sessions::SessionTabHelper::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), file_url_1));

  // Assigned to |inactive_tab_id| since we open another foreground tab
  // subsequently.
  int inactive_tab_id = get_active_tab_id();
  EXPECT_NE(extension_misc::kUnknownTabId, inactive_tab_id);

  GURL file_url_2 =
      net::FilePathToFileURL(extension->path().AppendASCII("background.js"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), file_url_2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
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
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension.get(), false /*grant_tab_permissions*/);
  EXPECT_FALSE(can_xhr_file_urls());
  EXPECT_FALSE(can_script_tab(active_tab_id));
  EXPECT_FALSE(can_script_tab(inactive_tab_id));
  EXPECT_FALSE(can_load_file_iframe());

  // Now grant the tab permission. Ensure the extension can now xhr file urls ,
  // script the active tab and embed file iframes. It should still not be able
  // to script the background tab.
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension.get(), true /*grant_tab_permissions*/);
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
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension.get(), true /*grant_tab_permissions*/);
  EXPECT_FALSE(can_xhr_file_urls());
  EXPECT_FALSE(can_script_tab(active_tab_id));
  EXPECT_FALSE(can_script_tab(inactive_tab_id));
  EXPECT_FALSE(can_load_file_iframe());
}

}  // namespace
}  // namespace extensions
