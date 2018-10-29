// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/extension_action/test_extension_action_api_observer.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/state_store.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/window_open_disposition.h"

namespace extensions {
namespace {

// A helper class to track StateStore changes.
class TestStateStoreObserver : public StateStore::TestObserver {
 public:
  TestStateStoreObserver(content::BrowserContext* context,
                         const std::string& extension_id)
      : extension_id_(extension_id), scoped_observer_(this) {
    scoped_observer_.Add(ExtensionSystem::Get(context)->state_store());
  }
  ~TestStateStoreObserver() override {}

  void WillSetExtensionValue(const std::string& extension_id,
                             const std::string& key) override {
    if (extension_id == extension_id_)
      ++updated_values_[key];
  }

  int CountForKey(const std::string& key) const {
    auto iter = updated_values_.find(key);
    return iter == updated_values_.end() ? 0 : iter->second;
  }

 private:
  std::string extension_id_;
  std::map<std::string, int> updated_values_;

  ScopedObserver<StateStore, StateStore::TestObserver> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(TestStateStoreObserver);
};

}  // namespace

using ExtensionActionAPITest = ExtensionApiTest;

// Check that updating the browser action badge for a specific tab id does not
// cause a disk write (since we only persist the defaults).
IN_PROC_BROWSER_TEST_F(ExtensionActionAPITest, TestNoUnnecessaryIO) {
  ExtensionTestMessageListener ready_listener("ready", false);

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Extension",
           "description": "An extension",
           "manifest_version": 2,
           "version": "0.1",
           "browser_action": {},
           "background": { "scripts": ["background.js"] }
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     "chrome.test.sendMessage('ready');");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // The script template to update the browser action.
  constexpr char kUpdate[] =
      R"(chrome.browserAction.setBadgeText(%s);
         domAutomationController.send('pass');)";
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SessionID tab_id = SessionTabHelper::IdForTab(web_contents);
  constexpr char kBrowserActionKey[] = "browser_action";
  TestStateStoreObserver test_state_store_observer(profile(), extension->id());

  {
    TestExtensionActionAPIObserver test_api_observer(profile(),
                                                     extension->id());
    // First, update a specific tab.
    std::string update_options =
        base::StringPrintf("{text: 'New Text', tabId: %d}", tab_id.id());
    EXPECT_EQ("pass", browsertest_util::ExecuteScriptInBackgroundPage(
                          profile(), extension->id(),
                          base::StringPrintf(kUpdate, update_options.c_str())));
    test_api_observer.Wait();

    // The action update should be associated with the specific tab.
    EXPECT_EQ(web_contents, test_api_observer.last_web_contents());
    // Since this was only updating a specific tab, this should *not* result in
    // a StateStore write. We should only write to the StateStore with new
    // default values.
    EXPECT_EQ(0, test_state_store_observer.CountForKey(kBrowserActionKey));
  }

  {
    TestExtensionActionAPIObserver test_api_observer(profile(),
                                                     extension->id());
    // Next, update the default badge text.
    EXPECT_EQ("pass",
              browsertest_util::ExecuteScriptInBackgroundPage(
                  profile(), extension->id(),
                  base::StringPrintf(kUpdate, "{text: 'Default Text'}")));
    test_api_observer.Wait();
    // The action update should not be associated with a specific tab.
    EXPECT_EQ(nullptr, test_api_observer.last_web_contents());

    // This *should* result in a StateStore write, since we persist the default
    // state of the extension action.
    EXPECT_EQ(1, test_state_store_observer.CountForKey(kBrowserActionKey));
  }
}

// Verify that tab-specific values are cleared on navigation and on tab
// removal. Regression test for https://crbug.com/834033.
IN_PROC_BROWSER_TEST_F(ExtensionActionAPITest,
                       ValuesAreClearedOnNavigationAndTabRemoval) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Extension",
           "description": "An extension",
           "manifest_version": 2,
           "version": "0.1",
           "browser_action": {}
         })");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  auto* action_manager = ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), initial_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* web_contents = tab_strip_model->GetActiveWebContents();
  int tab_id = SessionTabHelper::IdForTab(web_contents).id();

  // There should be no explicit title to start, but should be one if we set
  // one.
  EXPECT_FALSE(action->HasTitle(tab_id));
  action->SetTitle(tab_id, "alpha");
  EXPECT_TRUE(action->HasTitle(tab_id));

  // Navigating should clear the title.
  GURL second_url = embedded_test_server()->GetURL("/title2.html");
  ui_test_utils::NavigateToURL(browser(), second_url);

  EXPECT_EQ(second_url, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(action->HasTitle(tab_id));

  action->SetTitle(tab_id, "alpha");
  {
    content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);
    tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                        TabStripModel::CLOSE_NONE);
    destroyed_watcher.Wait();
  }
  // The title should have been cleared on tab removal as well.
  EXPECT_FALSE(action->HasTitle(tab_id));
}

}  // namespace extensions
