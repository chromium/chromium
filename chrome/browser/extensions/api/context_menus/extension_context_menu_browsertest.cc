// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <set>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/menu_manager_test_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/state_store_test_observer.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/models/menu_model.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/wm/window_pin_util.h"
#endif

using content::WebContents;
using extensions::ContextMenuMatcher;
using ContextType = extensions::ExtensionBrowserTest::ContextType;
using extensions::MenuItem;
using extensions::ResultCatcher;
using ui::MenuModel;

namespace {

constexpr char kPersistentExtensionId[] = "knldjmfmopnpolahpmmgbagdohdnhkik";

}  // namespace

class ExtensionContextMenuBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  explicit ExtensionContextMenuBrowserTest(
      ContextType context_type = ContextType::kNone)
      : ExtensionBrowserTest(context_type) {}
  ~ExtensionContextMenuBrowserTest() override = default;
  ExtensionContextMenuBrowserTest(const ExtensionContextMenuBrowserTest&) =
      delete;
  ExtensionContextMenuBrowserTest& operator=(
      const ExtensionContextMenuBrowserTest&) = delete;

  // Returns the active WebContents.
  WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Shortcut to return the current MenuManager.
  extensions::MenuManager* menu_manager() {
    return extensions::MenuManager::Get(browser()->profile());
  }

  // Returns a pointer to the currently loaded extension with |name|, or null
  // if not found.
  const extensions::Extension* GetExtensionNamed(const std::string& name) {
    const extensions::ExtensionSet& extensions =
        extensions::ExtensionRegistry::Get(browser()->profile())
            ->enabled_extensions();
    for (const auto& ext : extensions) {
      if (ext->name() == name)
        return ext.get();
    }
    return nullptr;
  }

  // This gets all the items that any extension has registered for possible
  // inclusion in context menus.
  MenuItem::List GetItems() {
    MenuItem::List result;
    std::set<MenuItem::ExtensionKey> extension_ids =
        menu_manager()->ExtensionIds();
    for (const auto& extension_id : extension_ids) {
      const MenuItem::OwnedList* list = menu_manager()->MenuItems(extension_id);
      for (const auto& item : *list)
        result.push_back(item.get());
    }
    return result;
  }

  // This creates a test menu for a page with |page_url| and |link_url|, looks
  // for an extension item with the given |label|, and returns true if the item
  // was found.
  bool MenuHasItemWithLabel(const GURL& frame_url,
                            const GURL& link_url,
                            bool is_subframe,
                            const std::string& label) {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        TestRenderViewContextMenu::Create(GetWebContents(), frame_url, link_url,
                                          is_subframe));
    return MenuHasExtensionItemWithLabel(menu.get(), label);
  }

  // Click on a context menu identified by |target_menu_item_id|, and returns
  // the result of chrome.test.sendMessage. The .js test file that sets up the
  // context menu should call chrome.test.sendMessage in its onclick event.
  std::string ClickMenuInFrame(content::RenderFrameHost* frame,
                               const std::string& target_menu_item_id) {
    content::ContextMenuParams params;
    if (frame->GetParent())
      params.frame_url = frame->GetLastCommittedURL();
    else
      params.page_url = frame->GetLastCommittedURL();

    TestRenderViewContextMenu menu(*frame, params);
    menu.Init();

    MenuItem::Id menu_item_id;
    menu_item_id.string_uid = target_menu_item_id;
    int command_id = -1;
    if (!FindCommandId(&menu, menu_item_id, &command_id))
      return "Menu item not found: " + target_menu_item_id;

    ExtensionTestMessageListener listener;
    menu.ExecuteCommand(command_id, 0);
    if (!listener.WaitUntilSatisfied())
      return "Onclick never fired for menu item: " + target_menu_item_id;

    return listener.message();
  }

  bool MenuHasExtensionItemWithLabel(TestRenderViewContextMenu* menu,
                                     const std::string& label) {
    std::u16string label16 = base::UTF8ToUTF16(label);
    for (const auto& it : menu->extension_items().extension_item_map_) {
      const MenuItem::Id& id = it.second;
      std::u16string tmp_label;
      EXPECT_TRUE(GetItemLabel(menu, id, &tmp_label));
      if (tmp_label == label16)
        return true;
    }
    return false;
  }

  // Looks in the menu for an extension item with |id|, and if it is found and
  // has a label, that is put in |result| and we return true. Otherwise returns
  // false.
  bool GetItemLabel(TestRenderViewContextMenu* menu,
                    const MenuItem::Id& id,
                    std::u16string* result) const {
    int command_id = 0;
    if (!FindCommandId(menu, id, &command_id))
      return false;

    raw_ptr<MenuModel> model = nullptr;
    size_t index = 0;
    if (!menu->GetMenuModelAndItemIndex(command_id, &model, &index)) {
      return false;
    }
    *result = model->GetLabelAt(index);
    return true;
  }

  // Given an extension menu item id, tries to find the corresponding command id
  // in the menu.
  bool FindCommandId(TestRenderViewContextMenu* menu,
                     const MenuItem::Id& id,
                     int* command_id) const {
    for (const auto& it : menu->extension_items().extension_item_map_) {
      if (it.second == id) {
        *command_id = it.first;
        return true;
      }
    }
    return false;
  }

  // Given a menu item id, executes the item's command.
  void ExecuteCommand(TestRenderViewContextMenu* menu,
                      const extensions::ExtensionId& extension_id,
                      const std::string& item_uid) {
    MenuItem::Id id(false, MenuItem::ExtensionKey(extension_id));
    id.string_uid = item_uid;
    int command_id = -1;
    ASSERT_TRUE(FindCommandId(menu, id, &command_id));
    menu->ExecuteCommand(command_id, 0);
  }

  // Verifies a radio item's selection state (checked or unchecked).
  void VerifyRadioItemSelectionState(
      TestRenderViewContextMenu* menu,
      const extensions::ExtensionId& extension_id,
      const std::string& radio_uid,
      bool should_be_checked) {
    MenuItem::Id id(false, MenuItem::ExtensionKey(extension_id));
    id.string_uid = radio_uid;
    int command_id = -1;
    ASSERT_TRUE(FindCommandId(menu, id, &command_id));
    EXPECT_EQ(should_be_checked, menu->IsCommandIdChecked(command_id));
  }

  base::FilePath GetRootDir() const {
    return test_data_dir_.AppendASCII("context_menus");
  }
};

class ExtensionContextMenuLazyTest
    : public ExtensionContextMenuBrowserTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionContextMenuLazyTest()
      : ExtensionContextMenuBrowserTest(GetParam()) {}
  ~ExtensionContextMenuLazyTest() override = default;
  ExtensionContextMenuLazyTest(const ExtensionContextMenuLazyTest&) = delete;
  ExtensionContextMenuLazyTest& operator=(const ExtensionContextMenuLazyTest&) =
      delete;

  void SetUpOnMainThread() override {
    ExtensionContextMenuBrowserTest::SetUpOnMainThread();
    // Set shorter delays to prevent test timeouts.
    extensions::ProcessManager::SetEventPageIdleTimeForTesting(1);
    extensions::ProcessManager::SetEventPageSuspendingTimeForTesting(100);
  }

 protected:
  const extensions::Extension* LoadContextMenuExtension(
      std::string_view subdirectory) {
    base::FilePath extension_dir = GetRootDir().AppendASCII(subdirectory);
    return LoadExtension(extension_dir);
  }

  // Helper to load an extension from context_menus/top_level/|subdirectory| in
  // the extensions test data dir.
  const extensions::Extension* LoadTopLevelContextMenuExtension(
      std::string_view subdirectory) {
    base::FilePath extension_dir =
        GetRootDir().AppendASCII("top_level").AppendASCII(subdirectory);
    return LoadExtension(extension_dir, {});
  }

  const extensions::Extension* LoadContextMenuExtensionWithIncognitoFlags(
      std::string_view subdirectory) {
    base::FilePath extension_dir = GetRootDir().AppendASCII(subdirectory);
    return LoadExtension(extension_dir, {.allow_in_incognito = true});
  }

  // This creates an extension that starts |enabled| and then switches to
  // |!enabled|.
  void TestEnabledContextMenu(bool enabled) {
    ExtensionTestMessageListener begin("begin", ReplyBehavior::kWillReply);
    ExtensionTestMessageListener create("create", ReplyBehavior::kWillReply);
    ExtensionTestMessageListener update("update");
    ASSERT_TRUE(LoadContextMenuExtension("enabled"));

    ASSERT_TRUE(begin.WaitUntilSatisfied());

    if (enabled)
      begin.Reply("start enabled");
    else
      begin.Reply("start disabled");

    // Wait for the extension to tell us it's created an item.
    ASSERT_TRUE(create.WaitUntilSatisfied());

    GURL page_url("http://www.google.com");

    // Create and build our test context menu.
    std::unique_ptr<TestRenderViewContextMenu> menu(
        TestRenderViewContextMenu::Create(GetWebContents(), page_url));

    // Look for the extension item in the menu, and make sure it's |enabled|.
    int command_id = ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
    ASSERT_EQ(enabled, menu->IsCommandIdEnabled(command_id));
    create.Reply("go");

    // Update the item and make sure it is now |!enabled|.
    ASSERT_TRUE(update.WaitUntilSatisfied());
    ASSERT_EQ(!enabled, menu->IsCommandIdEnabled(command_id));
  }
};

class ExtensionContextMenuPersistentTest
    : public ExtensionContextMenuBrowserTest {
 public:
  // Helper to load an extension from context_menus/|subdirectory| in the
  // extensions test data dir.
  const extensions::Extension* LoadContextMenuExtension(
      std::string_view subdirectory) {
    base::FilePath extension_dir = GetRootDir().AppendASCII(subdirectory);
    return LoadExtension(extension_dir);
  }
};

// Tests adding a simple context menu item.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, Simple) {
  ExtensionTestMessageListener listener1("created item");
  ExtensionTestMessageListener listener2("onclick fired");
  ASSERT_TRUE(LoadContextMenuExtension("simple"));

  // Wait for the extension to tell us it's created an item.
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), page_url));

  // Look for the extension item in the menu, and execute it.
  int command_id = ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->IsCommandIdEnabled(command_id));
  menu->ExecuteCommand(command_id, 0);

  // Wait for the extension's script to tell us its onclick fired.
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

// Tests that context menus for event page and Service Worker-based
// extensions are stored properly.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, PRE_Persistent) {
  extensions::StateStoreTestObserver observer(profile());
  ResultCatcher catcher;
  const extensions::Extension* extension =
      LoadContextMenuExtension("persistent");
  ASSERT_TRUE(extension);

  // Wait for the extension to tell us it's been installed and the
  // context menu has been created.
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Wait for the context menu to be stored.
  observer.WaitForExtensionAndKey(extension->id(), "context_menus");
}

IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, Persistent) {
  extensions::MenuManagerTestObserver observer(menu_manager());
  ResultCatcher catcher;

  // Wait for the context menu to finish loading.
  observer.WaitForExtension(kPersistentExtensionId);

  // Open a tab to trigger the update.
  ASSERT_TRUE(
      extensions::browsertest_util::AddTab(browser(), GURL("chrome:version")));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that previous onclick is not fired after updating the menu's onclick,
// and whether setting onclick to null removes the handler.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuPersistentTest, UpdateOnclick) {
  ExtensionTestMessageListener listener_error1("onclick1-unexpected");
  ExtensionTestMessageListener listener_error2("onclick2-unexpected");
  ExtensionTestMessageListener listener_update1("update1",
                                                ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_update2("update2");
  ExtensionTestMessageListener listener_done("onclick2");

  const extensions::Extension* extension =
      LoadContextMenuExtension("onclick_null");
  ASSERT_TRUE(extension);

  // Wait till item has been created and updated.
  ASSERT_TRUE(listener_update1.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), page_url));

  // Look for the extension item in the menu, and execute it.
  MenuItem::Id id(false, MenuItem::ExtensionKey(extension->id()));
  id.string_uid = "id1";
  int command_id = -1;
  ASSERT_TRUE(FindCommandId(menu.get(), id, &command_id));
  menu->ExecuteCommand(command_id, 0);

  // Let the test proceed.
  listener_update1.Reply("");

  // Wait until the second context menu has been set up.
  ASSERT_TRUE(listener_update2.WaitUntilSatisfied());

  // Rebuild the context menu and click on the second extension item.
  menu = TestRenderViewContextMenu::Create(GetWebContents(), page_url);
  id.string_uid = "id2";
  ASSERT_TRUE(FindCommandId(menu.get(), id, &command_id));
  menu->ExecuteCommand(command_id, 0);
  ASSERT_TRUE(listener_done.WaitUntilSatisfied());

  // Upon completion, the replaced onclick callbacks should not have fired.
  ASSERT_FALSE(listener_error1.was_satisfied());
  ASSERT_FALSE(listener_error2.was_satisfied());
}

// Tests that updating the first radio item in a radio list from checked to
// unchecked should not work. The radio button should remain checked because
// context menu radio lists should always have one item selected.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest,
                       UpdateCheckedStateOfFirstRadioItem) {
  ExtensionTestMessageListener listener_created_radio1("created radio1 item");
  ExtensionTestMessageListener listener_created_radio2("created radio2 item");
  ExtensionTestMessageListener listener_created_item1("created normal item");
  ExtensionTestMessageListener listener_created_item2(
      "created second normal item");

  ExtensionTestMessageListener listener_radio2_clicked("onclick radio2");
  ExtensionTestMessageListener listener_item1_clicked("onclick normal item");
  ExtensionTestMessageListener listener_radio1_updated("radio1 updated");

  const extensions::Extension* extension =
      LoadContextMenuExtension("radio_check");
  ASSERT_TRUE(extension);

  // Check that all menu items are created.
  ASSERT_TRUE(listener_created_radio1.WaitUntilSatisfied());
  ASSERT_TRUE(listener_created_radio2.WaitUntilSatisfied());
  ASSERT_TRUE(listener_created_item1.WaitUntilSatisfied());
  ASSERT_TRUE(listener_created_item2.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), page_url));

  VerifyRadioItemSelectionState(menu.get(), extension->id(), "radio1", true);
  VerifyRadioItemSelectionState(menu.get(), extension->id(), "radio2", false);

  // Clicking the regular item calls chrome.contextMenus.update to uncheck the
  // first radio item.
  ExecuteCommand(menu.get(), extension->id(), "item1");
  ASSERT_TRUE(listener_item1_clicked.WaitUntilSatisfied());

  // Unchecking the second item should not deselect it. Recall that context menu
  // radio lists should always have one item selected.
  ASSERT_TRUE(listener_radio1_updated.WaitUntilSatisfied());
  VerifyRadioItemSelectionState(menu.get(), extension->id(), "radio1", true);
  VerifyRadioItemSelectionState(menu.get(), extension->id(), "radio2", false);
}

// Tests that updating a checked radio button (that is not the first item) to be
// unchecked should not work. The radio button should remain checked because
// context menu radio lists should always have one item selected.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest,
                       UpdateCheckedStateOfNonfirstRadioItem) {
  ExtensionTestMessageListener listener_created_radio1("created radio1 item");
  ExtensionTestMessageListener listener_created_radio2("created radio2 item");
  ExtensionTestMessageListener listener_created_item1("created normal item");
  ExtensionTestMessageListener listener_created_item2(
      "created second normal item");

  ExtensionTestMessageListener listener_radio2_clicked("onclick radio2");
  ExtensionTestMessageListener listener_item2_clicked(
      "onclick second normal item");
  ExtensionTestMessageListener listener_radio2_updated("radio2 updated");

  const extensions::Extension* extension =
      LoadContextMenuExtension("radio_check");
  ASSERT_TRUE(extension);

  // Check that all menu items are created.
  ASSERT_TRUE(listener_created_radio1.WaitUntilSatisfied());
  ASSERT_TRUE(listener_created_radio2.WaitUntilSatisfied());
  ASSERT_TRUE(listener_created_item1.WaitUntilSatisfied());

  ASSERT_TRUE(listener_created_item2.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), page_url));

  VerifyRadioItemSelectionState(menu.get(), extension->id(), "radio1", true);
  VerifyRadioItemSelectionState(menu.get(), extension->id(), "radio2", false);

  // Click on the second radio button. This should uncheck the first radio item
  // and check the second item.
  ExecuteCommand(menu.get(), extension->id(), "radio2");
  ASSERT_TRUE(listener_radio2_clicked.WaitUntilSatisfied());
  VerifyRadioItemSelectionState(menu.get(), extension->id(), "radio1", false);
  VerifyRadioItemSelectionState(menu.get(), extension->id(), "radio2", true);

  // Clicking the regular item calls chrome.contextMenus.update to uncheck the
  // second radio item.
  ExecuteCommand(menu.get(), extension->id(), "item2");
  ASSERT_TRUE(listener_item2_clicked.WaitUntilSatisfied());

  // Unchecking the second item should not deselect it. Recall that context menu
  // radio lists should always have one item selected.
  ASSERT_TRUE(listener_radio2_updated.WaitUntilSatisfied());
  VerifyRadioItemSelectionState(menu.get(), extension->id(), "radio1", false);
  VerifyRadioItemSelectionState(menu.get(), extension->id(), "radio2", true);
}

// Tests that setting "documentUrlPatterns" for an item properly restricts
// those items to matching pages.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, Patterns) {
  ExtensionTestMessageListener listener("created items");

  ASSERT_TRUE(LoadContextMenuExtension("patterns"));

  // Wait for the js test code to create its two items with patterns.
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Check that a document url that should match the items' patterns appears.
  GURL google_url("http://www.google.com");
  ASSERT_TRUE(MenuHasItemWithLabel(google_url, GURL(), false,
                                   std::string("test_item1")));
  ASSERT_TRUE(MenuHasItemWithLabel(google_url, GURL(), false,
                                   std::string("test_item2")));

  // Now check with a non-matching url.
  GURL test_url("http://www.test.com");
  ASSERT_FALSE(
      MenuHasItemWithLabel(test_url, GURL(), false, std::string("test_item1")));
  ASSERT_FALSE(
      MenuHasItemWithLabel(test_url, GURL(), false, std::string("test_item2")));
}

// Tests registering an item with a very long title that should get truncated in
// the actual menu displayed.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, LongTitle) {
  ExtensionTestMessageListener listener("created");

  // Load the extension and wait until it's created a menu item.
  ASSERT_TRUE(LoadContextMenuExtension("long_title"));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Make sure we have an item registered with a long title.
  size_t limit = extensions::ContextMenuMatcher::kMaxExtensionItemTitleLength;
  MenuItem::List items = GetItems();
  ASSERT_EQ(1u, items.size());
  MenuItem* item = items.at(0);
  ASSERT_GT(item->title().size(), limit);

  // Create a context menu, then find the item's label. It should be properly
  // truncated.
  GURL url("http://foo.com/");
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), url));

  std::u16string label;
  ASSERT_TRUE(GetItemLabel(menu.get(), item->id(), &label));
  ASSERT_TRUE(label.size() <= limit);
}

// Checks that Context Menus are ordered alphabetically by their name when
// extensions have only one single Context Menu item and by the extension name
// when multiples Context Menu items are created.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, TopLevel) {
  // We expect to see the following items in the menu:
  //   An Extension with multiple Context Menus
  //     Context Menu #1
  //     Context Menu #2
  //   Context Menu #1 - Extension #2
  //   Context Menu #2 - Extension #3
  //   Context Menu #3 - Extension #1
  //   Ze Extension with multiple Context Menus
  //     Context Menu #1
  //     Context Menu #2

  // Load extensions and wait until it's created a single menu item.
  ExtensionTestMessageListener listener1("created item");
  ASSERT_TRUE(LoadTopLevelContextMenuExtension("single1"));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener listener2("created item");
  ASSERT_TRUE(LoadTopLevelContextMenuExtension("single2"));
  ASSERT_TRUE(listener2.WaitUntilSatisfied());

  ExtensionTestMessageListener listener3("created item");
  ASSERT_TRUE(LoadTopLevelContextMenuExtension("single3"));
  ASSERT_TRUE(listener3.WaitUntilSatisfied());

  // Load extensions and wait until it's created two menu items.
  ExtensionTestMessageListener listener4("created items");
  ASSERT_TRUE(LoadTopLevelContextMenuExtension("multi4"));
  ASSERT_TRUE(listener4.WaitUntilSatisfied());

  ExtensionTestMessageListener listener5("created items");
  ASSERT_TRUE(LoadTopLevelContextMenuExtension("multi5"));
  ASSERT_TRUE(listener5.WaitUntilSatisfied());

  GURL url("http://foo.com/");
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), url));

  size_t index = 0;
  raw_ptr<MenuModel> model = nullptr;

  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0), &model,
      &index));
  EXPECT_EQ(u"An Extension with multiple Context Menus",
            model->GetLabelAt(index++));
  EXPECT_EQ(u"Context Menu #1 - Extension #2", model->GetLabelAt(index++));
  EXPECT_EQ(u"Context Menu #2 - Extension #3", model->GetLabelAt(index++));
  EXPECT_EQ(u"Context Menu #3 - Extension #1", model->GetLabelAt(index++));
  EXPECT_EQ(u"Ze Extension with multiple Context Menus",
            model->GetLabelAt(index++));
}

// Checks that in |menu|, the item at |index| has type |expected_type| and a
// label of |expected_label|.
static void ExpectLabelAndType(const char* expected_label,
                               MenuModel::ItemType expected_type,
                               const MenuModel& menu,
                               size_t index) {
  EXPECT_EQ(expected_type, menu.GetTypeAt(index));
  EXPECT_EQ(base::UTF8ToUTF16(expected_label), menu.GetLabelAt(index));
}

// In the separators test we build a submenu with items and separators in two
// different ways - this is used to verify the results in both cases. Separators
// are not included on OS_CHROMEOS.
static void VerifyMenuForSeparatorsTest(const MenuModel& menu) {
  // We expect to see the following items in the menu:
  //  radio1
  //  radio2
  //  --separator-- (automatically added)
  //  normal1
  //  --separator--
  //  normal2
  //  --separator--
  //  radio3
  //  radio4
  //  --separator--
  //  normal3

  size_t index = 0;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(7u, menu.GetItemCount());
#else
  ASSERT_EQ(11u, menu.GetItemCount());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ExpectLabelAndType("radio1", MenuModel::TYPE_RADIO, menu, index++);
  ExpectLabelAndType("radio2", MenuModel::TYPE_RADIO, menu, index++);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
#endif  // !BUILDFLAG(IS_CHROMEOS)
  ExpectLabelAndType("normal1", MenuModel::TYPE_COMMAND, menu, index++);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
#endif  // !BUILDFLAG(IS_CHROMEOS)
  ExpectLabelAndType("normal2", MenuModel::TYPE_COMMAND, menu, index++);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
#endif  // !BUILDFLAG(IS_CHROMEOS)
  ExpectLabelAndType("radio3", MenuModel::TYPE_RADIO, menu, index++);
  ExpectLabelAndType("radio4", MenuModel::TYPE_RADIO, menu, index++);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
#endif  // !BUILDFLAG(IS_CHROMEOS)
  ExpectLabelAndType("normal3", MenuModel::TYPE_COMMAND, menu, index++);
}

// Tests a number of cases for auto-generated and explicitly added separators.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuPersistentTest, Separators) {
  // Load the extension.
  ASSERT_TRUE(LoadContextMenuExtension("separators"));
  const extensions::Extension* extension = GetExtensionNamed("Separators Test");
  ASSERT_TRUE(extension);

  // Navigate to test1.html inside the extension, which should create a bunch
  // of items at the top-level (but they'll get pushed into an auto-generated
  // parent).
  ExtensionTestMessageListener listener1("test1 create finished");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(extension->GetResourceURL("test1.html"))));
  EXPECT_TRUE(listener1.WaitUntilSatisfied());

  GURL url("http://www.google.com/");
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), url));

  // The top-level item should be an "automagic parent" with the extension's
  // name.
  raw_ptr<MenuModel> model = nullptr;
  size_t index = 0;
  std::u16string label;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0),
      &model,
      &index));
  EXPECT_EQ(base::UTF8ToUTF16(extension->name()), model->GetLabelAt(index));
  ASSERT_EQ(MenuModel::TYPE_SUBMENU, model->GetTypeAt(index));

  // Get the submenu and verify the items there.
  MenuModel* submenu = model->GetSubmenuModelAt(index);
  ASSERT_TRUE(submenu);
  VerifyMenuForSeparatorsTest(*submenu);
  // Depends on `menu` so must be cleared before it is destroyed below.
  model = nullptr;

  // Now run our second test - navigate to test2.html which creates an explicit
  // parent node and populates that with the same items as in test1.
  ExtensionTestMessageListener listener2("test2 create finished");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(extension->GetResourceURL("test2.html"))));
  EXPECT_TRUE(listener2.WaitUntilSatisfied());
  menu = TestRenderViewContextMenu::Create(GetWebContents(), url);
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0),
      &model,
      &index));
  EXPECT_EQ(u"parent", model->GetLabelAt(index));
  submenu = model->GetSubmenuModelAt(index);
  ASSERT_TRUE(submenu);
  VerifyMenuForSeparatorsTest(*submenu);
}

// Tests that targetUrlPattern keeps items from appearing when there is no
// target url.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, TargetURLs) {
  ExtensionTestMessageListener listener("created items");
  ASSERT_TRUE(LoadContextMenuExtension("target_urls"));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  GURL google_url("http://www.google.com");
  GURL non_google_url("http://www.foo.com");

  // No target url - the item should not appear.
  ASSERT_FALSE(
      MenuHasItemWithLabel(google_url, GURL(), false, std::string("item1")));

  // A matching target url - the item should appear.
  ASSERT_TRUE(MenuHasItemWithLabel(google_url, google_url, false,
                                   std::string("item1")));

  // A non-matching target url - the item should not appear.
  ASSERT_FALSE(MenuHasItemWithLabel(google_url, non_google_url, false,
                                    std::string("item1")));
}

// TODO(http://crbug.com/1035062): This test is flaky only with the event
// page-based version. Remove this once that's fixed.
using ExtensionContextMenuSWTest = ExtensionContextMenuLazyTest;

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionContextMenuSWTest,
                         ::testing::Values(ContextType::kServiceWorker));

// Tests adding of context menus in incognito mode.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuSWTest, IncognitoSplit) {
  ExtensionTestMessageListener created("created item regular");
  ExtensionTestMessageListener created_incognito("created item incognito");

  ExtensionTestMessageListener onclick("onclick fired regular");
  ExtensionTestMessageListener onclick_incognito("onclick fired incognito");

  // Open an incognito window.
  Browser* browser_incognito =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  ASSERT_TRUE(LoadContextMenuExtensionWithIncognitoFlags("incognito"));

  // Wait for the extension's processes to tell us they've created an item.
  ASSERT_TRUE(created.WaitUntilSatisfied());
  ASSERT_TRUE(created_incognito.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), page_url));
  WebContents* incognito_web_contents =
      browser_incognito->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<TestRenderViewContextMenu> menu_incognito(
      TestRenderViewContextMenu::Create(incognito_web_contents, page_url));

  // Look for the extension item in the menu, and execute it.
  int command_id = ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->IsCommandIdEnabled(command_id));
  menu->ExecuteCommand(command_id, 0);

  // Wait for the extension's script to tell us its onclick fired. Ensure
  // that the incognito version doesn't fire until we explicitly click the
  // incognito menu item.
  ASSERT_TRUE(onclick.WaitUntilSatisfied());
  EXPECT_FALSE(onclick_incognito.was_satisfied());

  ASSERT_TRUE(menu_incognito->IsCommandIdEnabled(command_id));
  menu_incognito->ExecuteCommand(command_id, 0);
  ASSERT_TRUE(onclick_incognito.WaitUntilSatisfied());
}

// Tests that items with a context of frames only appear when the menu is
// invoked in a frame.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, Frames) {
  ExtensionTestMessageListener listener("created items");
  ASSERT_TRUE(LoadContextMenuExtension("frames"));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  ASSERT_TRUE(
      MenuHasItemWithLabel(page_url, GURL(), false, std::string("Page item")));
  ASSERT_FALSE(
      MenuHasItemWithLabel(page_url, GURL(), false, std::string("Frame item")));

  ASSERT_TRUE(
      MenuHasItemWithLabel(page_url, GURL(), true, std::string("Page item")));
  ASSERT_TRUE(
      MenuHasItemWithLabel(page_url, GURL(), true, std::string("Frame item")));
}

// Tests that info.frameId is correctly set when the context menu is invoked.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, ClickInFrame) {
  ExtensionTestMessageListener listener("created items");
  ASSERT_TRUE(LoadContextMenuExtension("frames"));
  GURL url_with_frame("data:text/html,<iframe name='child'>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_frame));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Click on a menu item in the main frame.
  EXPECT_EQ(
      "pageUrl=" + url_with_frame.spec() + ", frameUrl=undefined, frameId=0",
      ClickMenuInFrame(GetWebContents()->GetPrimaryMainFrame(), "item1"));

  // Click on a menu item in the child frame.
  content::RenderFrameHost* child_frame = content::FrameMatchingPredicate(
      GetWebContents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "child"));
  ASSERT_TRUE(child_frame);
  int extension_api_frame_id =
      extensions::ExtensionApiFrameIdMap::GetFrameId(child_frame);
  EXPECT_EQ(
      base::StringPrintf("pageUrl=undefined, frameUrl=about:blank, frameId=%d",
                         extension_api_frame_id),
      ClickMenuInFrame(child_frame, "item1"));
}

// Tests enabling and disabling a context menu item. The item starts
// enabled.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, StartEnabled) {
  TestEnabledContextMenu(true);
}

// Tests enabling and disabling a context menu item. The item starts
// disabled.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, StartDisabled) {
  TestEnabledContextMenu(false);
}

IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, EventPage) {
  // This test is event page-specific.
  if (GetParam() == ContextType::kServiceWorker)
    return;
  GURL about_blank("about:blank");
  extensions::ExtensionHostTestHelper host_helper(profile());
  host_helper.RestrictToType(
      extensions::mojom::ViewType::kExtensionBackgroundPage);
  const extensions::Extension* extension =
      LoadContextMenuExtension("event_page");
  ASSERT_TRUE(extension);
  // Wait for the background page to cycle.
  host_helper.WaitForDocumentElementAvailable();
  host_helper.WaitForHostDestroyed();

  // Test that menu items appear while the page is unloaded.
  ASSERT_TRUE(
      MenuHasItemWithLabel(about_blank, GURL(), false, std::string("Item 1")));
  ASSERT_TRUE(MenuHasItemWithLabel(about_blank, GURL(), false,
                                   std::string("Checkbox 1")));

  // Test that checked menu items retain their checkedness.
  extensions::ExtensionHostTestHelper checkbox_checked(profile());
  host_helper.RestrictToType(
      extensions::mojom::ViewType::kExtensionBackgroundPage);
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), about_blank));

  MenuItem::Id id(false, MenuItem::ExtensionKey(extension->id()));
  id.string_uid = "checkbox1";
  int command_id = -1;
  ASSERT_TRUE(FindCommandId(menu.get(), id, &command_id));
  EXPECT_FALSE(menu->IsCommandIdChecked(command_id));

  // Executing the checkbox also fires the onClicked event.
  ExtensionTestMessageListener listener("onClicked fired for checkbox1");
  menu->ExecuteCommand(command_id, 0);
  checkbox_checked.WaitForHostDestroyed();

  EXPECT_TRUE(menu->IsCommandIdChecked(command_id));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
}

// Flaky on Mac and Windows. https://crbug.com/1035062
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_IncognitoSplitContextMenuCount \
  DISABLED_IncognitoSplitContextMenuCount
#else
#define MAYBE_IncognitoSplitContextMenuCount IncognitoSplitContextMenuCount
#endif
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest,
                       MAYBE_IncognitoSplitContextMenuCount) {
  // TODO(crbug.com/40617251): Not yet implemented.
  if (GetParam() == ContextType::kServiceWorker)
    return;
  ExtensionTestMessageListener created("created item regular");
  ExtensionTestMessageListener created_incognito("created item incognito");

  // Create an incognito profile.
  Profile* incognito =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(incognito);
  ASSERT_TRUE(LoadContextMenuExtensionWithIncognitoFlags("incognito"));

  // Wait for the extension's processes to tell us they've created an item.
  ASSERT_TRUE(created.WaitUntilSatisfied());
  ASSERT_TRUE(created_incognito.WaitUntilSatisfied());
  ASSERT_EQ(2u, GetItems().size());

  browser()->profile()->DestroyOffTheRecordProfile(incognito);
  ASSERT_EQ(1u, GetItems().size());
}

// Tests updating checkboxes' checked state to true and false.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLazyTest, UpdateCheckboxes) {
  ExtensionTestMessageListener listener_context_menu_created("Menu created");
  const extensions::Extension* extension =
      LoadContextMenuExtension("checkboxes");
  ASSERT_TRUE(extension);

  ASSERT_TRUE(listener_context_menu_created.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), page_url));

  VerifyRadioItemSelectionState(menu.get(), extension->id(), "checkbox1",
                                false);
  VerifyRadioItemSelectionState(menu.get(), extension->id(), "checkbox2", true);

  ExtensionTestMessageListener listener_item1_clicked("onclick normal item");
  ExtensionTestMessageListener listener_unchecked_checkbox2(
      "checkbox2 unchecked");
  // Clicking the regular item calls chrome.contextMenus.update to uncheck the
  // second checkbox item.
  ExecuteCommand(menu.get(), extension->id(), "item1");
  ASSERT_TRUE(listener_item1_clicked.WaitUntilSatisfied());
  ASSERT_TRUE(listener_unchecked_checkbox2.WaitUntilSatisfied());

  VerifyRadioItemSelectionState(menu.get(), extension->id(), "checkbox1",
                                false);
  VerifyRadioItemSelectionState(menu.get(), extension->id(), "checkbox2",
                                false);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Extension context menu tests in locked fullscreen when locked and not locked
// for OnTask. Only relevant for non-web browser scenarios.
class ExtensionContextMenuLockedFullscreenTest
    : public ExtensionContextMenuBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  bool IsLockedForOnTask() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(ExtensionContextMenuLockedFullscreenTest,
                       VerifyItemStateInLockedFullscreenForOnTask) {
  browser()->SetLockedForOnTask(IsLockedForOnTask());

  // Enter locked fullscreen.
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);

  // Load test extension and wait for js test code to create context menu with
  // one item.
  ExtensionTestMessageListener listener("created context menu");
  base::FilePath extension_dir = GetRootDir().AppendASCII("locked_fullscreen");
  ASSERT_TRUE(
      LoadExtension(extension_dir, {.wait_for_registration_stored = true}));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Create / build the context menu and verify item state is enabled if the
  // instance is locked for OnTask. False otherwise.
  const GURL page_url("http://www.google.com");
  const std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), page_url));
  int command_id = ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_EQ(IsLockedForOnTask(), menu->IsCommandIdEnabled(command_id));
}

INSTANTIATE_TEST_SUITE_P(ExtensionContextMenuLockedFullscreenTests,
                         ExtensionContextMenuLockedFullscreenTest,
                         ::testing::Bool());
#endif

INSTANTIATE_TEST_SUITE_P(EventPage,
                         ExtensionContextMenuLazyTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionContextMenuLazyTest,
                         ::testing::Values(ContextType::kServiceWorker));
