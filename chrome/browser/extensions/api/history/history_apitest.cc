// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"

namespace {

std::string RunScriptAndReturnResult(const extensions::ExtensionHost* host,
                                     const std::string& script) {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(host->host_contents(),
                                                     script, &result))
      << script;
  return result;
}

}  // namespace

namespace extensions {

class HistoryApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    host_resolver()->AddRule("www.a.com", "127.0.0.1");
    host_resolver()->AddRule("www.b.com", "127.0.0.1");
  }
};

// Full text search indexing sometimes exceeds a timeout. (http://crbug/119505)
IN_PROC_BROWSER_TEST_F(HistoryApiTest, DISABLED_MiscSearch) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("history/regular", "misc_search.html"))
      << message_;
}

// Same could happen here without the FTS (http://crbug/119505)
IN_PROC_BROWSER_TEST_F(HistoryApiTest, DISABLED_TimedSearch) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("history/regular", "timed_search.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(HistoryApiTest, Delete) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("history/regular", "delete.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(HistoryApiTest, DeleteProhibited) {
  browser()->profile()->GetPrefs()->
      SetBoolean(prefs::kAllowDeletingBrowserHistory, false);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("history/regular", "delete_prohibited.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(HistoryApiTest, GetVisits) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("history/regular", "get_visits.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(HistoryApiTest, SearchAfterAdd) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("history/regular", "search_after_add.html"))
      << message_;
}

// Test when History API is used from incognito mode, it has access to the
// regular mode history and actual incognito navigation has no effect on it.
IN_PROC_BROWSER_TEST_F(HistoryApiTest, Incognito) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Setup.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  ExtensionTestMessageListener regular_listener("regular ready", false);
  ExtensionTestMessageListener incognito_listener("incognito ready", false);
  const Extension* extension = LoadExtensionWithFlags(
      test_data_dir_.AppendASCII("history/incognito"), kFlagEnableIncognito);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(regular_listener.WaitUntilSatisfied());
  ASSERT_TRUE(incognito_listener.WaitUntilSatisfied());

  ExtensionHost* on_the_record_background_page =
      ProcessManager::Get(browser()->profile())
          ->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(on_the_record_background_page);

  ExtensionHost* incognito_background_page =
      ProcessManager::Get(incognito_browser->profile())
          ->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(incognito_background_page);
  EXPECT_NE(incognito_background_page, on_the_record_background_page);

  // Check if history is empty in regular mode.
  EXPECT_EQ("0", RunScriptAndReturnResult(on_the_record_background_page,
                                          "countItemsInHistory()"));

  // Insert an item in incognito mode.
  EXPECT_EQ(std::string("success"),
            RunScriptAndReturnResult(incognito_background_page, "addItem()"));

  // Check history in incognito mode.
  EXPECT_EQ("1", RunScriptAndReturnResult(on_the_record_background_page,
                                          "countItemsInHistory()"));

  // Check history in regular mode.
  EXPECT_EQ("1", RunScriptAndReturnResult(on_the_record_background_page,
                                          "countItemsInHistory()"));

  // Perform navigation in incognito mode.
  const GURL b_com =
      embedded_test_server()->GetURL("www.b.com", "/simple.html");
  content::TestNavigationObserver incognito_observer(
      incognito_browser->tab_strip_model()->GetActiveWebContents());
  ui_test_utils::NavigateToURL(incognito_browser, b_com);
  EXPECT_TRUE(incognito_observer.last_navigation_succeeded());

  // Check history in regular mode is not modified by incognito navigation.
  EXPECT_EQ("1", RunScriptAndReturnResult(on_the_record_background_page,
                                          "countItemsInHistory()"));

  // Check that history in incognito mode is not modified by navigation as
  // incognito navigations are not recorded in history.
  EXPECT_EQ("1", RunScriptAndReturnResult(incognito_background_page,
                                          "countItemsInHistory()"));

  // Perform navigation in regular mode.
  content::TestNavigationObserver regular_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ui_test_utils::NavigateToURL(browser(), b_com);
  EXPECT_TRUE(regular_observer.last_navigation_succeeded());

  // Check history in regular mode is modified by navigation.
  EXPECT_EQ("2", RunScriptAndReturnResult(on_the_record_background_page,
                                          "countItemsInHistory()"));

  // Check history in incognito mode is modified by navigation.
  EXPECT_EQ("2", RunScriptAndReturnResult(incognito_background_page,
                                          "countItemsInHistory()"));
}

}  // namespace extensions
