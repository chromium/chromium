// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

using ContextType = ExtensionBrowserTest::ContextType;

class HistoryApiTest : public ExtensionApiTest,
                       public testing::WithParamInterface<ContextType> {
 public:
  HistoryApiTest() : ExtensionApiTest(GetParam()) {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    host_resolver()->AddRule("www.a.com", "127.0.0.1");
    host_resolver()->AddRule("www.b.com", "127.0.0.1");
  }

  static std::string ExecuteScript(const ExtensionId& extension_id,
                                   content::BrowserContext* context,
                                   const std::string& script) {
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        context, extension_id, script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    EXPECT_TRUE(result.is_string());
    return result.is_string() ? result.GetString() : std::string();
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         HistoryApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         HistoryApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(HistoryApiTest, MiscSearch) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/misc_search")) << message_;
}

IN_PROC_BROWSER_TEST_P(HistoryApiTest, TimedSearch) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/timed_search")) << message_;
}

// TODO(crbug.com/1423419): This tests fails when the extension uses a
// service worker. Only run the legacy background page version for now.
using HistoryApiBackgroundPageTest = HistoryApiTest;

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         HistoryApiBackgroundPageTest,
                         ::testing::Values(ContextType::kPersistentBackground));

IN_PROC_BROWSER_TEST_P(HistoryApiBackgroundPageTest, Delete) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/delete")) << message_;
}

IN_PROC_BROWSER_TEST_P(HistoryApiTest, DeleteProhibited) {
  browser()->profile()->GetPrefs()->
      SetBoolean(prefs::kAllowDeletingBrowserHistory, false);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/delete_prohibited"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(HistoryApiTest, GetVisits) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/get_visits")) << message_;
}

IN_PROC_BROWSER_TEST_P(HistoryApiTest, SearchAfterAdd) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/search_after_add")) << message_;
}

// Test when History API is used from incognito mode, it has access to the
// regular mode history and actual incognito navigation has no effect on it.
IN_PROC_BROWSER_TEST_P(HistoryApiTest, Incognito) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Setup.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  Profile* incognito_profile = incognito_browser->profile();
  ExtensionTestMessageListener regular_listener("regular ready");
  ExtensionTestMessageListener incognito_listener("incognito ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("history/incognito"),
                    {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  ASSERT_TRUE(regular_listener.WaitUntilSatisfied());
  ASSERT_TRUE(incognito_listener.WaitUntilSatisfied());

  const ExtensionId& extension_id = extension->id();

  // Check if history is empty in regular mode.
  EXPECT_EQ("0",
            ExecuteScript(extension_id, profile(), "countItemsInHistory()"));

  // Insert an item in incognito mode.
  EXPECT_EQ("success",
            ExecuteScript(extension_id, incognito_profile, "addItem()"));

  // Check history in incognito mode.
  EXPECT_EQ("1", ExecuteScript(extension_id, incognito_profile,
                               "countItemsInHistory()"));

  // Check history in regular mode.
  EXPECT_EQ("1",
            ExecuteScript(extension_id, profile(), "countItemsInHistory()"));

  // Perform navigation in incognito mode.
  const GURL b_com =
      embedded_test_server()->GetURL("www.b.com", "/simple.html");
  content::TestNavigationObserver incognito_observer(
      incognito_browser->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, b_com));
  EXPECT_TRUE(incognito_observer.last_navigation_succeeded());

  // Check history in regular mode is not modified by incognito navigation.
  EXPECT_EQ("1",
            ExecuteScript(extension_id, profile(), "countItemsInHistory()"));

  // Check that history in incognito mode is not modified by navigation as
  // incognito navigations are not recorded in history.
  EXPECT_EQ("1", ExecuteScript(extension_id, incognito_profile,
                               "countItemsInHistory()"));

  // Perform navigation in regular mode.
  content::TestNavigationObserver regular_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), b_com));
  EXPECT_TRUE(regular_observer.last_navigation_succeeded());

  // Check history in regular mode is modified by navigation.
  EXPECT_EQ("2",
            ExecuteScript(extension_id, profile(), "countItemsInHistory()"));

  // Check history in incognito mode is modified by navigation.
  EXPECT_EQ("2", ExecuteScript(extension_id, incognito_profile,
                               "countItemsInHistory()"));
}

}  // namespace extensions
