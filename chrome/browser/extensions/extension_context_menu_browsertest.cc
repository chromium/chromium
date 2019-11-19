// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/lazy_background_page_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/scoped_worker_based_extensions_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/models/menu_model.h"

using content::WebContents;
using extensions::ContextMenuMatcher;
using ContextType = extensions::ExtensionBrowserTest::ContextType;
using extensions::MenuItem;
using ui::MenuModel;

class ExtensionContextMenuBrowserTest
    : public extensions::ExtensionBrowserTest,
      public testing::WithParamInterface<ContextType> {
 public:
  void SetUp() override {
    extensions::ExtensionBrowserTest::SetUp();
    // Service Workers are currently only available on certain channels, so set
    // the channel for those tests.
    if (GetParam() == ContextType::kServiceWorker) {
      current_channel_ =
          std::make_unique<extensions::ScopedWorkerBasedExtensionsChannel>();
    }
  }

  std::string GetExtensionDirectory(base::StringPiece root) {
    if (GetParam() == ContextType::kPersistentBackground)
      return std::string(root);
    DCHECK_EQ(ContextType::kServiceWorker, GetParam());
    return base::StrCat({root, "/service_worker"});
  }

  // Helper to load an extension from context_menus/|subdirectory| in the
  // extensions test data dir.
  const extensions::Extension* LoadContextMenuExtension(
      base::StringPiece subdirectory) {
    base::FilePath extension_dir =
        test_data_dir_.AppendASCII("context_menus")
            .AppendASCII(GetExtensionDirectory(subdirectory));
    return LoadExtension(extension_dir);
  }

  // Helper to load an extension from context_menus/top_level/|subdirectory| in
  // the extensions test data dir.
  const extensions::Extension* LoadTopLevelContextMenuExtension(
      base::StringPiece subdirectory) {
    base::FilePath extension_dir =
        test_data_dir_.AppendASCII("context_menus").AppendASCII("top_level");
    extension_dir =
        extension_dir.AppendASCII(GetExtensionDirectory(subdirectory));
    return LoadExtension(extension_dir);
  }

  const extensions::Extension* LoadContextMenuExtensionIncognito(
      base::StringPiece subdirectory) {
    base::FilePath extension_dir =
        test_data_dir_.AppendASCII("context_menus")
            .AppendASCII(GetExtensionDirectory(subdirectory));
    return LoadExtensionIncognito(extension_dir);
  }

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
        extensions::ExtensionRegistry::Get(
            browser()->profile())->enabled_extensions();
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
    for (auto i = extension_ids.begin(); i != extension_ids.end(); ++i) {
      const MenuItem::OwnedList* list = menu_manager()->MenuItems(*i);
      for (const auto& item : *list)
        result.push_back(item.get());
    }
    return result;
  }

  // This creates a test menu for a page with |page_url| and |link_url|, looks
  // for an extension item with the given |label|, and returns true if the item
  // was found.
  bool MenuHasItemWithLabel(const GURL& page_url,
                            const GURL& link_url,
                            const GURL& frame_url,
                            const std::string& label) {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        TestRenderViewContextMenu::Create(GetWebContents(), page_url, link_url,
                                          frame_url));
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

    TestRenderViewContextMenu menu(frame, params);
    menu.Init();

    MenuItem::Id menu_item_id;
    menu_item_id.string_uid = target_menu_item_id;
    int command_id = -1;
    if (!FindCommandId(&menu, menu_item_id, &command_id))
      return "Menu item not found: " + target_menu_item_id;

    ExtensionTestMessageListener listener(false);
    menu.ExecuteCommand(command_id, 0);
    if (!listener.WaitUntilSatisfied())
      return "Onclick never fired for menu item: " + target_menu_item_id;

    return listener.message();
  }

  // This creates an extension that starts |enabled| and then switches to
  // |!enabled|.
  void TestEnabledContextMenu(bool enabled) {
    ExtensionTestMessageListener begin("begin", true);
    ExtensionTestMessageListener create("create", true);
    ExtensionTestMessageListener update("update", false);
    ASSERT_TRUE(LoadContextMenuExtension("enabled"));

    ASSERT_TRUE(begin.WaitUntilSatisfied());

    if (enabled)
      begin.Reply("start enabled");
    else
      begin.Reply("start disabled");

    // Wait for the extension to tell us it's created an item.
    ASSERT_TRUE(create.WaitUntilSatisfied());
    create.Reply("go");

    GURL page_url("http://www.google.com");

    // Create and build our test context menu.
    std::unique_ptr<TestRenderViewContextMenu> menu(
        TestRenderViewContextMenu::Create(GetWebContents(), page_url, GURL(),
                                          GURL()));

    // Look for the extension item in the menu, and make sure it's |enabled|.
    int command_id = ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
    ASSERT_EQ(enabled, menu->IsCommandIdEnabled(command_id));

    // Update the item and make sure it is now |!enabled|.
    ASSERT_TRUE(update.WaitUntilSatisfied());
    ASSERT_EQ(!enabled, menu->IsCommandIdEnabled(command_id));
  }

  bool MenuHasExtensionItemWithLabel(TestRenderViewContextMenu* menu,
                                     const std::string& label) {
    base::string16 label16 = base::UTF8ToUTF16(label);
    for (const auto& it : menu->extension_items().extension_item_map_) {
      const MenuItem::Id& id = it.second;
      base::string16 tmp_label;
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
                    base::string16* result) const {
    int command_id = 0;
    if (!FindCommandId(menu, id, &command_id))
      return false;

    MenuModel* model = nullptr;
    int index = -1;
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

  std::unique_ptr<extensions::ScopedWorkerBasedExtensionsChannel>
      current_channel_;
};

// Tests adding a simple context menu item.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, Simple) {
  ExtensionTestMessageListener listener1("created item", false);
  ExtensionTestMessageListener listener2("onclick fired", false);
  ASSERT_TRUE(LoadContextMenuExtension("simple"));

  // Wait for the extension to tell us it's created an item.
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), page_url, GURL(),
                                        GURL()));

  // Look for the extension item in the menu, and execute it.
  int command_id = ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->IsCommandIdEnabled(command_id));
  menu->ExecuteCommand(command_id, 0);

  // Wait for the extension's script to tell us its onclick fired.
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

// Tests that previous onclick is not fired after updating the menu's onclick,
// and whether setting onclick to null removes the handler.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, UpdateOnclick) {
  // The onclick property is not supported for service worker-based
  // extensions.
  if (GetParam() == ContextType::kServiceWorker)
    return;
  ExtensionTestMessageListener listener_error1("onclick1-unexpected", false);
  ExtensionTestMessageListener listener_error2("onclick2-unexpected", false);
  ExtensionTestMessageListener listener_update1("update1", true);
  ExtensionTestMessageListener listener_update2("update2", false);
  ExtensionTestMessageListener listener_done("onclick2", false);

  const extensions::Extension* extension =
      LoadContextMenuExtension("onclick_null");
  ASSERT_TRUE(extension);

  // Wait till item has been created and updated.
  ASSERT_TRUE(listener_update1.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), page_url, GURL(),
                                        GURL()));

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
  menu = TestRenderViewContextMenu::Create(GetWebContents(), page_url, GURL(),
                                           GURL());
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
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest,
                       UpdateCheckedStateOfFirstRadioItem) {
  ExtensionTestMessageListener listener_created_radio1("created radio1 item",
                                                       false);
  ExtensionTestMessageListener listener_created_radio2("created radio2 item",
                                                       false);
  ExtensionTestMessageListener listener_created_item1("created normal item",
                                                      false);
  ExtensionTestMessageListener listener_created_item2(
      "created second normal item", false);

  ExtensionTestMessageListener listener_radio2_clicked("onclick radio2", false);
  ExtensionTestMessageListener listener_item1_clicked("onclick normal item",
                                                      false);
  ExtensionTestMessageListener listener_radio1_updated("radio1 updated", false);

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
      TestRenderViewContextMenu::Create(GetWebContents(), page_url, GURL(),
                                        GURL()));

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
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest,
                       UpdateCheckedStateOfNonfirstRadioItem) {
  ExtensionTestMessageListener listener_created_radio1("created radio1 item",
                                                       false);
  ExtensionTestMessageListener listener_created_radio2("created radio2 item",
                                                       false);
  ExtensionTestMessageListener listener_created_item1("created normal item",
                                                      false);
  ExtensionTestMessageListener listener_created_item2(
      "created second normal item", false);

  ExtensionTestMessageListener listener_radio2_clicked("onclick radio2", false);
  ExtensionTestMessageListener listener_item2_clicked(
      "onclick second normal item", false);
  ExtensionTestMessageListener listener_radio2_updated("radio2 updated", false);

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
      TestRenderViewContextMenu::Create(GetWebContents(), page_url, GURL(),
                                        GURL()));

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
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, Patterns) {
  ExtensionTestMessageListener listener("created items", false);

  ASSERT_TRUE(LoadContextMenuExtension("patterns"));

  // Wait for the js test code to create its two items with patterns.
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Check that a document url that should match the items' patterns appears.
  GURL google_url("http://www.google.com");
  ASSERT_TRUE(MenuHasItemWithLabel(google_url, GURL(), GURL(),
                                   std::string("test_item1")));
  ASSERT_TRUE(MenuHasItemWithLabel(google_url, GURL(), GURL(),
                                   std::string("test_item2")));

  // Now check with a non-matching url.
  GURL test_url("http://www.test.com");
  ASSERT_FALSE(MenuHasItemWithLabel(test_url, GURL(), GURL(),
                                    std::string("test_item1")));
  ASSERT_FALSE(MenuHasItemWithLabel(test_url, GURL(), GURL(),
                                    std::string("test_item2")));
}

// Tests registering an item with a very long title that should get truncated in
// the actual menu displayed.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, LongTitle) {
  ExtensionTestMessageListener listener("created", false);

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
      TestRenderViewContextMenu::Create(GetWebContents(), url, GURL(), GURL()));

  base::string16 label;
  ASSERT_TRUE(GetItemLabel(menu.get(), item->id(), &label));
  ASSERT_TRUE(label.size() <= limit);
}

// Flaky on Windows debug bots. http://crbug.com/251590
#if defined(OS_WIN)
#define MAYBE_TopLevel DISABLED_TopLevel
#else
#define MAYBE_TopLevel TopLevel
#endif
// Checks that Context Menus are ordered alphabetically by their name when
// extensions have only one single Context Menu item and by the extension name
// when multiples Context Menu items are created.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, MAYBE_TopLevel) {
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
  ExtensionTestMessageListener listener1("created item", false);
  ASSERT_TRUE(LoadTopLevelContextMenuExtension("single1"));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener listener2("created item", false);
  ASSERT_TRUE(LoadTopLevelContextMenuExtension("single2"));
  ASSERT_TRUE(listener2.WaitUntilSatisfied());

  ExtensionTestMessageListener listener3("created item", false);
  ASSERT_TRUE(LoadTopLevelContextMenuExtension("single3"));
  ASSERT_TRUE(listener3.WaitUntilSatisfied());

  // Load extensions and wait until it's created two menu items.
  ExtensionTestMessageListener listener4("created items", false);
  ASSERT_TRUE(LoadTopLevelContextMenuExtension("multi4"));
  ASSERT_TRUE(listener4.WaitUntilSatisfied());

  ExtensionTestMessageListener listener5("created items", false);
  ASSERT_TRUE(LoadTopLevelContextMenuExtension("multi5"));
  ASSERT_TRUE(listener5.WaitUntilSatisfied());

  GURL url("http://foo.com/");
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), url, GURL(), GURL()));

  int index = 0;
  MenuModel* model = nullptr;

  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0), &model,
      &index));
  EXPECT_EQ(base::UTF8ToUTF16("An Extension with multiple Context Menus"),
            model->GetLabelAt(index++));
  EXPECT_EQ(base::UTF8ToUTF16("Context Menu #1 - Extension #2"),
            model->GetLabelAt(index++));
  EXPECT_EQ(base::UTF8ToUTF16("Context Menu #2 - Extension #3"),
            model->GetLabelAt(index++));
  EXPECT_EQ(base::UTF8ToUTF16("Context Menu #3 - Extension #1"),
            model->GetLabelAt(index++));
  EXPECT_EQ(base::UTF8ToUTF16("Ze Extension with multiple Context Menus"),
            model->GetLabelAt(index++));
}

// Checks that in |menu|, the item at |index| has type |expected_type| and a
// label of |expected_label|.
static void ExpectLabelAndType(const char* expected_label,
                               MenuModel::ItemType expected_type,
                               const MenuModel& menu,
                               int index) {
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

  int index = 0;
#if defined(OS_CHROMEOS)
  ASSERT_EQ(7, menu.GetItemCount());
#else
  ASSERT_EQ(11, menu.GetItemCount());
#endif  // OS_CHROMEOS
  ExpectLabelAndType("radio1", MenuModel::TYPE_RADIO, menu, index++);
  ExpectLabelAndType("radio2", MenuModel::TYPE_RADIO, menu, index++);
#if !defined(OS_CHROMEOS)
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
#endif  // !OS_CHROMEOS
  ExpectLabelAndType("normal1", MenuModel::TYPE_COMMAND, menu, index++);
#if !defined(OS_CHROMEOS)
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
#endif  // !OS_CHROMEOS
  ExpectLabelAndType("normal2", MenuModel::TYPE_COMMAND, menu, index++);
#if !defined(OS_CHROMEOS)
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
#endif  // !OS_CHROMEOS
  ExpectLabelAndType("radio3", MenuModel::TYPE_RADIO, menu, index++);
  ExpectLabelAndType("radio4", MenuModel::TYPE_RADIO, menu, index++);
#if !defined(OS_CHROMEOS)
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
#endif  // !OS_CHROMEOS
  ExpectLabelAndType("normal3", MenuModel::TYPE_COMMAND, menu, index++);
}

#if defined(OS_WIN)
#define MAYBE_Separators DISABLED_Separators
#else
#define MAYBE_Separators Separators
#endif

// Tests a number of cases for auto-generated and explicitly added separators.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, Separators) {
  // TODO(crbug.com/939664): Not yet implemented.
  if (GetParam() == ContextType::kServiceWorker)
    return;
  // Load the extension.
  ASSERT_TRUE(LoadContextMenuExtension("separators"));
  const extensions::Extension* extension = GetExtensionNamed("Separators Test");
  ASSERT_TRUE(extension);

  // Navigate to test1.html inside the extension, which should create a bunch
  // of items at the top-level (but they'll get pushed into an auto-generated
  // parent).
  ExtensionTestMessageListener listener1("test1 create finished", false);
  ui_test_utils::NavigateToURL(browser(),
                               GURL(extension->GetResourceURL("test1.html")));
  EXPECT_TRUE(listener1.WaitUntilSatisfied());

  GURL url("http://www.google.com/");
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), url, GURL(), GURL()));

  // The top-level item should be an "automagic parent" with the extension's
  // name.
  MenuModel* model = nullptr;
  int index = 0;
  base::string16 label;
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

  // Now run our second test - navigate to test2.html which creates an explicit
  // parent node and populates that with the same items as in test1.
  ExtensionTestMessageListener listener2("test2 create finished", false);
  ui_test_utils::NavigateToURL(browser(),
                               GURL(extension->GetResourceURL("test2.html")));
  EXPECT_TRUE(listener2.WaitUntilSatisfied());
  menu =
      TestRenderViewContextMenu::Create(GetWebContents(), url, GURL(), GURL());
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0),
      &model,
      &index));
  EXPECT_EQ(base::UTF8ToUTF16("parent"), model->GetLabelAt(index));
  submenu = model->GetSubmenuModelAt(index);
  ASSERT_TRUE(submenu);
  VerifyMenuForSeparatorsTest(*submenu);
}

// Tests that targetUrlPattern keeps items from appearing when there is no
// target url.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, TargetURLs) {
  ExtensionTestMessageListener listener("created items", false);
  ASSERT_TRUE(LoadContextMenuExtension("target_urls"));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  GURL google_url("http://www.google.com");
  GURL non_google_url("http://www.foo.com");

  // No target url - the item should not appear.
  ASSERT_FALSE(
      MenuHasItemWithLabel(google_url, GURL(), GURL(), std::string("item1")));

  // A matching target url - the item should appear.
  ASSERT_TRUE(MenuHasItemWithLabel(google_url, google_url, GURL(),
                                   std::string("item1")));

  // A non-matching target url - the item should not appear.
  ASSERT_FALSE(MenuHasItemWithLabel(google_url, non_google_url, GURL(),
                                    std::string("item1")));
}

// Tests adding of context menus in incognito mode.
#if defined(OS_LINUX)
// Flakily hangs on Linux/CrOS - http://crbug.com/88317
#define MAYBE_IncognitoSplit DISABLED_IncognitoSplit
#else
#define MAYBE_IncognitoSplit IncognitoSplit
#endif

IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, MAYBE_IncognitoSplit) {
  ExtensionTestMessageListener created("created item regular", false);
  ExtensionTestMessageListener created_incognito("created item incognito",
                                                 false);

  ExtensionTestMessageListener onclick("onclick fired regular", false);
  ExtensionTestMessageListener onclick_incognito("onclick fired incognito",
                                                 false);

  // Open an incognito window.
  Browser* browser_incognito =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  ASSERT_TRUE(LoadContextMenuExtensionIncognito("incognito"));

  // Wait for the extension's processes to tell us they've created an item.
  ASSERT_TRUE(created.WaitUntilSatisfied());
  ASSERT_TRUE(created_incognito.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), page_url, GURL(),
                                        GURL()));
  WebContents* incognito_web_contents =
      browser_incognito->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<TestRenderViewContextMenu> menu_incognito(
      TestRenderViewContextMenu::Create(incognito_web_contents, page_url,
                                        GURL(), GURL()));

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
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, Frames) {
  ExtensionTestMessageListener listener("created items", false);
  ASSERT_TRUE(LoadContextMenuExtension("frames"));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");
  GURL no_frame_url;
  GURL frame_url("http://www.google.com");

  ASSERT_TRUE(MenuHasItemWithLabel(page_url, GURL(), no_frame_url,
                                   std::string("Page item")));
  ASSERT_FALSE(MenuHasItemWithLabel(page_url, GURL(), no_frame_url,
                                    std::string("Frame item")));

  ASSERT_TRUE(MenuHasItemWithLabel(page_url, GURL(), frame_url,
                                   std::string("Page item")));
  ASSERT_TRUE(MenuHasItemWithLabel(page_url, GURL(), frame_url,
                                   std::string("Frame item")));
}

// Tests that info.frameId is correctly set when the context menu is invoked.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, ClickInFrame) {
  ExtensionTestMessageListener listener("created items", false);
  ASSERT_TRUE(LoadContextMenuExtension("frames"));
  GURL url_with_frame("data:text/html,<iframe name='child'>");
  ui_test_utils::NavigateToURL(browser(), url_with_frame);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Click on a menu item in the main frame.
  EXPECT_EQ(
      "pageUrl=" + url_with_frame.spec() + ", frameUrl=undefined, frameId=0",
      ClickMenuInFrame(GetWebContents()->GetMainFrame(), "item1"));

  // Click on a menu item in the child frame.
  content::RenderFrameHost* child_frame = content::FrameMatchingPredicate(
      GetWebContents(),
      base::BindRepeating(&content::FrameMatchesName, "child"));
  ASSERT_TRUE(child_frame);
  int extension_api_frame_id =
      extensions::ExtensionApiFrameIdMap::GetFrameId(child_frame);
  EXPECT_EQ(
      base::StringPrintf("pageUrl=undefined, frameUrl=about:blank, frameId=%d",
                         extension_api_frame_id),
      ClickMenuInFrame(child_frame, "item1"));
}

// Tests enabling and disabling a context menu item.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, Enabled) {
  TestEnabledContextMenu(true);
  TestEnabledContextMenu(false);
}

class ExtensionContextMenuBrowserLazyTest :
    public ExtensionContextMenuBrowserTest {
  void SetUpOnMainThread() override {
    ExtensionContextMenuBrowserTest::SetUpOnMainThread();
    // Set shorter delays to prevent test timeouts.
    extensions::ProcessManager::SetEventPageIdleTimeForTesting(1);
    extensions::ProcessManager::SetEventPageSuspendingTimeForTesting(0);
  }
};

IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserLazyTest, EventPage) {
  GURL about_blank("about:blank");
  LazyBackgroundObserver page_complete;
  const extensions::Extension* extension = LoadContextMenuExtension(
      "event_page");
  ASSERT_TRUE(extension);
  page_complete.Wait();

  // Test that menu items appear while the page is unloaded.
  ASSERT_TRUE(MenuHasItemWithLabel(
      about_blank, GURL(), GURL(), std::string("Item 1")));
  ASSERT_TRUE(MenuHasItemWithLabel(
      about_blank, GURL(), GURL(), std::string("Checkbox 1")));

  // Test that checked menu items retain their checkedness.
  LazyBackgroundObserver checkbox_checked;
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), about_blank, GURL(),
                                        GURL()));

  MenuItem::Id id(false, MenuItem::ExtensionKey(extension->id()));
  id.string_uid = "checkbox1";
  int command_id = -1;
  ASSERT_TRUE(FindCommandId(menu.get(), id, &command_id));
  EXPECT_FALSE(menu->IsCommandIdChecked(command_id));

  // Executing the checkbox also fires the onClicked event.
  ExtensionTestMessageListener listener("onClicked fired for checkbox1", false);
  menu->ExecuteCommand(command_id, 0);
  checkbox_checked.WaitUntilClosed();

  EXPECT_TRUE(menu->IsCommandIdChecked(command_id));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest,
                       IncognitoSplitContextMenuCount) {
  // TODO(crbug.com/939664): Not yet implemented.
  if (GetParam() == ContextType::kServiceWorker)
    return;
  ExtensionTestMessageListener created("created item regular", false);
  ExtensionTestMessageListener created_incognito("created item incognito",
                                                 false);

  // Create an incognito profile.
  ASSERT_TRUE(browser()->profile()->GetOffTheRecordProfile());
  ASSERT_TRUE(LoadContextMenuExtensionIncognito("incognito"));

  // Wait for the extension's processes to tell us they've created an item.
  ASSERT_TRUE(created.WaitUntilSatisfied());
  ASSERT_TRUE(created_incognito.WaitUntilSatisfied());
  ASSERT_EQ(2u, GetItems().size());

  browser()->profile()->DestroyOffTheRecordProfile();
  ASSERT_EQ(1u, GetItems().size());
}

// Tests updating checkboxes' checked state to true and false.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuBrowserTest, UpdateCheckboxes) {
  ExtensionTestMessageListener listener_context_menu_created("Menu created",
                                                             false);
  const extensions::Extension* extension =
      LoadContextMenuExtension("checkboxes");
  ASSERT_TRUE(extension);

  ASSERT_TRUE(listener_context_menu_created.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetWebContents(), page_url, GURL(),
                                        GURL()));

  VerifyRadioItemSelectionState(menu.get(), extension->id(), "checkbox1",
                                false);
  VerifyRadioItemSelectionState(menu.get(), extension->id(), "checkbox2", true);

  ExtensionTestMessageListener listener_item1_clicked("onclick normal item",
                                                      false);
  ExtensionTestMessageListener listener_unchecked_checkbox2(
      "checkbox2 unchecked", false);
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

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionContextMenuBrowserTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionContextMenuBrowserTest,
                         ::testing::Values(ContextType::kServiceWorker));
// TODO(crbug.com/939664): Enable this test for service workers?
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionContextMenuBrowserLazyTest,
                         ::testing::Values(ContextType::kPersistentBackground));
