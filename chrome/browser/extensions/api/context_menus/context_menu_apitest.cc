// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "components/version_info/channel.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_host.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/models/menu_model.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/extension_menu_model_android.h"
#else
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class ExtensionContextMenuApiTest : public ExtensionApiTest {
 public:
  ExtensionContextMenuApiTest() = default;
  ~ExtensionContextMenuApiTest() override = default;
  ExtensionContextMenuApiTest(const ExtensionContextMenuApiTest&) = delete;
  ExtensionContextMenuApiTest& operator=(const ExtensionContextMenuApiTest&) =
      delete;
};

IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest, ContextMenus) {
  ASSERT_TRUE(RunExtensionTest("context_menus/event_page")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest, Count) {
  ASSERT_TRUE(RunExtensionTest("context_menus/count")) << message_;
}

// crbug.com/41190960 -- creating context menus from multiple script contexts
// should work.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest,
                       ContextMenusFromMultipleContexts) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("context_menus/add_from_multiple_contexts"))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  {
    // Tell the extension to update the page action state.
    ResultCatcher catcher;
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                              extension->GetResourceURL("popup.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  {
    // Tell the extension to update the page action state again.
    ResultCatcher catcher;
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                              extension->GetResourceURL("popup2.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest, ContextMenusBasics) {
  ASSERT_TRUE(RunExtensionTest("context_menus/basics")) << message_;
}

class ExtensionTabContextMenuApiTest : public ExtensionContextMenuApiTest {
 public:
  ExtensionTabContextMenuApiTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionTabContextMenu);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExtensionTabContextMenuApiTest, ContextMenusTabContext) {
  // Wait for the extension to signal that it has created the menu item.
  ExtensionTestMessageListener listener("created");

  ResultCatcher catcher;

  // Load the extension which calls chrome.contextMenus.create.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("context_menus/tab_context"));
  ASSERT_TRUE(extension);

  // Wait until the extension has completed the menu item creation call.
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Verify the menu model. We create a tab menu model for the active tab
  // to inspect its contents.
#if BUILDFLAG(IS_ANDROID)
  ExtensionMenuModel menu(profile(), GetActiveWebContents());
  menu.PopulateModel();
  EXPECT_TRUE(menu.HasVisibleItems());
  // Calling PopulateModel() again should be idempotent and not duplicate items.
  menu.PopulateModel();
#else
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int index = tab_strip->active_index();
  TabMenuModel menu(nullptr, nullptr, tab_strip, index);
#endif

  // Iterate through the menu items to count how many times the
  // extension-provided item is present. This ensures that exactly one such item
  // has been added, validating against duplicates or missing items.
  int match_count = 0;
  size_t item_index = 0;
  for (size_t i = 0; i < menu.GetItemCount(); ++i) {
    if (menu.GetLabelAt(i) == u"Test Tab Item") {
      match_count++;
      item_index = i;
    }
  }
  ASSERT_EQ(1, match_count);

  EXPECT_TRUE(menu.IsVisibleAt(item_index));
  EXPECT_TRUE(menu.IsEnabledAt(item_index));
  // Tap on the context menu item. This will trigger the background script's
  // onClicked listener.
  menu.ActivatedAt(item_index);

  // Wait for the extension to verify it received the onClicked event, which it
  // indicates via success in the test.
  ASSERT_TRUE(catcher.GetNextResult());
}

#if !BUILDFLAG(IS_ANDROID)
class FakeTabContextMenuControllerDelegate
    : public TabContextMenuController::Delegate {
 public:
  bool IsContextMenuCommandChecked(
      TabStripModel::ContextMenuCommand command_id) override {
    return false;
  }

  bool IsContextMenuCommandEnabled(
      tabs::TabInterface* tab,
      TabStripModel::ContextMenuCommand command_id) override {
    return true;
  }

  bool IsContextMenuCommandAlerted(
      TabStripModel::ContextMenuCommand command_id) override {
    return false;
  }

  void ExecuteContextMenuCommand(tabs::TabInterface* tab,
                                 TabStripModel::ContextMenuCommand command_id,
                                 int event_flags) override {}
  bool GetContextMenuAccelerator(int command_id,
                                 ui::Accelerator* accelerator) override {
    return false;
  }
};
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(ExtensionTabContextMenuApiTest,
                       ContextMenusTabContextMultipleItems) {
  // Wait for the extension to signal that it has created the menu items.
  ExtensionTestMessageListener listener("created");

  ResultCatcher catcher;

  // Load the extension which calls chrome.contextMenus.create multiple times.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("context_menus/tab_context_multiple"));
  ASSERT_TRUE(extension);

  // Wait until the extension has completed the menu item creation calls.
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Verify the menu model. We create a tab menu model for the active tab
  // to inspect its contents.
#if BUILDFLAG(IS_ANDROID)
  ExtensionMenuModel menu(profile(), GetActiveWebContents());
  menu.PopulateModel();
#else
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int index = tab_strip->active_index();
  TabMenuModel menu(nullptr, nullptr, tab_strip, index);
#endif

  // Because there are multiple items, ContextMenuMatcher must group them inside
  // a submenu. Find the index of the extension-named submenu item.
  size_t submenu_index = menu.GetItemCount();
  for (size_t i = 0; i < menu.GetItemCount(); ++i) {
    if (menu.GetLabelAt(i) == base::UTF8ToUTF16(extension->name())) {
      submenu_index = i;
      break;
    }
  }
  ASSERT_LT(submenu_index, menu.GetItemCount());
  ASSERT_EQ(menu.GetTypeAt(submenu_index), ui::MenuModel::TYPE_SUBMENU);

  // Verify that both items are present.
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);
  ASSERT_TRUE(submenu);
  ASSERT_EQ(2u, submenu->GetItemCount());

#if !BUILDFLAG(IS_ANDROID)
  FakeTabContextMenuControllerDelegate fake_delegate;

  // Instantiate the TabContextMenuController to wrap the model, matching the
  // real UI delegate.
  auto context_menu_controller = std::make_unique<TabContextMenuController>(
      tab_strip->GetTabAtIndex(index)->GetHandle(), &fake_delegate);

  auto tab_menu_model = std::make_unique<TabMenuModel>(
      context_menu_controller.get(), nullptr, tab_strip, index);
  TabMenuModel* tab_menu_model_ptr = tab_menu_model.get();
  context_menu_controller->LoadModel(std::move(tab_menu_model),
                                     tab_menu_model_ptr);

  TabMenuModel* loaded_model = context_menu_controller->GetTabMenuModel();
  size_t loaded_submenu_index = loaded_model->GetItemCount();
  for (size_t i = 0; i < loaded_model->GetItemCount(); ++i) {
    if (loaded_model->GetLabelAt(i) == base::UTF8ToUTF16(extension->name())) {
      loaded_submenu_index = i;
      break;
    }
  }
  ASSERT_LT(loaded_submenu_index, loaded_model->GetItemCount());
  ui::MenuModel* loaded_submenu =
      loaded_model->GetSubmenuModelAt(loaded_submenu_index);
  ASSERT_TRUE(loaded_submenu);

  // Verify that querying the submenu item states routes successfully without
  // crashes.
  EXPECT_TRUE(loaded_submenu->IsVisibleAt(0));
  EXPECT_TRUE(loaded_submenu->IsEnabledAt(0));
  EXPECT_FALSE(loaded_submenu->IsItemCheckedAt(0));

  // Simulate user clicking the first submenu item.
  loaded_submenu->ActivatedAt(0);
#else
  // Verify that querying the submenu item states routes successfully without
  // crashes.
  EXPECT_TRUE(submenu->IsVisibleAt(0));
  EXPECT_TRUE(submenu->IsEnabledAt(0));
  EXPECT_FALSE(submenu->IsItemCheckedAt(0));

  // Simulate user clicking the first submenu item.
  submenu->ActivatedAt(0);
#endif

  // Wait for the extension background script to receive the click event and
  // succeed.
  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest, ContextMenusNoPerms) {
  ASSERT_TRUE(RunExtensionTest("context_menus/no_perms")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest, ContextMenusMultipleIds) {
  ASSERT_TRUE(RunExtensionTest("context_menus/item_ids")) << message_;
}

namespace {

// The background script used by the tab scrubbing tests below. Rather than
// asserting on the click data itself, it echoes the `info` and `tab` objects
// it was given back to the browser process, so that the expectations can live
// in the tests themselves. `%s` is the context the menu item is shown in.
constexpr char kScrubbingScriptTemplate[] = R"(
  chrome.contextMenus.onClicked.addListener((info, tab) => {
    chrome.test.sendMessage(JSON.stringify({info: info, tab: tab}));
  });

  chrome.contextMenus.create(
      {id: 'test_item', title: 'Test Item', contexts: ['%s']},
      () => chrome.test.sendMessage('created'));
)";

// Verifies that the sensitive metadata the extension is not allowed to see was
// removed from `tab`.
void ExpectTabScrubbed(const base::DictValue& tab) {
  EXPECT_FALSE(tab.contains("url"));
  EXPECT_FALSE(tab.contains("title"));
  EXPECT_FALSE(tab.contains("favIconUrl"));
  EXPECT_FALSE(tab.contains("pendingUrl"));
  // Non-sensitive properties should still be present. This also guards against
  // an unexpected object trivially satisfying the checks above.
  EXPECT_TRUE(tab.FindInt("id").has_value());
}

// Verifies that `tab` still has the metadata for the page at `url`.
void ExpectTabNotScrubbed(const base::DictValue& tab, const GURL& url) {
  const std::string* tab_url = tab.FindString("url");
  ASSERT_TRUE(tab_url);
  EXPECT_EQ(url.spec(), *tab_url);
  const std::string* title = tab.FindString("title");
  ASSERT_TRUE(title);
  // simple.html has a title of "OK".
  EXPECT_EQ("OK", *title);
  EXPECT_TRUE(tab.FindInt("id").has_value());
}

// Verifies the `pageUrl` the extension was given for the click.
void ExpectPageUrl(const base::DictValue& info, const GURL& url) {
  const std::string* page_url = info.FindString("pageUrl");
  ASSERT_TRUE(page_url);
  EXPECT_EQ(url.spec(), *page_url);
}

}  // namespace

class ContextMenusTabScrubbingTest : public ExtensionContextMenuApiTest {
 public:
  // The context the extension's menu item is shown in.
  enum class MenuContext {
    kPage,
    kAction,
  };

  // The `info` and `tab` objects an extension was given for a menu click.
  struct ClickData {
    base::DictValue info;
    base::DictValue tab;
  };

  void SetUpOnMainThread() override {
    ExtensionContextMenuApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Loads an extension with a single context menu item shown in
  // `menu_context`, granted the contextMenus permission along with any
  // `extra_permissions` and `host_permissions`. Returns nullptr on failure.
  const Extension* LoadScrubbingExtension(
      MenuContext menu_context,
      const std::vector<std::string>& extra_permissions = {},
      const std::vector<std::string>& host_permissions = {}) {
    base::ListValue permissions;
    permissions.Append("contextMenus");
    for (const std::string& permission : extra_permissions) {
      permissions.Append(permission);
    }

    base::DictValue manifest =
        base::DictValue()
            .Set("name", "ContextMenus Tab Scrubbing Test")
            .Set("version", "1")
            .Set("manifest_version", 3)
            .Set("permissions", std::move(permissions))
            .Set("background",
                 base::DictValue().Set("service_worker", "sw.js"));

    if (!host_permissions.empty()) {
      base::ListValue hosts;
      for (const std::string& host : host_permissions) {
        hosts.Append(host);
      }
      manifest.Set("host_permissions", std::move(hosts));
    }

    const char* context_name = nullptr;
    switch (menu_context) {
      case MenuContext::kPage:
        context_name = "page";
        break;
      case MenuContext::kAction:
        // Items shown in the action context require the extension to have an
        // action to show them on.
        manifest.Set("action", base::DictValue());
        context_name = "action";
        break;
    }

    test_dir_.WriteManifest(manifest);
    test_dir_.WriteFile(
        FILE_PATH_LITERAL("sw.js"),
        base::StringPrintf(kScrubbingScriptTemplate, context_name));

    // Wait for the item to be created, otherwise it won't be in the menu when
    // one of the Click*ContextMenuItem() methods shows it.
    ExtensionTestMessageListener created_listener("created");
    const Extension* extension = LoadExtension(test_dir_.UnpackedPath());
    if (!extension || !created_listener.WaitUntilSatisfied()) {
      return nullptr;
    }
    return extension;
  }

  GURL GetTestUrl(std::string_view host) {
    return embedded_test_server()->GetURL(host, "/simple.html");
  }

  // Shows a page context menu on the active tab and activates the extension's
  // item in it, returning the data the extension received for the click.
  std::optional<ClickData> ClickPageContextMenuItem() {
    content::RenderFrameHost* frame =
        GetActiveWebContents()->GetPrimaryMainFrame();
    content::ContextMenuParams params;
    params.page_url = frame->GetLastCommittedURL();

    const int command_id =
        ContextMenuMatcher::ConvertToExtensionsCustomCommandId(/*id=*/0);

    ExtensionTestMessageListener click_listener;
#if BUILDFLAG(IS_ANDROID)
    ExtensionMenuModel menu(*frame, params);
    menu.PopulateModel();
#else
    TestRenderViewContextMenu menu(*frame, params);
    menu.Init();
#endif
    EXPECT_TRUE(menu.IsCommandIdVisible(command_id));
    EXPECT_TRUE(menu.IsCommandIdEnabled(command_id));
    menu.ExecuteCommand(command_id, /*event_flags=*/0);

    return WaitForClickData(click_listener);
  }

  // Shows the context menu for the extension's toolbar action and activates
  // the extension's item in it, returning the data the extension received for
  // the click.
  std::optional<ClickData> ClickActionContextMenuItem(
      const Extension& extension) {
    const int command_id =
        ContextMenuMatcher::ConvertToExtensionsCustomCommandId(/*id=*/0);
    ExtensionContextMenuModel menu(
        &extension, browser_window_interface(),
        /*is_pinned=*/true, /*delegate=*/nullptr,
        /*can_show_icon_in_toolbar=*/true,
        ExtensionContextMenuModel::ContextMenuSource::kToolbarAction);
    EXPECT_TRUE(menu.GetIndexOfCommandId(command_id).has_value());

    ExtensionTestMessageListener click_listener;
    menu.ExecuteCommand(command_id, /*event_flags=*/0);

    return WaitForClickData(click_listener);
  }

 private:
  // Waits for the extension to echo back the data for a menu item click.
  std::optional<ClickData> WaitForClickData(
      ExtensionTestMessageListener& listener) {
    if (!listener.WaitUntilSatisfied()) {
      ADD_FAILURE() << "Never received a click from the extension.";
      return std::nullopt;
    }

    std::optional<base::DictValue> click_data =
        base::JSONReader::ReadDict(listener.message(), base::JSON_PARSE_RFC);
    base::DictValue* info = click_data ? click_data->FindDict("info") : nullptr;
    base::DictValue* tab = click_data ? click_data->FindDict("tab") : nullptr;
    if (!info || !tab) {
      ADD_FAILURE() << "Unexpected click data: " << listener.message();
      return std::nullopt;
    }
    return ClickData{std::move(*info), std::move(*tab)};
  }

  // The files for the extension loaded by LoadScrubbingExtension(). Owned by
  // the fixture so that they outlive the loaded extension.
  TestExtensionDir test_dir_;
};

// An extension without tab or host permissions should have sensitive tab
// metadata scrubbed when its menu item is invoked from the toolbar action.
IN_PROC_BROWSER_TEST_F(ContextMenusTabScrubbingTest,
                       ActionContextMenuWithoutPermissionsScrubsTab) {
  const Extension* extension = LoadScrubbingExtension(MenuContext::kAction);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(), GetTestUrl("example.test")));

  std::optional<ClickData> click_data = ClickActionContextMenuItem(*extension);
  ASSERT_TRUE(click_data);
  // `pageUrl` is only provided for items invoked on a page.
  EXPECT_FALSE(click_data->info.contains("pageUrl"));
  ExpectTabScrubbed(click_data->tab);
}

// An extension with the activeTab permission is granted access to the tab when
// its menu item is invoked, so the tab should not be scrubbed.
IN_PROC_BROWSER_TEST_F(ContextMenusTabScrubbingTest,
                       ActionContextMenuWithActiveTabDoesNotScrubTab) {
  const Extension* extension =
      LoadScrubbingExtension(MenuContext::kAction, {"activeTab"});
  ASSERT_TRUE(extension);
  const GURL url = GetTestUrl("example.test");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

  std::optional<ClickData> click_data = ClickActionContextMenuItem(*extension);
  ASSERT_TRUE(click_data);
  EXPECT_FALSE(click_data->info.contains("pageUrl"));
  ExpectTabNotScrubbed(click_data->tab, url);
}

// An extension without tab or host permissions should have sensitive tab
// metadata scrubbed when its menu item is invoked from a page context menu,
// though it is still told which page the click happened on.
IN_PROC_BROWSER_TEST_F(ContextMenusTabScrubbingTest,
                       PageContextMenuWithoutPermissionsScrubsTab) {
  ASSERT_TRUE(LoadScrubbingExtension(MenuContext::kPage));
  const GURL url = GetTestUrl("example.test");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

  std::optional<ClickData> click_data = ClickPageContextMenuItem();
  ASSERT_TRUE(click_data);
  ExpectPageUrl(click_data->info, url);
  ExpectTabScrubbed(click_data->tab);
}

// An extension with the activeTab permission is granted access to the tab when
// its menu item is invoked, so the tab should not be scrubbed.
IN_PROC_BROWSER_TEST_F(ContextMenusTabScrubbingTest,
                       PageContextMenuWithActiveTabDoesNotScrubTab) {
  ASSERT_TRUE(LoadScrubbingExtension(MenuContext::kPage, {"activeTab"}));
  const GURL url = GetTestUrl("example.test");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

  std::optional<ClickData> click_data = ClickPageContextMenuItem();
  ASSERT_TRUE(click_data);
  ExpectPageUrl(click_data->info, url);
  ExpectTabNotScrubbed(click_data->tab, url);
}

// An extension with the tabs permission can see tab metadata for any tab, so
// the tab should not be scrubbed.
IN_PROC_BROWSER_TEST_F(ContextMenusTabScrubbingTest,
                       PageContextMenuWithTabsPermissionDoesNotScrubTab) {
  ASSERT_TRUE(LoadScrubbingExtension(MenuContext::kPage, {"tabs"}));
  const GURL url = GetTestUrl("example.test");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

  std::optional<ClickData> click_data = ClickPageContextMenuItem();
  ASSERT_TRUE(click_data);
  ExpectPageUrl(click_data->info, url);
  ExpectTabNotScrubbed(click_data->tab, url);
}

// An extension with explicit host permissions should only see tab metadata for
// origins it has access to.
IN_PROC_BROWSER_TEST_F(ContextMenusTabScrubbingTest,
                       PageContextMenuWithHostPermissionsScrubsPerOrigin) {
  ASSERT_TRUE(LoadScrubbingExtension(MenuContext::kPage,
                                     /*extra_permissions=*/{},
                                     {"*://example.test/*"}));

  {
    SCOPED_TRACE("Invoked on an origin the extension cannot access.");
    const GURL url = GetTestUrl("other.test");
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

    std::optional<ClickData> click_data = ClickPageContextMenuItem();
    ASSERT_TRUE(click_data);
    ExpectPageUrl(click_data->info, url);
    ExpectTabScrubbed(click_data->tab);
  }

  {
    SCOPED_TRACE("Invoked on an origin the extension can access.");
    const GURL url = GetTestUrl("example.test");
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

    std::optional<ClickData> click_data = ClickPageContextMenuItem();
    ASSERT_TRUE(click_data);
    ExpectPageUrl(click_data->info, url);
    ExpectTabNotScrubbed(click_data->tab, url);
  }
}

class ExtensionContextMenuVisibilityApiTest
    : public ExtensionContextMenuApiTest {
 public:
  ExtensionContextMenuVisibilityApiTest() = default;

  ExtensionContextMenuVisibilityApiTest(
      const ExtensionContextMenuVisibilityApiTest&) = delete;
  ExtensionContextMenuVisibilityApiTest& operator=(
      const ExtensionContextMenuVisibilityApiTest&) = delete;

  void TearDownOnMainThread() override {
    // Depends on `menu_` so must be cleared before it is destroyed.
    top_level_model_ = nullptr;
#if BUILDFLAG(IS_ANDROID)
    extension_menu_model_.reset();
#else
    menu_.reset();
#endif
    extension_ = nullptr;
    ExtensionContextMenuApiTest::TearDownOnMainThread();
  }

  void SetUpTestExtension() {
    extension_ = LoadExtension(
        test_data_dir_.AppendASCII("context_menus/item_visibility/"));
  }

  // Sets up the top-level model that is passed to UI code to be displayed. On
  // Android these are only extensions-related items, whereas on Win/Mac/Linux
  // this includes general context menu items as well.
  bool SetupTopLevelMenuModel() {
    content::RenderFrameHost* frame =
        GetActiveWebContents()->GetPrimaryMainFrame();
    content::ContextMenuParams params;
    params.page_url = frame->GetLastCommittedURL();

#if BUILDFLAG(IS_ANDROID)
    extension_menu_model_ =
        std::make_unique<ExtensionMenuModel>(*frame, params);
    extension_menu_model_->PopulateModel();
    top_level_model_ = extension_menu_model_.get();
    top_level_index_ = 0;
    bool valid_setup = true;
#else
    // Create context menu.
    menu_ = std::make_unique<TestRenderViewContextMenu>(*frame, params);
    menu_->Init();

    // Get menu model.
    std::optional<std::pair<ui::MenuModel*, size_t>> model_and_index =
        menu_->GetMenuModelAndItemIndex(
            menu_->extension_items().ConvertToExtensionsCustomCommandId(0));
    CHECK(model_and_index);
    top_level_model_ = model_and_index->first;
    top_level_index_ = model_and_index->second;
    EXPECT_TRUE(top_level_model_);
    EXPECT_GT(top_level_index(), 0u);
    // TODO: Eliminate this variable.
    bool valid_setup = true;
#endif  // BUILDFLAG(IS_ANDROID)

    return valid_setup;
  }

  void CallAPI(const std::string& script) { CallAPI(extension_, script); }

  void CallAPI(const Extension* extension, const std::string& script) {
    std::string expr = script;
    if (!expr.empty() && expr.back() == ';') {
      expr.pop_back();
    }
    std::string wrapped_script = expr + ".then(chrome.test.sendScriptResult);";
    ASSERT_EQ(base::Value(false),
              BackgroundScriptExecutor::ExecuteScript(
                  profile(), extension->id(), wrapped_script,
                  BackgroundScriptExecutor::ResultCapture::kSendScriptResult));
  }

  // Verifies that the UI menu model has the given number of extension menu
  // items, |num_items|, of a menu model |type|.
  void VerifyNumExtensionItemsInMenuModel(int num_items,
                                          ui::MenuModel::ItemType type) {
    int num_found = 0;
    for (size_t i = 0; i < top_level_model_->GetItemCount(); ++i) {
      int command_id = top_level_model_->GetCommandIdAt(i);
      if (ContextMenuMatcher::IsExtensionsCustomCommandId(command_id) &&
          top_level_model_->GetTypeAt(i) == type) {
        ++num_found;
      }
    }
    ASSERT_EQ(num_found, num_items);
  }

  // Verifies that the context menu is valid and contains the given number of
  // menu items, |num_items|. Note that this includes items manually added by
  // extensions, but not the automatically added extension name, if present.
  void VerifyNumContextMenuItems(size_t num_items) {
#if BUILDFLAG(IS_ANDROID)
    ASSERT_TRUE(extension_menu_model_);
    size_t items_in_menu =
        extension_menu_model_->matcher_for_test().extension_item_map().size();
    EXPECT_EQ(num_items, items_in_menu);
#else
    ASSERT_TRUE(menu_);
    EXPECT_EQ(num_items,
              (menu_->extension_items().extension_item_map().size()));
#endif  // BUILDFLAG(IS_ANDROID)
  }

  // Verifies a context menu item's visibility, title, and item type.
  void VerifyMenuItem(const std::string& title,
                      ui::MenuModel* model,
                      size_t index,
                      ui::MenuModel::ItemType type,
                      bool visible) {
    EXPECT_EQ(base::ASCIIToUTF16(title), model->GetLabelAt(index));
    ASSERT_EQ(type, model->GetTypeAt(index));
    EXPECT_EQ(visible, model->IsVisibleAt(index));
  }

  size_t top_level_index() const { return top_level_index_; }

  const Extension* extension() { return extension_; }

  raw_ptr<ui::MenuModel> top_level_model_ = nullptr;

 private:
  raw_ptr<const Extension> extension_ = nullptr;
#if BUILDFLAG(IS_ANDROID)
  // Contains only the extension menu items.
  std::unique_ptr<ExtensionMenuModel> extension_menu_model_;
#else
  // Contains Chrome context menu items and extension menu items.
  std::unique_ptr<TestRenderViewContextMenu> menu_;
#endif
  // Where the extension items start in the menu. Always 0 on Android because
  // the menu only contains extension items.
  size_t top_level_index_ = 0;
};

// Tests showing a single visible menu item in the top-level menu model, which
// includes actions like "Back", "View Page Source", "Inspect", etc.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuVisibilityApiTest,
                       ShowOneTopLevelItem) {
  SetUpTestExtension();
  CallAPI("create({title: 'item', visible: true});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(1);

  VerifyMenuItem("item", top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_COMMAND, true);

  // There should be no submenu model.
  EXPECT_FALSE(top_level_model_->GetSubmenuModelAt(top_level_index()));
}

// Tests hiding a menu item in the top-level menu model, which includes actions
// like "Back", "View Page Source", "Inspect", etc.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuVisibilityApiTest,
                       HideTopLevelItem) {
  SetUpTestExtension();
  CallAPI("create({id: 'item1', title: 'item', visible: true});");
  CallAPI("update('item1', {visible: false});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(1);

  VerifyMenuItem("item", top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_COMMAND, false);

  // There should be no submenu model.
  EXPECT_FALSE(top_level_model_->GetSubmenuModelAt(top_level_index()));
}

// Tests showing a single top-level parent menu item, when it is visible, but
// all of its child items are hidden. The child items' hidden states are tested
// too. Recall that a top-level item can be either a parent item specified by
// the developer or parent item labeled with the extension's name. In this case,
// we test the former.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuVisibilityApiTest,
                       ShowTopLevelItemIfAllItsChildrenAreHidden) {
  SetUpTestExtension();
  CallAPI("create({id: 'id', title: 'parent', visible: true});");
  CallAPI("create({title: 'child1', parentId: 'id', visible: false});");
  CallAPI("create({title: 'child2', parentId: 'id', visible: false});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(3);

  VerifyMenuItem("parent", top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, true);

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(2u, submodel->GetItemCount());

  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("child2", submodel, 1, ui::MenuModel::TYPE_COMMAND, false);
}

// Tests showing a top-level parent menu item as a submenu, when some of its
// child items are visibile. The child items' visibilities are tested too.
// Recall that a top-level item can be either a parent item specified by the
// developer or parent item labeled with the extension's name. In this case, we
// test the former.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuVisibilityApiTest,
                       ShowTopLevelSubmenuItemIfSomeOfChildrenAreVisible) {
  SetUpTestExtension();
  CallAPI("create({id: 'id', title: 'parent', visible: true});");
  CallAPI("create({title: 'child1', parentId: 'id', visible: true});");
  CallAPI("create({title: 'child2', parentId: 'id', visible: false});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(3);

  VerifyMenuItem("parent", top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, true);

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(2u, submodel->GetItemCount());

  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_COMMAND, true);
  VerifyMenuItem("child2", submodel, 1, ui::MenuModel::TYPE_COMMAND, false);
}

// Tests showing a single top-level parent menu item, when it is visible and has
// a visible submenu, but submenu has child items where all of submenu's child
// items are hidden. Recall that a top-level item can be either a parent item
// specified by the developer or parent item labeled with the extension's name.
// In this case, we test the former.
IN_PROC_BROWSER_TEST_F(
    ExtensionContextMenuVisibilityApiTest,
    ShowTopLevelItemWithASubmenuWhereAllSubmenusChildrenAreHidden) {
  SetUpTestExtension();

  CallAPI("create({id: 'parent', title: 'parent', visible: true});");
  CallAPI(
      "create({id: 'child1', title: 'child1', parentId: 'parent', visible: "
      "true});");
  CallAPI("create({title: 'child2', parentId: 'child1', visible: false});");
  CallAPI("create({title: 'child3', parentId: 'child1', visible: false});");

  ASSERT_TRUE(SetupTopLevelMenuModel());
  VerifyNumContextMenuItems(4);

  VerifyMenuItem("parent", top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, true);

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(1u, submodel->GetItemCount());

  // When a parent item is specified by the developer (as opposed to generated),
  // its visibility is determined by the specified state.
  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_SUBMENU, true);

  submodel = submodel->GetSubmenuModelAt(0);
  ASSERT_TRUE(submodel);
  EXPECT_EQ(2u, submodel->GetItemCount());

  VerifyMenuItem("child2", submodel, 0, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("child3", submodel, 1, ui::MenuModel::TYPE_COMMAND, false);
}

// Tests hiding a top-level parent menu item, when all of its child items are
// hidden. Recall that a top-level item can be either a parent item specified by
// the developer or parent item labeled with the extension's name. In this case,
// we test the latter. This extension-named top-level item should be hidden,
// when all of its child items are hidden.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuVisibilityApiTest,
                       HideExtensionNamedTopLevelItemIfAllChildrenAreHidden) {
  SetUpTestExtension();
  CallAPI("create({title: 'item1', visible: false});");
  CallAPI("create({title: 'item2', visible: false});");
  CallAPI("create({title: 'item3', visible: false});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(3);

  VerifyMenuItem(extension()->name(), top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, false);

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(3u, submodel->GetItemCount());

  VerifyMenuItem("item1", submodel, 0, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("item2", submodel, 1, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("item3", submodel, 2, ui::MenuModel::TYPE_COMMAND, false);
}

// Tests updating a top-level parent menu item, when the submenu item is not
// visible first and is then updated to visible. Recall that a top-level item
// can be either a parent item specified by the developer or parent item labeled
// with the extension's name. In this case, we test the former.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuVisibilityApiTest,
                       UpdateTopLevelItem) {
  SetUpTestExtension();

  CallAPI("create({id: 'parent', title: 'parent', visible: true});");
  CallAPI(
      "create({id: 'child1', title: 'child1', parentId: 'parent', visible: "
      "false});");

  // Verify that the child item is hidden.
  ASSERT_TRUE(SetupTopLevelMenuModel());
  VerifyNumContextMenuItems(2);
  VerifyMenuItem("parent", top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, true);

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(1u, submodel->GetItemCount());
  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_COMMAND, false);

  // Update child1 to visible.
  CallAPI("update('child1', {visible: true});");

  // Verify that the child item is visible.
  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_COMMAND, true);
}

// Tests updating a top-level parent menu item, when the menu item is not
// visible first and is then updated to visible. Recall that a top-level item
// can be either a parent item specified by the developer or parent item labeled
// with the extension's name. In this case, we test the latter.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuVisibilityApiTest,
                       UpdateExtensionNamedTopLevelItem) {
  SetUpTestExtension();
  CallAPI("create({id: 'item1', title: 'item1', visible: false});");
  CallAPI("update('item1', {visible: true});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(1);
  VerifyMenuItem("item1", top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_COMMAND, true);
}

// Tests showing a top-level parent menu item, when some of its child items are
// visible. The child items' visibilities are tested as well. Recall that a
// top-level item can be either a parent item specified by the developer or
// parent item labeled with the extension's name. In this case, we test the
// latter.
//
// Also, this tests that hiding a parent item should hide its children even if
// they are set as visible.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuVisibilityApiTest,
                       ShowExtensionNamedTopLevelItemIfSomeChildrenAreVisible) {
  SetUpTestExtension();
  CallAPI("create({title: 'item1'});");
  CallAPI("create({title: 'item2'});");
  CallAPI("create({title: 'item3', id: 'item3', visible: false});");
  CallAPI("create({title: 'child1', visible: true, parentId: 'item3'});");
  CallAPI("create({title: 'child2', visible: true, parentId: 'item3'});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(5);

  VerifyMenuItem(extension()->name(), top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, true);

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(3u, submodel->GetItemCount());

  VerifyMenuItem("item1", submodel, 0, ui::MenuModel::TYPE_COMMAND, true);
  VerifyMenuItem("item2", submodel, 1, ui::MenuModel::TYPE_COMMAND, true);
  VerifyMenuItem("item3", submodel, 2, ui::MenuModel::TYPE_SUBMENU, false);

  ui::MenuModel* item3_submodel = submodel->GetSubmenuModelAt(2);
  ASSERT_TRUE(item3_submodel);
  EXPECT_EQ(2u, item3_submodel->GetItemCount());

  // Though the children's internal visibility state remains unchanged, the ui
  // code will hide the children if the parent is hidden.
  VerifyMenuItem("child1", item3_submodel, 0, ui::MenuModel::TYPE_COMMAND,
                 true);
  VerifyMenuItem("child2", item3_submodel, 1, ui::MenuModel::TYPE_COMMAND,
                 true);
}

// Tests that more than one extension named top-level parent menu item can be
// displayed in the context menu.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuVisibilityApiTest,
                       ShowMultipleExtensionNamedTopLevelItemsWithChidlren) {
  const Extension* e1 =
      LoadExtension(test_data_dir_.AppendASCII("context_menus/simple/one"));
  const Extension* e2 =
      LoadExtension(test_data_dir_.AppendASCII("context_menus/simple/two"));

  CallAPI(e1, "create({title: 'item1'});");
  CallAPI(e1, "create({title: 'item2'});");
  CallAPI(e2, "create({title: 'item1'});");
  CallAPI(e2, "create({title: 'item2'});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumExtensionItemsInMenuModel(2, ui::MenuModel::TYPE_SUBMENU);

  // The UI menu model organizes extension menu items alphabetically by
  // extension name, regardless of installation order. For example, if an
  // extension named "aaa" was installed after extension "bbb", then extension
  // "aaa" item would precede "bbb" in the context menu.
  VerifyMenuItem(e1->name(), top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, true);
  VerifyMenuItem(e2->name(), top_level_model_, top_level_index() + 1,
                 ui::MenuModel::TYPE_SUBMENU, true);
}

class ExtensionContextMenuVisibilityApiMenuSimplificationTest
    : public ExtensionContextMenuVisibilityApiTest,
      public testing::WithParamInterface<bool> {
 public:
  ExtensionContextMenuVisibilityApiMenuSimplificationTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(features::kMenuSimplification);
    } else {
      feature_list_.InitAndDisableFeature(features::kMenuSimplification);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ExtensionContextMenuVisibilityApiMenuSimplificationTest,
    testing::Bool());

// Tests hiding a parent menu item, when it is hidden and some of its children
// are visible.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuVisibilityApiMenuSimplificationTest,
                       HideTopLevelSubmenuItemIfHiddenAndSomeChildrenVisible) {
  SetUpTestExtension();
  CallAPI("create({id: 'id', title: 'parent', visible: false});");
  CallAPI("create({title: 'child1', parentId: 'id', visible: false});");
  CallAPI("create({title: 'child2', parentId: 'id', visible: true});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(3);

  VerifyMenuItem("parent", top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, false);

#if !BUILDFLAG(IS_ANDROID)
  // Since the extension submenu is hidden, the previous separator should not be
  // in the model. On Android top_level_index() is 0 so we don't test this.
  // With kMenuSimplification enabled, there is a separator from the end of the
  // page items group before the hidden extension submenu item.
  if (!GetParam()) {
    EXPECT_NE(ui::MenuModel::TYPE_SEPARATOR,
              top_level_model_->GetTypeAt(top_level_index() - 1));
  }
#endif

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(2u, submodel->GetItemCount());

  // Though the children's internal visibility state remains unchanged, the ui
  // code will hide the children if the parent is hidden.
  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("child2", submodel, 1, ui::MenuModel::TYPE_COMMAND, true);
}

// Tests hiding a parent menu item, when it is hidden and so are all of its
// children.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuVisibilityApiMenuSimplificationTest,
                       HideTopLevelSubmenuItemIfHiddenAndChildrenHidden) {
  SetUpTestExtension();
  CallAPI("create({id: 'id', title: 'parent', visible: false});");
  CallAPI("create({title: 'child1', parentId: 'id', visible: false});");
  CallAPI("create({title: 'child2', parentId: 'id', visible: false});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(3);

  VerifyMenuItem("parent", top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, false);

#if !BUILDFLAG(IS_ANDROID)
  // Since the extension submenu is hidden, the previous separator should not be
  // in the model. On Android top_level_index() is 0 so we don't test this.
  if (!GetParam()) {
    EXPECT_NE(ui::MenuModel::TYPE_SEPARATOR,
              top_level_model_->GetTypeAt(top_level_index() - 1));
  }
#endif

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(2u, submodel->GetItemCount());

  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("child2", submodel, 1, ui::MenuModel::TYPE_COMMAND, false);
}

}  // namespace extensions
