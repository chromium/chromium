// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/extension_action/test_extension_action_api_observer.h"
#include "chrome/browser/extensions/api/extension_action/test_icon_image_observer.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/script_executor.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/color_utils.h"

namespace extensions {
namespace {

// A background script that allows for setting the icon dynamically.
constexpr char kSetIconBackgroundJsTemplate[] =
    R"(function setIcon(details) {
         chrome.%s.setIcon(details, () => {
           chrome.test.assertNoLastError();
           chrome.test.notifyPass();
         });
       }
       function setIconPromise(details) {
         chrome.%s.setIcon(details)
           .then(chrome.test.notifyPass)
           .catch(chrome.test.notifyFail);
       })";

constexpr char kPageHtmlTemplate[] =
    R"(<html><script src="page.js"></script></html>)";

// Runs |script| in the given |web_contents| and waits for it to send a
// test-passed result. This will fail if the test in |script| fails. Note:
// |web_contents| is expected to be an extension contents with access to
// extension APIs.
void RunTestAndWaitForSuccess(content::WebContents* web_contents,
                              const std::string& script) {
  SCOPED_TRACE(script);
  ResultCatcher result_catcher;
  content::ExecuteScriptAsync(web_contents, script);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// A helper class to track StateStore changes.
class TestStateStoreObserver : public StateStore::TestObserver {
 public:
  TestStateStoreObserver(content::BrowserContext* context,
                         const ExtensionId& extension_id)
      : extension_id_(extension_id) {
    scoped_observation_.Observe(ExtensionSystem::Get(context)->state_store());
  }

  TestStateStoreObserver(const TestStateStoreObserver&) = delete;
  TestStateStoreObserver& operator=(const TestStateStoreObserver&) = delete;

  ~TestStateStoreObserver() override {}

  void WillSetExtensionValue(const ExtensionId& extension_id,
                             const std::string& key) override {
    if (extension_id == extension_id_)
      ++updated_values_[key];
  }

  int CountForKey(const std::string& key) const {
    auto iter = updated_values_.find(key);
    return iter == updated_values_.end() ? 0 : iter->second;
  }

 private:
  ExtensionId extension_id_;
  std::map<std::string, int> updated_values_;

  base::ScopedObservation<StateStore, StateStore::TestObserver>
      scoped_observation_{this};
};

// A helper class to handle setting or getting the values for an action from JS.
class ActionTestHelper {
 public:
  ActionTestHelper(const char* api_name,
                   const char* set_method_name,
                   const char* get_method_name,
                   const char* js_property_key,
                   content::WebContents* web_contents)
      : api_name_(api_name),
        set_method_name_(set_method_name),
        get_method_name_(get_method_name),
        js_property_key_(js_property_key),
        web_contents_(web_contents) {}

  ActionTestHelper(const ActionTestHelper&) = delete;
  ActionTestHelper& operator=(const ActionTestHelper&) = delete;

  ~ActionTestHelper() = default;

  // Checks the value for the given |tab_id|.
  void CheckValueForTab(const char* expected_js_value, int tab_id) const {
    constexpr char kScriptTemplate[] =
        R"(chrome.%s.%s({tabId: %d}, (res) => {
             chrome.test.assertNoLastError();
             chrome.test.assertEq(%s, res);
             chrome.test.notifyPass();
           });)";
    RunTestAndWaitForSuccess(
        web_contents_,
        base::StringPrintf(kScriptTemplate, api_name_, get_method_name_, tab_id,
                           expected_js_value));
  }

  // Checks the default value.
  void CheckDefaultValue(const char* expected_js_value) const {
    constexpr char kScriptTemplate[] =
        R"(chrome.%s.%s({}, (res) => {
             chrome.test.assertNoLastError();
             chrome.test.assertEq(%s, res);
             chrome.test.notifyPass();
           });)";
    RunTestAndWaitForSuccess(
        web_contents_, base::StringPrintf(kScriptTemplate, api_name_,
                                          get_method_name_, expected_js_value));
  }

  // Sets the value for a given |tab_id|.
  void SetValueForTab(const char* new_js_value, int tab_id) const {
    constexpr char kScriptTemplate[] =
        R"(chrome.%s.%s({tabId: %d, %s: %s}, () => {
             chrome.test.assertNoLastError();
             chrome.test.notifyPass();
           });)";
    RunTestAndWaitForSuccess(
        web_contents_,
        base::StringPrintf(kScriptTemplate, api_name_, set_method_name_, tab_id,
                           js_property_key_, new_js_value));
  }

  // Sets the default value.
  void SetDefaultValue(const char* new_js_value) const {
    constexpr char kScriptTemplate[] =
        R"(chrome.%s.%s({%s: %s}, () => {
             chrome.test.assertNoLastError();
             chrome.test.notifyPass();
           });)";
    RunTestAndWaitForSuccess(
        web_contents_,
        base::StringPrintf(kScriptTemplate, api_name_, set_method_name_,
                           js_property_key_, new_js_value));
  }

 private:
  // The name of the api (e.g., "action").
  const char* const api_name_;
  // The name of the method to call to set the value (e.g., "setPopup").
  const char* const set_method_name_;
  // The name of the method to call to get the value (e.g., "getPopup").
  const char* const get_method_name_;
  // The name of the property in the set method details (e.g., "popup").
  const char* const js_property_key_;
  // The WebContents to use to execute API calls.
  const raw_ptr<content::WebContents> web_contents_;
};

// Forces a flush of the StateStore, where action state is persisted.
void FlushStateStore(Profile* profile) {
  base::RunLoop run_loop;
  ExtensionSystem::Get(profile)->state_store()->FlushForTesting(
      run_loop.QuitWhenIdleClosure());
  run_loop.Run();
}

}  // namespace

// A class that allows for cross-origin navigations with embedded test server.
class ExtensionActionAPITest : public ExtensionApiTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

// Alias these for readability, when a test only exercises one type of action.
using BrowserActionAPITest = ExtensionActionAPITest;
using PageActionAPITest = ExtensionActionAPITest;

// A class that runs tests exercising each type of possible toolbar action.
class MultiActionAPITest
    : public ExtensionActionAPITest,
      public testing::WithParamInterface<ActionInfo::Type> {
 public:
  MultiActionAPITest() = default;

  // Returns true if the |action| has whatever state its default is on the
  // tab with the given |tab_id|.
  bool ActionHasDefaultState(const ExtensionAction& action, int tab_id) const {
    bool is_visible = action.GetIsVisible(tab_id);
    bool default_is_visible =
        action.default_state() == ActionInfo::DefaultState::kEnabled;
    return is_visible == default_is_visible;
  }

  // Ensures the |action| is enabled on the tab with the given |tab_id|.
  void EnsureActionIsEnabledOnTab(ExtensionAction* action, int tab_id) {
    if (action->GetIsVisible(tab_id))
      return;
    action->SetIsVisible(tab_id, true);
    // Just setting the state on the action doesn't update the UI. Ensure
    // observers are notified.
    extensions::ExtensionActionAPI* extension_action_api =
        extensions::ExtensionActionAPI::Get(profile());
    extension_action_api->NotifyChange(action, GetActiveTab(), profile());
  }

  // Ensures the |action| is enabled on the currently-active tab.
  void EnsureActionIsEnabledOnActiveTab(ExtensionAction* action) {
    EnsureActionIsEnabledOnTab(action, GetActiveTabId());
  }

  // Returns the id of the currently-active tab.
  int GetActiveTabId() const {
    content::WebContents* web_contents = GetActiveTab();
    return sessions::SessionTabHelper::IdForTab(web_contents).id();
  }

  content::WebContents* GetActiveTab() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Returns the action associated with |extension|.
  ExtensionAction* GetExtensionAction(const Extension& extension) {
    auto* action_manager = ExtensionActionManager::Get(profile());
    return action_manager->GetExtensionAction(extension);
  }
};

// Canvas tests rely on the harness producing pixel output in order to read back
// pixels from a canvas element. So we have to override the setup function.
class MultiActionAPICanvasTest : public MultiActionAPITest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    MultiActionAPITest::SetUp();
  }
};

// Check that updating the browser action badge for a specific tab id does not
// cause a disk write (since we only persist the defaults).
// Only browser actions persist settings.
IN_PROC_BROWSER_TEST_F(BrowserActionAPITest, TestNoUnnecessaryIO) {
  ExtensionTestMessageListener ready_listener("ready");

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
  static constexpr char kUpdate[] =
      R"(chrome.browserAction.setBadgeText(%s);
         chrome.test.sendScriptResult('pass');)";
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  static constexpr char kBrowserActionKey[] = "browser_action";
  TestStateStoreObserver test_state_store_observer(profile(), extension->id());

  {
    TestExtensionActionAPIObserver test_api_observer(profile(),
                                                     extension->id());
    // First, update a specific tab.
    std::string update_options =
        base::StringPrintf("{text: 'New Text', tabId: %d}", tab_id.id());
    EXPECT_EQ("pass", ExecuteScriptInBackgroundPage(
                          extension->id(),
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
              ExecuteScriptInBackgroundPage(
                  extension->id(),
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
IN_PROC_BROWSER_TEST_P(MultiActionAPITest,
                       ValuesAreClearedOnNavigationAndTabRemoval) {
  TestExtensionDir test_dir;
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Extension",
           "description": "An extension",
           "manifest_version": %d,
           "version": "0.1",
           "%s": {}
         })";

  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  auto* action_manager = ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), initial_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* web_contents = tab_strip_model->GetActiveWebContents();
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  // There should be no explicit title to start, but should be one if we set
  // one.
  EXPECT_FALSE(action->HasTitle(tab_id));
  action->SetTitle(tab_id, "alpha");
  EXPECT_TRUE(action->HasTitle(tab_id));

  // Navigating should clear the title.
  GURL second_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), second_url));

  EXPECT_EQ(second_url, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(action->HasTitle(tab_id));

  action->SetTitle(tab_id, "alpha");
  {
    content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);
    tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                        TabCloseTypes::CLOSE_NONE);
    destroyed_watcher.Wait();
  }
  // The title should have been cleared on tab removal as well.
  EXPECT_FALSE(action->HasTitle(tab_id));
}

// Tests that tooltips of an extension action icon can be specified using UTF8.
// See http://crbug.com/25349.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, TitleLocalization) {
  TestExtensionDir test_dir;
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Hreggvi\u00F0ur is my name",
           "description": "Hreggvi\u00F0ur: l10n action",
           "manifest_version": %d,
           "version": "0.1",
           "%s": {
             "default_title": "Hreggvi\u00F0ur"
           }
         })";

  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  auto* action_manager = ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  EXPECT_EQ(base::WideToUTF8(L"Hreggvi\u00F0ur: l10n action"),
            extension->description());
  EXPECT_EQ(base::WideToUTF8(L"Hreggvi\u00F0ur is my name"), extension->name());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  EXPECT_EQ(base::WideToUTF8(L"Hreggvi\u00F0ur"), action->GetTitle(tab_id));
  EXPECT_EQ(base::WideToUTF8(L"Hreggvi\u00F0ur"),
            action->GetTitle(ExtensionAction::kDefaultTabId));
}

// Tests dispatching the onClicked event to listeners when the extension action
// in the toolbar is pressed.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, OnClickedDispatching) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Clicking",
           "manifest_version": %d,
           "version": "0.1",
           "%s": {},
           "background": { %s }
         })";
  constexpr char kBackgroundJsTemplate[] =
      R"(chrome.%s.onClicked.addListener((tab) => {
           // Check a few properties on the tabs object to make sure it's sane.
           chrome.test.assertTrue(!!tab);
           chrome.test.assertTrue(tab.id > 0);
           chrome.test.assertTrue(tab.index > -1);
           chrome.test.notifyPass();
         });)";

  const char* background_specification =
      GetParam() == ActionInfo::Type::kAction
          ? R"("service_worker": "background.js")"
          : R"("scripts": ["background.js"])";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam()),
      background_specification));
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(kBackgroundJsTemplate,
                                        GetAPINameForActionType(GetParam())));

  // Though this says "ExtensionActionTestHelper", it's actually used for all
  // toolbar actions.
  // TODO(devlin): Rename it to ToolbarActionTestUtil.
  std::unique_ptr<ExtensionActionTestHelper> toolbar_helper =
      ExtensionActionTestHelper::Create(browser());
  EXPECT_EQ(0, toolbar_helper->NumberOfBrowserActions());

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_EQ(1, toolbar_helper->NumberOfBrowserActions());
  EXPECT_TRUE(toolbar_helper->HasAction(extension->id()));

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  const int tab_id = GetActiveTabId();
  EXPECT_TRUE(ActionHasDefaultState(*action, tab_id));
  EnsureActionIsEnabledOnActiveTab(action);
  EXPECT_FALSE(action->HasPopup(tab_id));

  ResultCatcher result_catcher;
  toolbar_helper->Press(extension->id());
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Tests the creation of a popup when one is specified in the manifest.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, PopupCreation) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Clicking",
           "manifest_version": %d,
           "version": "0.1",
           "%s": {
             "default_popup": "popup.html"
           }
         })";

  constexpr char kPopupHtml[] =
      R"(<!doctype html>
         <html>
           <script src="popup.js"></script>
         </html>)";
  constexpr char kPopupJs[] =
      "window.onload = function() { chrome.test.notifyPass(); };";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), kPopupHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.js"), kPopupJs);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  std::unique_ptr<ExtensionActionTestHelper> toolbar_helper =
      ExtensionActionTestHelper::Create(browser());

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  const int tab_id = GetActiveTabId();
  EXPECT_TRUE(ActionHasDefaultState(*action, tab_id));
  EnsureActionIsEnabledOnActiveTab(action);
  EXPECT_TRUE(action->HasPopup(tab_id));

  ResultCatcher result_catcher;
  toolbar_helper->Press(extension->id());
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  ProcessManager* process_manager = ProcessManager::Get(profile());
  ProcessManager::FrameSet frames =
      process_manager->GetRenderFrameHostsForExtension(extension->id());
  ASSERT_EQ(1u, frames.size());
  content::RenderFrameHost* render_frame_host = *frames.begin();
  EXPECT_EQ(extension->GetResourceURL("popup.html"),
            render_frame_host->GetLastCommittedURL());

  content::WebContents* popup_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  ASSERT_TRUE(popup_contents);

  content::WebContentsDestroyedWatcher contents_destroyed(popup_contents);
  EXPECT_TRUE(content::ExecJs(popup_contents, "window.close()"));
  contents_destroyed.Wait();

  frames = process_manager->GetRenderFrameHostsForExtension(extension->id());
  EXPECT_EQ(0u, frames.size());
}

// Tests that sessionStorage does not persist between closing and opening of a
// popup.
// TODO(crbug.com/40795982): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_SessionStorageDoesNotPersistBetweenOpenings \
  DISABLED_SessionStorageDoesNotPersistBetweenOpenings
#else
#define MAYBE_SessionStorageDoesNotPersistBetweenOpenings \
  SessionStorageDoesNotPersistBetweenOpenings
#endif
IN_PROC_BROWSER_TEST_P(MultiActionAPITest,
                       MAYBE_SessionStorageDoesNotPersistBetweenOpenings) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test sessionStorage",
           "manifest_version": %d,
           "version": "0.1",
           "%s": {
             "default_popup": "popup.html"
           }
         })";

  constexpr char kPopupHtml[] =
      R"(<!doctype html>
         <html>
           <script src="popup.js"></script>
         </html>)";

  constexpr char kPopupJs[] =
      R"(window.onload = function() {
           if (!sessionStorage.foo) {
             sessionStorage.foo = 1;
           } else {
             sessionStorage.foo = parseInt(sessionStorage.foo) + 1;
           }
           chrome.test.notifyPass();};
        )";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), kPopupHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.js"), kPopupJs);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  std::unique_ptr<ExtensionActionTestHelper> toolbar_helper =
      ExtensionActionTestHelper::Create(browser());

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  int tab_id = GetActiveTabId();
  EnsureActionIsEnabledOnActiveTab(action);
  EXPECT_TRUE(action->HasPopup(tab_id));

  ResultCatcher result_catcher;
  toolbar_helper->Press(extension->id());
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  ProcessManager* process_manager = ProcessManager::Get(profile());
  ProcessManager::FrameSet frames =
      process_manager->GetRenderFrameHostsForExtension(extension->id());
  ASSERT_EQ(1u, frames.size());
  content::RenderFrameHost* render_frame_host = *frames.begin();

  content::WebContents* popup_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  ASSERT_TRUE(popup_contents);

  EXPECT_EQ("1", content::EvalJs(popup_contents, "sessionStorage.foo"));

  const std::string session_storage_id1 =
      popup_contents->GetController().GetDefaultSessionStorageNamespace()->id();

  // Close the popup.
  content::WebContentsDestroyedWatcher contents_destroyed(popup_contents);
  EXPECT_TRUE(content::ExecJs(popup_contents, "window.close()"));
  contents_destroyed.Wait();

  frames = process_manager->GetRenderFrameHostsForExtension(extension->id());
  EXPECT_EQ(0u, frames.size());

  // Open the popup again.
  toolbar_helper->Press(extension->id());
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  frames = process_manager->GetRenderFrameHostsForExtension(extension->id());
  ASSERT_EQ(1u, frames.size());
  render_frame_host = *frames.begin();

  popup_contents = content::WebContents::FromRenderFrameHost(render_frame_host);
  const std::string session_storage_id2 =
      popup_contents->GetController().GetDefaultSessionStorageNamespace()->id();

  // Verify that sessionStorage did not persist. The reason is that closing the
  // popup ends the session and clears objects in sessionStorage.
  EXPECT_NE(session_storage_id1, session_storage_id2);
  EXPECT_EQ("1", content::EvalJs(popup_contents, "sessionStorage.foo"));
}

using ActionAndBrowserActionAPITest = MultiActionAPITest;

// Tests whether action values persist across sessions.
// Note: Since pageActions are only applicable on a specific tab, this test
// doesn't apply to them.
IN_PROC_BROWSER_TEST_P(ActionAndBrowserActionAPITest, PRE_ValuesArePersisted) {
  const char* dir_name = nullptr;
  switch (GetParam()) {
    case ActionInfo::Type::kAction:
      dir_name = "extension_action/action_persistence";
      break;
    case ActionInfo::Type::kBrowser:
      dir_name = "extension_action/browser_action_persistence";
      break;
    case ActionInfo::Type::kPage:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  // Load up an extension, which then modifies the popup, title, and badge text
  // of the action. We need to use a "real" extension on disk here (rather than
  // a TestExtensionDir owned by the test fixture), because it needs to persist
  // to the next test.
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(dir_name));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Verify the values were modified.
  auto* action_manager = ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  EXPECT_EQ(extension->GetResourceURL("modified_popup.html"),
            action->GetPopupUrl(ExtensionAction::kDefaultTabId));
  EXPECT_EQ("modified title", action->GetTitle(ExtensionAction::kDefaultTabId));
  EXPECT_EQ("custom badge text",
            action->GetExplicitlySetBadgeText(ExtensionAction::kDefaultTabId));

  // We flush the state store to ensure the modified state is correctly stored
  // on-disk (which could otherwise be potentially racy).
  FlushStateStore(profile());
}

IN_PROC_BROWSER_TEST_P(ActionAndBrowserActionAPITest, ValuesArePersisted) {
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);
  EXPECT_EQ("Action persistence check", extension->name());

  // The previous action states are read from the state store on start-up.
  // Flushing it ensures that any pending tasks have run, and the action
  // should be up-to-date.
  FlushStateStore(profile());

  auto* action_manager = ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);

  // Only browser actions - not generic actions - persist values.
  bool expect_persisted_values = GetParam() == ActionInfo::Type::kBrowser;

  std::string expected_badge_text =
      expect_persisted_values ? "custom badge text" : "";

  EXPECT_EQ(expected_badge_text,
            action->GetExplicitlySetBadgeText(ExtensionAction::kDefaultTabId));

  // Due to https://crbug.com/1110156, action values with defaults specified in
  // the manifest - like popup and title - aren't persisted, even for browser
  // actions.
  EXPECT_EQ(extension->GetResourceURL("default_popup.html"),
            action->GetPopupUrl(ExtensionAction::kDefaultTabId));
  EXPECT_EQ("default title", action->GetTitle(ExtensionAction::kDefaultTabId));
}

// Tests setting the icon dynamically from the background page.
// TODO(crbug.com/40230315): flaky.
IN_PROC_BROWSER_TEST_P(MultiActionAPICanvasTest, DISABLED_DynamicSetIcon) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Clicking",
           "manifest_version": %d,
           "version": "0.1",
           "%s": {
             "default_icon": "red_icon.png"
           }
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtmlTemplate);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"),
                     base::StringPrintf(kSetIconBackgroundJsTemplate,
                                        GetAPINameForActionType(GetParam()),
                                        GetAPINameForActionType(GetParam())));
  test_dir.CopyFileTo(test_data_dir_.AppendASCII("icon_rgb_0_0_255.png"),
                      FILE_PATH_LITERAL("blue_icon.png"));
  test_dir.CopyFileTo(test_data_dir_.AppendASCII("icon_rgb_255_0_0.png"),
                      FILE_PATH_LITERAL("red_icon.png"));

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  ASSERT_TRUE(action->default_icon());
  // Wait for the default icon to finish loading; otherwise it may be empty
  // when we check it.
  TestIconImageObserver::WaitForIcon(action->default_icon_image());

  int tab_id = GetActiveTabId();
  EXPECT_TRUE(ActionHasDefaultState(*action, tab_id));
  EnsureActionIsEnabledOnActiveTab(action);

  std::unique_ptr<ExtensionActionTestHelper> toolbar_helper =
      ExtensionActionTestHelper::Create(browser());

  ASSERT_EQ(1, toolbar_helper->NumberOfBrowserActions());
  EXPECT_TRUE(toolbar_helper->HasAction(extension->id()));

  gfx::Image default_icon = toolbar_helper->GetIcon(extension->id());
  EXPECT_FALSE(default_icon.IsEmpty());

  // Check the midpoint. All these icons are solid, but the rendered icon
  // includes padding.
  const int mid_x = default_icon.Width() / 2;
  const int mid_y = default_icon.Height() / 2;
  // Note: We only validate the color here as a quick-and-easy way of validating
  // the icon is what we expect. Other tests do much more rigorous testing of
  // the icon's rendering.
  EXPECT_EQ(SK_ColorRED, default_icon.AsBitmap().getColor(mid_x, mid_y));

  // Open a tab to run the extension commands in.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension->GetResourceURL("page.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  const int new_tab_id = GetActiveTabId();
  EXPECT_NE(new_tab_id, tab_id);
  EXPECT_TRUE(ActionHasDefaultState(*action, new_tab_id));
  EnsureActionIsEnabledOnActiveTab(action);

  // The new tab should still have the same icon (the default).
  gfx::Image new_tab_icon = toolbar_helper->GetIcon(extension->id());
  EXPECT_FALSE(default_icon.IsEmpty());
  EXPECT_EQ(SK_ColorRED, default_icon.AsBitmap().getColor(mid_x, mid_y));

  // Set the icon for the new tab to a different icon in the extension package.
  RunTestAndWaitForSuccess(
      web_contents,
      base::StringPrintf("setIcon({tabId: %d, path: 'blue_icon.png'});",
                         new_tab_id));

  new_tab_icon = toolbar_helper->GetIcon(extension->id());
  EXPECT_FALSE(new_tab_icon.IsEmpty());
  EXPECT_EQ(SK_ColorBLUE, new_tab_icon.AsBitmap().getColor(mid_x, mid_y));

  // Next, set the icon to a dynamically-generated one (from canvas image data).
  constexpr char kSetIconFromImageData[] =
      R"({
           let canvas = document.createElement('canvas');
           canvas.width = 32;
           canvas.height = 32;
           let context = canvas.getContext('2d');
           context.clearRect(0, 0, 32, 32);
           context.fillStyle = '#00FF00';  // Green
           context.fillRect(0, 0, 32, 32);
           let imageData = context.getImageData(0, 0, 32, 32);
           setIcon({tabId: %d, imageData: imageData});
         })";
  RunTestAndWaitForSuccess(
      web_contents, base::StringPrintf(kSetIconFromImageData, new_tab_id));

  new_tab_icon = toolbar_helper->GetIcon(extension->id());
  EXPECT_FALSE(new_tab_icon.IsEmpty());
  EXPECT_EQ(SK_ColorGREEN, new_tab_icon.AsBitmap().getColor(mid_x, mid_y));

  // Manifest V3 extensions using the action API should also be able to use a
  // promise version of setIcon.
  if (GetManifestVersionForActionType(GetParam()) == 3) {
    constexpr char kSetIconPromiseScript[] =
        "setIconPromise({tabId: %d, path: 'blue_icon.png'});";
    RunTestAndWaitForSuccess(
        web_contents, base::StringPrintf(kSetIconPromiseScript, new_tab_id));

    new_tab_icon = toolbar_helper->GetIcon(extension->id());
    EXPECT_FALSE(new_tab_icon.IsEmpty());
    EXPECT_EQ(SK_ColorBLUE, new_tab_icon.AsBitmap().getColor(mid_x, mid_y));
  }

  // Switch back to the first tab. The icon should still be red, since the other
  // changes were for specific tabs.
  browser()->tab_strip_model()->ActivateTabAt(0);
  gfx::Image first_tab_icon = toolbar_helper->GetIcon(extension->id());
  EXPECT_FALSE(first_tab_icon.IsEmpty());
  EXPECT_EQ(SK_ColorRED, first_tab_icon.AsBitmap().getColor(mid_x, mid_y));

  // TODO(devlin): Add tests for setting icons as a dictionary of
  // { size -> image_data }.
}

// Tests calling setIcon() from JS with hooks that might cause issues with our
// custom bindings.
// Regression test for https://crbug.com/1087948.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, SetIconWithJavascriptHooks) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "JS Fun",
           "manifest_version": %d,
           "version": "0.1",
           "%s": {}
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtmlTemplate);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"),
                     base::StringPrintf(kSetIconBackgroundJsTemplate,
                                        GetAPINameForActionType(GetParam()),
                                        GetAPINameForActionType(GetParam())));
  test_dir.CopyFileTo(test_data_dir_.AppendASCII("icon_rgb_0_0_255.png"),
                      FILE_PATH_LITERAL("blue_icon.png"));

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension->GetResourceURL("page.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  int tab_id = GetActiveTabId();
  EXPECT_TRUE(ActionHasDefaultState(*action, tab_id));
  EnsureActionIsEnabledOnActiveTab(action);

  // Define a setter for objects on the imageData key. This could previously
  // result in an invalid arguments object being sent to the browser.
  constexpr char kScript[] =
      R"(Object.defineProperty(
             Object.prototype, 'imageData',
             { set() { console.warn('intercepted set'); } });
         'done';)";
  ASSERT_EQ("done", content::EvalJs(web_contents, kScript));

  constexpr char kOnePathScript[] =
      "setIcon({tabId: %d, path: 'blue_icon.png'});";
  RunTestAndWaitForSuccess(web_contents,
                           base::StringPrintf(kOnePathScript, tab_id));
  constexpr char kMultiPathScript[] =
      R"(setIcon({tabId: %d,
                  path: {16: 'blue_icon.png', 24: 'blue_icon.png'}});)";
  RunTestAndWaitForSuccess(web_contents,
                           base::StringPrintf(kMultiPathScript, tab_id));
  constexpr char kRawImageDataScript[] =
      R"(setIcon({tabId: %d,
                  imageData: {width:4,height:4,data:'a'.repeat(64)}});)";
  RunTestAndWaitForSuccess(web_contents,
                           base::StringPrintf(kRawImageDataScript, tab_id));
}

// Tests calling setIcon() from JS with `self` defined at the top-level.
// Regression test for https://crbug.com/1087948.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, SetIconWithSelfDefined) {
  // TODO(devlin): Pull code to load an extension like this into a helper
  // function.
  constexpr char kManifestTemplate[] =
      R"({
           "name": "JS Fun",
           "manifest_version": %d,
           "version": "0.1",
           "%s": {}
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));

  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtmlTemplate);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"),
                     base::StringPrintf(kSetIconBackgroundJsTemplate,
                                        GetAPINameForActionType(GetParam()),
                                        GetAPINameForActionType(GetParam())));
  test_dir.CopyFileTo(test_data_dir_.AppendASCII("icon_rgb_0_0_255.png"),
                      FILE_PATH_LITERAL("blue_icon.png"));

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension->GetResourceURL("page.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  int tab_id = GetActiveTabId();
  EXPECT_TRUE(ActionHasDefaultState(*action, tab_id));
  EnsureActionIsEnabledOnActiveTab(action);

  // Override 'self' in a local variable.
  constexpr char kOverrideSelfScript[] = "var self = ''; 'done';";
  ASSERT_EQ("done", content::EvalJs(web_contents, kOverrideSelfScript));

  // Try setting the icon. This should succeed. Previously, the custom bindings
  // for the setIcon code looked at the 'self' variable, but this could be
  // overridden by the extension.
  // See also https://crbug.com/1087948.
  constexpr char kSetIconScript[] =
      "setIcon({tabId: %d, path: 'blue_icon.png'});";
  RunTestAndWaitForSuccess(web_contents,
                           base::StringPrintf(kSetIconScript, tab_id));
}

// Tests calling setIcon() for a tab with an invalid icon path specified.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, SetIconInTabWithInvalidPath) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Bad Icon Path",
           "manifest_version": %d,
           "version": "0.1",
           "%s": {}
         })";

  constexpr char kPageJsTemplate[] =
      R"(function setIcon(details) {
           chrome.%s.setIcon(details, () => {
             chrome.test.assertLastError("%s");
             chrome.test.notifyPass();
           });
         })";

  constexpr char kExpectedError[] =
      "Could not load action icon 'does_not_exist.png'.";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));

  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtmlTemplate);
  test_dir.WriteFile(
      FILE_PATH_LITERAL("page.js"),
      base::StringPrintf(kPageJsTemplate, GetAPINameForActionType(GetParam()),
                         kExpectedError));

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  int tab_id = GetActiveTabId();
  EXPECT_TRUE(ActionHasDefaultState(*action, tab_id));
  EnsureActionIsEnabledOnActiveTab(action);

  // Calling setIcon with an invalid path in a non-service worker context should
  // emit a console error in that context and call the callback with lastError
  // set.
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(kExpectedError);

  constexpr char kSetIconScript[] =
      "setIcon({tabId: %d, path: 'does_not_exist.png'});";
  RunTestAndWaitForSuccess(web_contents,
                           base::StringPrintf(kSetIconScript, tab_id));
  ASSERT_TRUE(console_observer.Wait());
}

// Tests calling setIcon() in the service worker with an invalid icon paths
// specified. Regression test for https://crbug.com/1262029. Regression test for
// https://crbug.com/1372518.
IN_PROC_BROWSER_TEST_F(ExtensionActionAPITest, SetIconInWorkerWithInvalidPath) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Bad Icon Path In Worker",
           "manifest_version": 3,
           "version": "0.1",
           "action": {},
           "background": {"service_worker": "worker.js" }
         })";

  constexpr char kBackgroundJs[] =
      R"(let expectedError = "%s";
         let anotherExpectedError = "%s";
         const singlePath = 'does_not_exist.png';
         const multiplePaths = {
           16: 'does_not_exist.png',
           32: 'also_does_not_exist.png'
         };

         chrome.test.runTests([
           function singleWithCallback() {
             chrome.action.setIcon({path: singlePath}, () => {
               chrome.test.assertLastError(expectedError);
               chrome.test.succeed();
             });
           },
           async function singleWithPromise() {
             await chrome.test.assertPromiseRejects(
                 chrome.action.setIcon({path: singlePath}),
                 'Error: ' + expectedError);
             chrome.test.succeed();
           },
           /*
              Multiple icons are loaded asynchronously and either one could
              end up failing first. However only the first error is emitted,
              we check against both possibilities.
           */
           function multipleWithCallback() {
             chrome.action.setIcon({ path: multiplePaths }, () => {
               let errorMessage = chrome.runtime.lastError.message;
               chrome.test.assertTrue(errorMessage === expectedError
                 || errorMessage === anotherExpectedError);
               chrome.test.succeed();
             });
           },
           function multipleWithPromise() {
             chrome.action.setIcon({ path: multiplePaths })
               .then(() => {
                 chrome.test.fail();
               })
               .catch((error) => {
                 chrome.test.assertTrue(error.message === expectedError
                   || error.message === anotherExpectedError);
                 chrome.test.succeed();
               });
           }
         ]);)";

  constexpr char kExpectedError[] =
      "Failed to set icon 'does_not_exist.png': Failed to fetch";
  constexpr char kAnotherExpectedError[] =
      "Failed to set icon 'also_does_not_exist.png': Failed to fetch";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifestTemplate);

  test_dir.WriteFile(
      FILE_PATH_LITERAL("worker.js"),
      base::StringPrintf(kBackgroundJs, kExpectedError, kAnotherExpectedError));

  // Calling setIcon with an invalid path in a service worker context should
  // reject the promise or call the callback with lastError set.
  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Tests multiple cases of setting an invalid popup that violate same-origin
// checks.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, SetPopupWithInvalidPath) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Invalid Popup Path",
           "manifest_version": %d,
           "version": "0.1",
           "%s": {}
         })";
  constexpr char kSetPopupJsTemplate[] =
      R"(
        function setPopup(details, expectedError) {
          chrome.%s.setPopup(details, () => {
            chrome.test.assertLastError(expectedError);
            chrome.test.succeed();
          });
        };
      )";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"),
                     "// This space left blank.");
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtmlTemplate);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"),
                     base::StringPrintf(kSetPopupJsTemplate,
                                        GetAPINameForActionType(GetParam())));
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  auto get_script = [](int tab_id, const char* popup_input) {
    constexpr char kSetPopup[] = R"(setPopup({tabId: %d, popup: '%s'}, "%s");)";
    return base::StringPrintf(kSetPopup, tab_id, popup_input,
                              manifest_errors::kInvalidExtensionOriginPopup);
  };

  content::RenderFrameHost* navigated_host = ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html"));
  ASSERT_TRUE(navigated_host);
  content::WebContents* web_contents = GetActiveTab();
  int tab_id = GetActiveTabId();

  // Set the popup to an invalid nonexistent extension URL and expect an error.
  {
    static constexpr char kInvalidPopupUrl[] =
        "chrome-extension://notavalidextensionid/popup.html";
    RunTestAndWaitForSuccess(web_contents,
                             get_script(tab_id, kInvalidPopupUrl));
  }

  // Set the popup to a web URL and expect an error.
  {
    static constexpr char kWebUrl[] = "http://test.com";
    RunTestAndWaitForSuccess(web_contents, get_script(tab_id, kWebUrl));
  }

  // Set the popup to another existing extension and expect an error.
  {
    TestExtensionDir different_extension_dir;
    different_extension_dir.WriteManifest(base::StringPrintf(
        kManifestTemplate, GetManifestVersionForActionType(GetParam()),
        ActionInfo::GetManifestKeyForActionType(GetParam())));
    different_extension_dir.WriteFile(FILE_PATH_LITERAL("popup.html"),
                                      "// This space left blank.");
    const Extension* different_extension =
        LoadExtension(different_extension_dir.UnpackedPath());
    ASSERT_TRUE(different_extension);
    const std::string different_extension_popup_url =
        different_extension->GetResourceURL("popup.html").spec();
    RunTestAndWaitForSuccess(
        web_contents,
        get_script(tab_id, different_extension_popup_url.c_str()));
  }
}

// Tests various getter and setter methods.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, GettersAndSetters) {
  // Load up an extension with default values.
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Getters and Setters",
           "manifest_version": %d,
           "version": "0.1",
           "%s": {
             "default_title": "default title",
             "default_popup": "default_popup.html"
           }
         })";
  constexpr char kPageJs[] = "// Intentionally blank.";
  constexpr char kPopupHtml[] =
      "<!doctype html><html><body>Blank</body></html>";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtmlTemplate);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("default_popup.html"), kPopupHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("custom_popup1.html"), kPopupHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("custom_popup2.html"), kPopupHtml);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  int first_tab_id = GetActiveTabId();

  // Open a tab to run the extension commands in.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension->GetResourceURL("page.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // And a second new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  int second_tab_id = GetActiveTabId();

  // A simple structure to hold different representations of values (one JS,
  // one C++).
  struct ValuePair {
    std::string cpp;
    std::string js;

    bool operator!=(const ValuePair& rhs) const {
      return rhs.cpp != this->cpp || rhs.js != this->js;
    }
  };

  // A function that returns the the C++ result for the given ExtensionAction
  // and tab id.
  using CPPValueGetter =
      base::RepeatingCallback<std::string(ExtensionAction*, int)>;

  auto run_test =
      [action, first_tab_id, second_tab_id](
          ActionTestHelper& test_helper, const ValuePair& default_value,
          const ValuePair& custom_value1, const ValuePair& custom_value2,
          CPPValueGetter value_getter) {
        // Ensure all values are mutually exclusive.
        EXPECT_NE(default_value, custom_value1);
        EXPECT_NE(default_value, custom_value2);
        EXPECT_NE(custom_value1, custom_value2);

        SCOPED_TRACE(base::StringPrintf(
            "default: '%s', custom1: '%s', custom2: '%s'",
            default_value.cpp.c_str(), custom_value1.cpp.c_str(),
            custom_value2.cpp.c_str()));

        // A helper to check the value of a property of the action in both
        // C++ (from the ExtensionAction object) and in JS (through the API
        // method).
        auto check_value = [action, &value_getter, &test_helper](
                               const ValuePair& expected_value, int tab_id) {
          EXPECT_EQ(expected_value.cpp, value_getter.Run(action, tab_id));
          if (tab_id == ExtensionAction::kDefaultTabId)
            test_helper.CheckDefaultValue(expected_value.js.c_str());
          else
            test_helper.CheckValueForTab(expected_value.js.c_str(), tab_id);
        };

        // Page actions don't support setting a default value (because they are
        // inherently tab-specific).
        bool supports_default = GetParam() != ActionInfo::Type::kPage;

        // Check the initial state. These should start at the defaults.
        if (supports_default)
          check_value(default_value, ExtensionAction::kDefaultTabId);
        check_value(default_value, first_tab_id);
        check_value(default_value, second_tab_id);

        // Set the value for the first tab to be the first custom value.
        test_helper.SetValueForTab(custom_value1.js.c_str(), first_tab_id);

        // The first tab should have the custom value, while the second tab
        // (and the default tab, if supported) should still have the default
        // value.
        if (supports_default)
          check_value(default_value, ExtensionAction::kDefaultTabId);
        check_value(custom_value1, first_tab_id);
        check_value(default_value, second_tab_id);

        if (supports_default) {
          // Change the default value to the second custom value.
          test_helper.SetDefaultValue(custom_value2.js.c_str());

          // Now, the default and second tab should each have the second custom
          // value. Since the first tab had its own value set, it should still
          // be set to the first custom value.
          check_value(custom_value2, ExtensionAction::kDefaultTabId);
          check_value(custom_value1, first_tab_id);
          check_value(custom_value2, second_tab_id);
        }
      };

  const char* kApiName = GetAPINameForActionType(GetParam());

  {
    // setPopup/getPopup.
    GURL default_popup_url = extension->GetResourceURL("default_popup.html");
    GURL custom_popup_url1 = extension->GetResourceURL("custom_popup1.html");
    GURL custom_popup_url2 = extension->GetResourceURL("custom_popup2.html");
    ValuePair default_popup{default_popup_url.spec(),
                            base::StrCat({"'", default_popup_url.spec(), "'"})};
    ValuePair custom_popup1{custom_popup_url1.spec(),
                            base::StrCat({"'", custom_popup_url1.spec(), "'"})};
    ValuePair custom_popup2{custom_popup_url2.spec(),
                            base::StrCat({"'", custom_popup_url2.spec(), "'"})};

    auto get_popup = [](ExtensionAction* action, int tab_id) {
      return action->GetPopupUrl(tab_id).spec();
    };

    ActionTestHelper popup_helper(kApiName, "setPopup", "getPopup", "popup",
                                  web_contents);
    run_test(popup_helper, default_popup, custom_popup1, custom_popup2,
             base::BindRepeating(get_popup));
  }
  {
    // setTitle/getTitle.
    ValuePair default_title{"default title", "'default title'"};
    ValuePair custom_title1{"custom title1", "'custom title1'"};
    ValuePair custom_title2{"custom title2", "'custom title2'"};

    auto get_title = [](ExtensionAction* action, int tab_id) {
      return action->GetTitle(tab_id);
    };

    ActionTestHelper title_helper(kApiName, "setTitle", "getTitle", "title",
                                  web_contents);
    run_test(title_helper, default_title, custom_title1, custom_title2,
             base::BindRepeating(get_title));
  }

  // Page actions don't have badges; for them, the test is done.
  if (GetParam() == ActionInfo::Type::kPage) {
    return;
  }

  {
    // setBadgeText/getBadgeText.
    ValuePair default_badge_text{"", "''"};
    ValuePair custom_badge_text1{"custom badge1", "'custom badge1'"};
    ValuePair custom_badge_text2{"custom badge2", "'custom badge2'"};

    auto get_badge_text = [](ExtensionAction* action, int tab_id) {
      return action->GetExplicitlySetBadgeText(tab_id);
    };

    ActionTestHelper badge_text_helper(kApiName, "setBadgeText", "getBadgeText",
                                       "text", web_contents);
    run_test(badge_text_helper, default_badge_text, custom_badge_text1,
             custom_badge_text2, base::BindRepeating(get_badge_text));
  }
  {
    // setBadgeBackgroundColor/getBadgeBackgroundColor.
    ValuePair default_badge_color{"0,0,0", "[0, 0, 0, 0]"};
    ValuePair custom_badge_color1{"255,0,0", "[255, 0, 0, 255]"};
    ValuePair custom_badge_color2{"0,255,0", "[0, 255, 0, 255]"};

    auto get_badge_color = [](ExtensionAction* action, int tab_id) {
      return color_utils::SkColorToRgbString(
          action->GetBadgeBackgroundColor(tab_id));
    };

    ActionTestHelper badge_color_helper(kApiName, "setBadgeBackgroundColor",
                                        "getBadgeBackgroundColor", "color",
                                        web_contents);
    run_test(badge_color_helper, default_badge_color, custom_badge_color1,
             custom_badge_color2, base::BindRepeating(get_badge_color));
  }

  // TODO(crbug.com/40870872): Test using HTML colors instead of just color
  // arrays, including set/getBadgeBackgroundColor.
  // setBadgeTextColor/getBadgeTextColor.
  // This API is only supported on MV3.
  if (GetParam() != ActionInfo::Type::kBrowser) {
    {
      ValuePair default_badge_text_color{"0,0,0", "[0, 0, 0, 0]"};
      ValuePair custom_badge_text_color1{"255,0,0", "[255, 0, 0, 255]"};
      ValuePair custom_badge_text_color2{"0,255,0", "[0, 255, 0, 255]"};

      auto get_badge_text_color = [](ExtensionAction* action, int tab_id) {
        return color_utils::SkColorToRgbString(
            action->GetBadgeTextColor(tab_id));
      };

      ActionTestHelper badge_text_color_helper(kApiName, "setBadgeTextColor",
                                               "getBadgeTextColor", "color",
                                               web_contents);
      run_test(badge_text_color_helper, default_badge_text_color,
               custom_badge_text_color1, custom_badge_text_color2,
               base::BindRepeating(get_badge_text_color));
    }
  }
}

// Tests the functions to enable and disable extension actions.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, EnableAndDisable) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "enabled/disabled action test",
           "version": "0.1",
           "manifest_version": %d,
           "%s": {}
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtmlTemplate);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), "// This space left blank.");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  const int tab_id1 = GetActiveTabId();
  EnsureActionIsEnabledOnTab(action, tab_id1);

  // Open a tab to run the extension commands in.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension->GetResourceURL("page.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  const int tab_id2 = GetActiveTabId();
  EnsureActionIsEnabledOnTab(action, tab_id2);

  EXPECT_NE(tab_id1, tab_id2);

  const char* enable_function = nullptr;
  const char* disable_function = nullptr;
  switch (GetParam()) {
    case ActionInfo::Type::kAction:
    case ActionInfo::Type::kBrowser:
      enable_function = "enable";
      disable_function = "disable";
      break;
    case ActionInfo::Type::kPage:
      enable_function = "show";
      disable_function = "hide";
      break;
  }

  // Start by toggling the extension action on the current tab.
  {
    constexpr char kScriptTemplate[] =
        R"(chrome.%s.%s(%d, () => {
            chrome.test.assertNoLastError();
            chrome.test.notifyPass();
           });)";
    RunTestAndWaitForSuccess(
        web_contents,
        base::StringPrintf(kScriptTemplate, GetAPINameForActionType(GetParam()),
                           disable_function, tab_id2));
    EXPECT_FALSE(action->GetIsVisible(tab_id2));
    EXPECT_TRUE(action->GetIsVisible(tab_id1));
  }

  {
    constexpr char kScriptTemplate[] =
        R"(chrome.%s.%s(%d, () => {
            chrome.test.assertNoLastError();
            chrome.test.notifyPass();
           });)";
    RunTestAndWaitForSuccess(
        web_contents,
        base::StringPrintf(kScriptTemplate, GetAPINameForActionType(GetParam()),
                           enable_function, tab_id2));
    EXPECT_TRUE(action->GetIsVisible(tab_id2));
    EXPECT_TRUE(action->GetIsVisible(tab_id1));
  }

  // Page actions can't be enabled/disabled globally, but others can. Try
  // toggling global state by omitting the tab id if the type isn't a page
  // action.
  if (GetParam() == ActionInfo::Type::kPage) {
    return;
  }

  // We need to undo the explicit enable from above, since tab-specific
  // values take precedence.
  action->ClearAllValuesForTab(tab_id2);
  {
    constexpr char kScriptTemplate[] =
        R"(chrome.%s.%s(() => {
            chrome.test.assertNoLastError();
            chrome.test.notifyPass();
           });)";
    RunTestAndWaitForSuccess(
        web_contents,
        base::StringPrintf(kScriptTemplate, GetAPINameForActionType(GetParam()),
                           disable_function));
    EXPECT_EQ(false, action->GetIsVisible(tab_id2));
    EXPECT_EQ(false, action->GetIsVisible(tab_id1));
  }

  {
    constexpr char kScriptTemplate[] =
        R"(chrome.%s.%s(() => {
            chrome.test.assertNoLastError();
            chrome.test.notifyPass();
           });)";
    RunTestAndWaitForSuccess(
        web_contents,
        base::StringPrintf(kScriptTemplate, GetAPINameForActionType(GetParam()),
                           enable_function));
    EXPECT_EQ(true, action->GetIsVisible(tab_id2));
    EXPECT_EQ(true, action->GetIsVisible(tab_id1));
  }
}

// Tests that the check for enabled and disabled status are correctly reported.
IN_PROC_BROWSER_TEST_F(ExtensionActionAPITest, IsEnabled) {
  ASSERT_TRUE(RunExtensionTest("extension_action/is_enabled")) << message_;
}

// Tests that isEnabled correctly ignores declarativeContent rules for enable.
IN_PROC_BROWSER_TEST_F(ExtensionActionAPITest, IsEnabledIgnoreDeclarative) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Declarative content ignored test",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["activeTab", "declarativeContent"],
           "background": {
             "service_worker" : "background.js"
           },
           "action": {}
         })";
  constexpr char kSetupDeclarativeContent[] =
      R"(
          let rule1 = {
            conditions: [
              new chrome.declarativeContent.PageStateMatcher({
                pageUrl: { hostContains: 'google'},
              })
            ],
            actions: [ new chrome.declarativeContent.ShowAction() ]
          };
          chrome.runtime.onInstalled.addListener(function(details) {
            chrome.declarativeContent.onPageChanged.removeRules(
              undefined, function() {
                chrome.declarativeContent.onPageChanged.addRules(
                  [rule1], () => {
                    chrome.test.sendMessage('ready');
                });
            });
          });
          // Set tab disabled globally so that we can assert that the extension
          // cannot know the enable status for declarativeContent tabs it is
          // registered for.
          chrome.action.disable();
        )";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifestTemplate);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     kSetupDeclarativeContent);

  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  auto* action_manager = ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url(embedded_test_server()->GetURL("google.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(web_contents, url));
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  // Confirm that the tab is only visible for declarativeContent.
  ASSERT_TRUE(action->GetIsVisible(tab_id));
  ASSERT_FALSE(action->GetIsVisibleIgnoringDeclarative(tab_id));

  constexpr char kCheckIsEnabledStatusForTabId[] =
      R"(
        chrome.action.isEnabled(%d, (enabled) => {
          chrome.test.sendScriptResult(enabled);
        });
      )";
  base::Value script_result = BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(),
      base::StringPrintf(kCheckIsEnabledStatusForTabId, tab_id),
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  EXPECT_FALSE(script_result.GetBool());
}

using ActionAPITest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(ActionAPITest, TestGetUserSettings) {
  constexpr char kManifest[] =
      R"({
           "name": "getUserSettings Test",
           "manifest_version": 3,
           "version": "1",
           "background": {"service_worker": "worker.js"},
           "action": {}
         })";
  constexpr char kWorker[] =
      R"(chrome.action.onClicked.addListener(async () => {
           const settings = await chrome.action.getUserSettings();
           chrome.test.sendMessage(JSON.stringify(settings));
         });
         chrome.test.sendMessage('ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);

  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("ready");
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  ToolbarActionsModel* const toolbar_model =
      ToolbarActionsModel::Get(profile());
  EXPECT_FALSE(toolbar_model->IsActionPinned(extension->id()));

  std::unique_ptr<ExtensionActionTestHelper> toolbar_helper =
      ExtensionActionTestHelper::Create(browser());

  auto get_response = [extension, toolbar_helper = toolbar_helper.get()]() {
    ExtensionTestMessageListener listener;
    listener.set_extension_id(extension->id());
    toolbar_helper->Press(extension->id());
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    return listener.message();
  };

  EXPECT_EQ(R"({"isOnToolbar":false})", get_response());

  toolbar_model->SetActionVisibility(extension->id(), true);
  EXPECT_TRUE(toolbar_model->IsActionPinned(extension->id()));

  EXPECT_EQ(R"({"isOnToolbar":true})", get_response());
}

// Tests dispatching the onUserSettingsChanged event to listeners when the user
// pins or unpins the extension action.
IN_PROC_BROWSER_TEST_F(ActionAPITest, OnUserSettingsChanged) {
  constexpr char kManifest[] =
      R"({
           "name": "onUserSettingsChanged Test",
           "manifest_version": 3,
           "version": "1",
           "background": {"service_worker": "worker.js"},
           "action": {}
         })";
  constexpr char kWorker[] =
      R"(chrome.action.onUserSettingsChanged.addListener(change => {
           chrome.test.sendMessage(JSON.stringify(change));
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ToolbarActionsModel* const toolbar_model =
      ToolbarActionsModel::Get(profile());
  ASSERT_FALSE(toolbar_model->IsActionPinned(extension->id()));

  auto change_visibility_and_get_response = [extension,
                                             toolbar_model](bool pinned_state) {
    ExtensionTestMessageListener listener;
    listener.set_extension_id(extension->id());
    toolbar_model->SetActionVisibility(extension->id(), pinned_state);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    return listener.message();
  };

  EXPECT_EQ(R"({"isOnToolbar":true})",
            change_visibility_and_get_response(/*pinned_state=*/true));

  EXPECT_EQ(R"({"isOnToolbar":false})",
            change_visibility_and_get_response(/*pinned_state=*/false));
}

// Tests that invalid badge text colors return an API error to the caller.
IN_PROC_BROWSER_TEST_F(ActionAPITest, TestBadgeTextColorErrors) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  constexpr char kManifestTemplate[] =
      R"({
           "name": "alpha transparent error test",
           "version": "0.1",
           "manifest_version": 3,
           "action": {},
           "background": {"service_worker": "background.js" }
         })";
  static constexpr char kBackgroundJs[] =
      R"(
        const tabId = %d;
        const expectedError = '%s';
        chrome.test.runTests([
          async function badgeColorEmptyValueInvalid() {
            await chrome.test.assertPromiseRejects(
              chrome.action.setBadgeTextColor(
                {color: '', tabId}),
              'Error: ' + expectedError);
            chrome.test.succeed();
          },
          async function badgeColorAlphaTransparentInvalid() {
            await chrome.test.assertPromiseRejects(
              chrome.action.setBadgeTextColor(
                {color: [255, 255, 255, 0], tabId}),
              'Error: ' + expectedError);
            chrome.test.succeed();
          }
        ]);
      )";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifestTemplate);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtmlTemplate);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(kBackgroundJs, tab_id,
                                        extension_misc::kInvalidColorError));

  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Tests the setting and unsetting of badge text works for both global and tab
// specific cases.
IN_PROC_BROWSER_TEST_P(ActionAndBrowserActionAPITest,
                       TestSetBadgeTextGlobalAndTab) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test unsetting tab specific test",
           "version": "0.1",
           "manifest_version": %d,
           "%s": {},
           "background": { %s }
         })";
  const char* background_specification =
      GetParam() == ActionInfo::Type::kAction
          ? R"("service_worker": "background.js")"
          : R"("scripts": ["background.js"])";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam()),
      background_specification));
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// Empty");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  const int tab_id1 = GetActiveTabId();
  EnsureActionIsEnabledOnTab(action, tab_id1);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  const int tab_id2 = GetActiveTabId();
  EnsureActionIsEnabledOnTab(action, tab_id2);

  constexpr char kGlobalText[] = "Global text";
  constexpr char kTabText[] = "Tab text";

  const std::string kSetGlobalText = base::StringPrintf(
      R"(
        chrome.%s.setBadgeText({text: 'Global text'}, () => {
          chrome.test.sendScriptResult(true);
        });
      )",
      GetAPINameForActionType(GetParam()));
  const std::string kUnsetGlobalText = base::StringPrintf(
      R"(
        chrome.%s.setBadgeText({}, () => {
          chrome.test.sendScriptResult(true);
        });
      )",
      GetAPINameForActionType(GetParam()));
  const std::string kSetTabText = base::StringPrintf(
      R"(
        chrome.%s.setBadgeText({tabId: %d, text: 'Tab text'}, () => {
          chrome.test.sendScriptResult(true);
        });
      )",
      GetAPINameForActionType(GetParam()), tab_id1);
  const std::string kUnsetTabText = base::StringPrintf(
      R"(
        chrome.%s.setBadgeText({tabId: %d}, () => {
          chrome.test.sendScriptResult(true);
        });
      )",
      GetAPINameForActionType(GetParam()), tab_id1);

  auto run_script_and_wait_for_callback = [&](std::string script) {
    base::Value script_result = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    return script_result;
  };

  EXPECT_EQ("", action->GetExplicitlySetBadgeText(tab_id1));
  EXPECT_EQ("", action->GetExplicitlySetBadgeText(tab_id2));

  // Set a global text for all tabs.
  EXPECT_TRUE(run_script_and_wait_for_callback(kSetGlobalText).GetBool());
  EXPECT_EQ(kGlobalText, action->GetExplicitlySetBadgeText(tab_id1));
  EXPECT_EQ(kGlobalText, action->GetExplicitlySetBadgeText(tab_id2));

  // Now set a tab specific text for tab 1.
  EXPECT_TRUE(run_script_and_wait_for_callback(kSetTabText).GetBool());
  EXPECT_EQ(kTabText, action->GetExplicitlySetBadgeText(tab_id1));
  EXPECT_EQ(kGlobalText, action->GetExplicitlySetBadgeText(tab_id2));

  // Unsetting the global text will leave the tab specific text in place.
  EXPECT_TRUE(run_script_and_wait_for_callback(kUnsetGlobalText).GetBool());
  EXPECT_EQ(kTabText, action->GetExplicitlySetBadgeText(tab_id1));
  EXPECT_EQ("", action->GetExplicitlySetBadgeText(tab_id2));

  // Adding the global text back will not effect the tab specific text.
  EXPECT_TRUE(run_script_and_wait_for_callback(kSetGlobalText).GetBool());
  EXPECT_EQ(kTabText, action->GetExplicitlySetBadgeText(tab_id1));
  EXPECT_EQ(kGlobalText, action->GetExplicitlySetBadgeText(tab_id2));

  // Unsetting the tab specific text will return that tab to the global text.
  EXPECT_TRUE(run_script_and_wait_for_callback(kUnsetTabText).GetBool());
  EXPECT_EQ(kGlobalText, action->GetExplicitlySetBadgeText(tab_id1));
  EXPECT_EQ(kGlobalText, action->GetExplicitlySetBadgeText(tab_id2));

  // Finally unsetting the global text will return us back to nothing set.
  EXPECT_TRUE(run_script_and_wait_for_callback(kUnsetGlobalText).GetBool());
  EXPECT_EQ("", action->GetExplicitlySetBadgeText(tab_id1));
  EXPECT_EQ("", action->GetExplicitlySetBadgeText(tab_id2));
}

class ExtensionActionWithOpenPopupFeatureDisabledTest
    : public ExtensionActionAPITest {
 public:
  ExtensionActionWithOpenPopupFeatureDisabledTest() {
    feature_list_.InitAndDisableFeature(
        extensions_features::kApiActionOpenPopup);
  }
  ~ExtensionActionWithOpenPopupFeatureDisabledTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the action.openPopup() API is available to policy-installed
// extensions on even if the feature flag is disabled. Since this is controlled
// through our features files (which are tested separately), this is more of a
// smoke test than an end-to-end test.
// TODO(crbug.com/40057101): Remove this test when the API is available
// for all extensions on stable without a feature flag.
IN_PROC_BROWSER_TEST_F(ExtensionActionWithOpenPopupFeatureDisabledTest,
                       OpenPopupAvailabilityOnStableChannel) {
  TestExtensionDir test_dir;
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "action": {}
         })";
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     "chrome.test.sendMessage('ready');");

  auto is_open_popup_defined = [this](const Extension& extension) {
    static constexpr char kScript[] =
        R"(chrome.test.sendScriptResult(!!chrome.action.openPopup);)";
    return BackgroundScriptExecutor::ExecuteScript(
        profile(), extension.id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  };

  // Technically, we don't need the "ready" listener here, but this ensures we
  // don't cross streams with the policy extension loaded below (where we do
  // need the listener).
  ExtensionTestMessageListener non_policy_listener("ready");
  const Extension* non_policy_extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(non_policy_extension);
  ASSERT_TRUE(non_policy_listener.WaitUntilSatisfied());

  // Somewhat annoying: due to how our test helpers are written,
  // `EXPECT_EQ(false, base::Value)` works, but EXPECT_FALSE(base::Value) does
  // not.
  EXPECT_EQ(false, is_open_popup_defined(*non_policy_extension));

  // Unlike `LoadExtension()`, `InstallExtension()` doesn't wait for the service
  // worker to be ready, so we need a few manual waiters.
  base::FilePath packed_path = test_dir.Pack();
  service_worker_test_utils::TestServiceWorkerContextObserver
      registration_observer(profile());
  ExtensionTestMessageListener policy_listener("ready");
  const Extension* policy_extension = InstallExtension(
      packed_path, 1, mojom::ManifestLocation::kExternalPolicyDownload);
  ASSERT_TRUE(policy_extension);
  ASSERT_TRUE(policy_listener.WaitUntilSatisfied());
  registration_observer.WaitForRegistrationStored();

  EXPECT_EQ(true, is_open_popup_defined(*policy_extension));
}

INSTANTIATE_TEST_SUITE_P(All,
                         MultiActionAPITest,
                         testing::Values(ActionInfo::Type::kAction,
                                         ActionInfo::Type::kPage,
                                         ActionInfo::Type::kBrowser));

INSTANTIATE_TEST_SUITE_P(All,
                         ActionAndBrowserActionAPITest,
                         testing::Values(ActionInfo::Type::kAction,
                                         ActionInfo::Type::kBrowser));

INSTANTIATE_TEST_SUITE_P(All,
                         MultiActionAPICanvasTest,
                         testing::Values(ActionInfo::Type::kAction,
                                         ActionInfo::Type::kPage,
                                         ActionInfo::Type::kBrowser));

}  // namespace extensions
