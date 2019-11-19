// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/extension_action/test_extension_action_api_observer.h"
#include "chrome/browser/extensions/api/extension_action/test_icon_image_observer.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/color_utils.h"

namespace extensions {
namespace {

// Runs |script| in the background page of the extension with the given
// |extension_id|, and waits for it to send a test-passed result. This will
// fail if the test in |script| fails.
void RunTestAndWaitForSuccess(Profile* profile,
                              const ExtensionId& extension_id,
                              const std::string& script) {
  SCOPED_TRACE(script);
  ResultCatcher result_catcher;
  browsertest_util::ExecuteScriptInBackgroundPageNoWait(profile, extension_id,
                                                        script);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

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

// A helper class to handle setting or getting the values for an action from JS.
class ActionTestHelper {
 public:
  ActionTestHelper(const char* api_name,
                   const char* set_method_name,
                   const char* get_method_name,
                   const char* js_property_key,
                   Profile* profile,
                   const ExtensionId& extension_id)
      : api_name_(api_name),
        set_method_name_(set_method_name),
        get_method_name_(get_method_name),
        js_property_key_(js_property_key),
        profile_(profile),
        extension_id_(extension_id) {}
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
        profile_, extension_id_,
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
        profile_, extension_id_,
        base::StringPrintf(kScriptTemplate, api_name_, get_method_name_,
                           expected_js_value));
  }

  // Sets the value for a given |tab_id|.
  void SetValueForTab(const char* new_js_value, int tab_id) const {
    constexpr char kScriptTemplate[] =
        R"(chrome.%s.%s({tabId: %d, %s: %s}, () => {
             chrome.test.assertNoLastError();
             chrome.test.notifyPass();
           });)";
    RunTestAndWaitForSuccess(
        profile_, extension_id_,
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
        profile_, extension_id_,
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
  // The associated profile.
  Profile* const profile_;
  // The id of the extension.
  const ExtensionId extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ActionTestHelper);
};

}  // namespace

class ExtensionActionAPITest : public ExtensionApiTest {
 public:
  ExtensionActionAPITest() {}
  ~ExtensionActionAPITest() override {}

  const char* GetManifestKey(ActionInfo::Type action_type) {
    switch (action_type) {
      case ActionInfo::TYPE_ACTION:
        return manifest_keys::kAction;
      case ActionInfo::TYPE_BROWSER:
        return manifest_keys::kBrowserAction;
      case ActionInfo::TYPE_PAGE:
        return manifest_keys::kPageAction;
    }
    NOTREACHED();
    return nullptr;
  }

  const char* GetAPIName(ActionInfo::Type action_type) {
    switch (action_type) {
      case ActionInfo::TYPE_ACTION:
        return "action";
      case ActionInfo::TYPE_BROWSER:
        return "browserAction";
      case ActionInfo::TYPE_PAGE:
        return "pageAction";
    }
    NOTREACHED();
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionActionAPITest);
};

// Alias these for readability, when a test only exercises one type of action.
using BrowserActionAPITest = ExtensionActionAPITest;
using PageActionAPITest = ExtensionActionAPITest;

// A class that runs tests exercising each type of possible toolbar action.
class MultiActionAPITest
    : public ExtensionActionAPITest,
      public testing::WithParamInterface<ActionInfo::Type> {
 public:
  MultiActionAPITest()
      : current_channel_(
            extension_test_util::GetOverrideChannelForActionType(GetParam())) {}

  // Returns true if the |action| has whatever state its default is on the
  // tab with the given |tab_id|.
  bool ActionHasDefaultState(const ExtensionAction& action, int tab_id) const {
    bool is_visible = action.GetIsVisible(tab_id);
    bool default_is_visible =
        action.default_state() == ActionInfo::STATE_ENABLED;
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
    return SessionTabHelper::IdForTab(web_contents).id();
  }

  content::WebContents* GetActiveTab() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Returns the action associated with |extension|.
  ExtensionAction* GetExtensionAction(const Extension& extension) {
    auto* action_manager = ExtensionActionManager::Get(profile());
    return action_manager->GetExtensionAction(extension);
  }

 private:
  std::unique_ptr<ScopedCurrentChannel> current_channel_;

  DISALLOW_COPY_AND_ASSIGN(MultiActionAPITest);
};

// Canvas tests rely on the harness producing pixel output in order to read back
// pixels from a canvas element. So we have to override the setup function.
class MultiActionAPICanvasTest : public MultiActionAPITest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    ExtensionActionAPITest::SetUp();
  }
};

// Check that updating the browser action badge for a specific tab id does not
// cause a disk write (since we only persist the defaults).
// Only browser actions persist settings.
IN_PROC_BROWSER_TEST_F(BrowserActionAPITest, TestNoUnnecessaryIO) {
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
IN_PROC_BROWSER_TEST_P(MultiActionAPITest,
                       ValuesAreClearedOnNavigationAndTabRemoval) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Extension",
           "description": "An extension",
           "manifest_version": 2,
           "version": "0.1",
           "%s": {}
         })";

  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey(GetParam())));
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

// Tests that tooltips of an extension action icon can be specified using UTF8.
// See http://crbug.com/25349.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, TitleLocalization) {
  TestExtensionDir test_dir;
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Hreggvi\u00F0ur is my name",
           "description": "Hreggvi\u00F0ur: l10n action",
           "manifest_version": 2,
           "version": "0.1",
           "%s": {
             "default_title": "Hreggvi\u00F0ur"
           }
         })";

  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey(GetParam())));
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
  int tab_id = SessionTabHelper::IdForTab(web_contents).id();
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
           "manifest_version": 2,
           "version": "0.1",
           "%s": {},
           "background": { "scripts": ["background.js"] }
         })";
  constexpr char kBackgroundJsTemplate[] =
      R"(chrome.%s.onClicked.addListener((tab) => {
           // Check a few properties on the tabs object to make sure it's sane.
           chrome.test.assertTrue(!!tab);
           chrome.test.assertTrue(tab.id > 0);
           chrome.test.assertTrue(tab.index > -1);
           chrome.test.notifyPass();
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey(GetParam())));
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundJsTemplate, GetAPIName(GetParam())));

  // Though this says "BrowserActionTestUtil", it's actually used for all
  // toolbar actions.
  // TODO(devlin): Rename it to ToolbarActionTestUtil.
  std::unique_ptr<BrowserActionTestUtil> toolbar_helper =
      BrowserActionTestUtil::Create(browser());
  EXPECT_EQ(0, toolbar_helper->NumberOfBrowserActions());
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_EQ(1, toolbar_helper->NumberOfBrowserActions());
  EXPECT_EQ(extension->id(), toolbar_helper->GetExtensionId(0));

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  const int tab_id = GetActiveTabId();
  EXPECT_TRUE(ActionHasDefaultState(*action, tab_id));
  EnsureActionIsEnabledOnActiveTab(action);
  EXPECT_FALSE(action->HasPopup(tab_id));

  ResultCatcher result_catcher;
  toolbar_helper->Press(0);
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Tests the creation of a popup when one is specified in the manifest.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, PopupCreation) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Clicking",
           "manifest_version": 2,
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
  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), kPopupHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.js"), kPopupJs);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  std::unique_ptr<BrowserActionTestUtil> toolbar_helper =
      BrowserActionTestUtil::Create(browser());

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  const int tab_id = GetActiveTabId();
  EXPECT_TRUE(ActionHasDefaultState(*action, tab_id));
  EnsureActionIsEnabledOnActiveTab(action);
  EXPECT_TRUE(action->HasPopup(tab_id));

  ResultCatcher result_catcher;
  toolbar_helper->Press(0);
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
  EXPECT_TRUE(content::ExecuteScript(popup_contents, "window.close()"));
  contents_destroyed.Wait();

  frames = process_manager->GetRenderFrameHostsForExtension(extension->id());
  EXPECT_EQ(0u, frames.size());
}

// Tests setting the icon dynamically from the background page.
IN_PROC_BROWSER_TEST_P(MultiActionAPICanvasTest, DynamicSetIcon) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Clicking",
           "manifest_version": 2,
           "version": "0.1",
           "%s": {
             "default_icon": "red_icon.png"
           },
           "background": { "scripts": ["background.js"] }
         })";
  constexpr char kBackgroundJsTemplate[] =
      R"(function setIcon(details) {
           chrome.%s.setIcon(details, () => {
             chrome.test.assertNoLastError();
             chrome.test.notifyPass();
           });
         })";

  std::string blue_icon;
  std::string red_icon;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(
        test_data_dir_.AppendASCII("icon_rgb_0_0_255.png"), &blue_icon));
    ASSERT_TRUE(base::ReadFileToString(
        test_data_dir_.AppendASCII("icon_rgb_255_0_0.png"), &red_icon));
  }

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey(GetParam())));
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundJsTemplate, GetAPIName(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("blue_icon.png"), blue_icon);
  test_dir.WriteFile(FILE_PATH_LITERAL("red_icon.png"), red_icon);

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

  std::unique_ptr<BrowserActionTestUtil> toolbar_helper =
      BrowserActionTestUtil::Create(browser());

  ASSERT_EQ(1, toolbar_helper->NumberOfBrowserActions());
  EXPECT_EQ(extension->id(), toolbar_helper->GetExtensionId(0));

  gfx::Image default_icon = toolbar_helper->GetIcon(0);
  EXPECT_FALSE(default_icon.IsEmpty());

  // Check the midpoint. All these icons are solid, but the rendered icon
  // includes padding.
  const int mid_x = default_icon.Width() / 2;
  const int mid_y = default_icon.Height() / 2;
  // Note: We only validate the color here as a quick-and-easy way of validating
  // the icon is what we expect. Other tests do much more rigorous testing of
  // the icon's rendering.
  EXPECT_EQ(SK_ColorRED, default_icon.AsBitmap().getColor(mid_x, mid_y));

  // Create a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  const int new_tab_id = GetActiveTabId();
  EXPECT_NE(new_tab_id, tab_id);
  EXPECT_TRUE(ActionHasDefaultState(*action, new_tab_id));
  EnsureActionIsEnabledOnActiveTab(action);

  // The new tab should still have the same icon (the default).
  gfx::Image new_tab_icon = toolbar_helper->GetIcon(0);
  EXPECT_FALSE(default_icon.IsEmpty());
  EXPECT_EQ(SK_ColorRED, default_icon.AsBitmap().getColor(mid_x, mid_y));

  // Set the icon for the new tab to a different icon in the extension package.
  RunTestAndWaitForSuccess(
      profile(), extension->id(),
      base::StringPrintf("setIcon({tabId: %d, path: 'blue_icon.png'});",
                         new_tab_id));

  new_tab_icon = toolbar_helper->GetIcon(0);
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
      profile(), extension->id(),
      base::StringPrintf(kSetIconFromImageData, new_tab_id));

  new_tab_icon = toolbar_helper->GetIcon(0);
  EXPECT_FALSE(new_tab_icon.IsEmpty());
  EXPECT_EQ(SK_ColorGREEN, new_tab_icon.AsBitmap().getColor(mid_x, mid_y));

  // Switch back to the first tab. The icon should still be red, since the other
  // changes were for specific tabs.
  browser()->tab_strip_model()->ActivateTabAt(0);
  gfx::Image first_tab_icon = toolbar_helper->GetIcon(0);
  EXPECT_FALSE(first_tab_icon.IsEmpty());
  EXPECT_EQ(SK_ColorRED, first_tab_icon.AsBitmap().getColor(mid_x, mid_y));

  // TODO(devlin): Add tests for setting icons as a dictionary of
  // { size -> image_data }.
}

// Tests various getter and setter methods.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, GettersAndSetters) {
  // Load up an extension with default values.
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Getters and Setters",
           "manifest_version": 2,
           "version": "0.1",
           "%s": {
             "default_title": "default title",
             "default_popup": "default_popup.html"
           },
           "background": { "scripts": ["background.js"] }
         })";
  constexpr char kBackgroundJs[] = "// Intentionally blank.";
  constexpr char kPopupHtml[] =
      "<!doctype html><html><body>Blank</body></html>";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("default_popup.html"), kPopupHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("custom_popup1.html"), kPopupHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("custom_popup2.html"), kPopupHtml);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  int first_tab_id = GetActiveTabId();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
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
        bool supports_default = GetParam() != ActionInfo::TYPE_PAGE;

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

  const char* kApiName = GetAPIName(GetParam());

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
                                  profile(), extension->id());
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
                                  profile(), extension->id());
    run_test(title_helper, default_title, custom_title1, custom_title2,
             base::BindRepeating(get_title));
  }

  // Page actions don't have badges; for them, the test is done.
  if (GetParam() == ActionInfo::TYPE_PAGE)
    return;

  {
    // setBadgeText/getBadgeText.
    ValuePair default_badge_text{"", "''"};
    ValuePair custom_badge_text1{"custom badge1", "'custom badge1'"};
    ValuePair custom_badge_text2{"custom badge2", "'custom badge2'"};

    auto get_badge_text = [](ExtensionAction* action, int tab_id) {
      return action->GetExplicitlySetBadgeText(tab_id);
    };

    ActionTestHelper badge_text_helper(kApiName, "setBadgeText", "getBadgeText",
                                       "text", profile(), extension->id());
    run_test(badge_text_helper, default_badge_text, custom_badge_text1,
             custom_badge_text2, base::BindRepeating(get_badge_text));
  }
  {
    // setBadgeColor/getBadgeColor.
    ValuePair default_badge_color{"0,0,0", "[0, 0, 0, 0]"};
    ValuePair custom_badge_color1{"255,0,0", "[255, 0, 0, 255]"};
    ValuePair custom_badge_color2{"0,255,0", "[0, 255, 0, 255]"};

    auto get_badge_color = [](ExtensionAction* action, int tab_id) {
      return color_utils::SkColorToRgbString(
          action->GetBadgeBackgroundColor(tab_id));
    };

    ActionTestHelper badge_color_helper(kApiName, "setBadgeBackgroundColor",
                                        "getBadgeBackgroundColor", "color",
                                        profile(), extension->id());
    run_test(badge_color_helper, default_badge_color, custom_badge_color1,
             custom_badge_color2, base::BindRepeating(get_badge_color));
  }
}

// Tests the functions to enable and disable extension actions.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, EnableAndDisable) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "enabled/disabled action test",
           "version": "0.1",
           "manifest_version": 2,
           "%s": {},
           "background": {"scripts": ["background.js"]}
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     "// This space left blank.");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  const int tab_id1 = GetActiveTabId();
  EnsureActionIsEnabledOnTab(action, tab_id1);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  const int tab_id2 = GetActiveTabId();
  EnsureActionIsEnabledOnTab(action, tab_id2);

  EXPECT_NE(tab_id1, tab_id2);

  const char* enable_function = nullptr;
  const char* disable_function = nullptr;
  switch (GetParam()) {
    case ActionInfo::TYPE_ACTION:
    case ActionInfo::TYPE_BROWSER:
      enable_function = "enable";
      disable_function = "disable";
      break;
    case ActionInfo::TYPE_PAGE:
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
        profile(), extension->id(),
        base::StringPrintf(kScriptTemplate, GetAPIName(GetParam()),
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
        profile(), extension->id(),
        base::StringPrintf(kScriptTemplate, GetAPIName(GetParam()),
                           enable_function, tab_id2));
    EXPECT_TRUE(action->GetIsVisible(tab_id2));
    EXPECT_TRUE(action->GetIsVisible(tab_id1));
  }

  // Page actions can't be enabled/disabled globally, but others can. Try
  // toggling global state by omitting the tab id if the type isn't a page
  // action.
  if (GetParam() == ActionInfo::TYPE_PAGE)
    return;

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
        profile(), extension->id(),
        base::StringPrintf(kScriptTemplate, GetAPIName(GetParam()),
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
        profile(), extension->id(),
        base::StringPrintf(kScriptTemplate, GetAPIName(GetParam()),
                           enable_function));
    EXPECT_EQ(true, action->GetIsVisible(tab_id2));
    EXPECT_EQ(true, action->GetIsVisible(tab_id1));
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         MultiActionAPITest,
                         testing::Values(ActionInfo::TYPE_ACTION,
                                         ActionInfo::TYPE_PAGE,
                                         ActionInfo::TYPE_BROWSER));

INSTANTIATE_TEST_SUITE_P(,
                         MultiActionAPICanvasTest,
                         testing::Values(ActionInfo::TYPE_ACTION,
                                         ActionInfo::TYPE_PAGE,
                                         ActionInfo::TYPE_BROWSER));

}  // namespace extensions
