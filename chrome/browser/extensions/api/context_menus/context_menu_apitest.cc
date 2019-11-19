// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/scoped_worker_based_extensions_channel.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/models/menu_model.h"

namespace extensions {

class ExtensionContextMenuApiTest : public ExtensionApiTest {
 public:
  ExtensionContextMenuApiTest()
      : top_level_model_(nullptr), menu_(nullptr), top_level_index_(-1) {}

  void SetUpTestExtension() {
    extension_ = LoadExtension(
        test_data_dir_.AppendASCII("context_menus/item_visibility/"));
  }

  // Sets up the top-level model, which is the list of menu items (both related
  // and unrelated to extensions) that is passed to UI code to be displayed.
  bool SetupTopLevelMenuModel() {
    content::RenderFrameHost* frame =
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
    content::ContextMenuParams params;
    params.page_url = frame->GetLastCommittedURL();

    // Create context menu.
    menu_.reset(new TestRenderViewContextMenu(frame, params));
    menu_->Init();

    // Get menu model.
    bool valid_setup = menu_->GetMenuModelAndItemIndex(
        menu_->extension_items().ConvertToExtensionsCustomCommandId(0),
        &top_level_model_, &top_level_index_);

    EXPECT_GT(top_level_index(), 0);

    return valid_setup;
  }

  void CallAPI(const std::string& script) { CallAPI(extension_, script); }

  void CallAPI(const Extension* extension, const std::string& script) {
    content::WebContents* background_page = GetBackgroundPage(extension->id());
    bool error = false;
    ASSERT_TRUE(
        content::ExecuteScriptAndExtractBool(background_page, script, &error));
    ASSERT_FALSE(error);
  }

  // Verifies that the UI menu model has the given number of extension menu
  // items, |num_items|, of a menu model |type|.
  void VerifyNumExtensionItemsInMenuModel(int num_items,
                                          ui::MenuModel::ItemType type) {
    int num_found = 0;
    for (int i = 0; i < top_level_model_->GetItemCount(); i++) {
      int command_id = top_level_model_->GetCommandIdAt(i);
      if (ContextMenuMatcher::IsExtensionsCustomCommandId(command_id) &&
          top_level_model_->GetTypeAt(i) == type) {
        num_found++;
      }
    }
    ASSERT_TRUE(num_found == num_items);
  }

  // Verifies that the context menu is valid and contains the given number of
  // menu items, |num_items|.
  void VerifyNumContextMenuItems(int num_items) {
    ASSERT_TRUE(menu());
    EXPECT_EQ(num_items,
              (int)(menu_->extension_items().extension_item_map_.size()));
  }

  // Verifies a context menu item's visibility, title, and item type.
  void VerifyMenuItem(const std::string& title,
                      ui::MenuModel* model,
                      int index,
                      ui::MenuModel::ItemType type,
                      bool visible) {
    EXPECT_EQ(base::ASCIIToUTF16(title), model->GetLabelAt(index));
    ASSERT_EQ(type, model->GetTypeAt(index));
    EXPECT_EQ(visible, model->IsVisibleAt(index));
  }

  int top_level_index() { return top_level_index_; }

  TestRenderViewContextMenu* menu() { return menu_.get(); }

  const Extension* extension() { return extension_; }

  ui::MenuModel* top_level_model_;

 private:
  content::WebContents* GetBackgroundPage(const std::string& extension_id) {
    return process_manager()
        ->GetBackgroundHostForExtension(extension_id)
        ->host_contents();
  }

  ProcessManager* process_manager() { return ProcessManager::Get(profile()); }

  const Extension* extension_;
  std::unique_ptr<TestRenderViewContextMenu> menu_;
  int top_level_index_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionContextMenuApiTest);
};

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContextMenus) {
  ASSERT_TRUE(RunExtensionTest("context_menus/basics")) << message_;
  ASSERT_TRUE(RunExtensionTest("context_menus/no_perms")) << message_;
  ASSERT_TRUE(RunExtensionTest("context_menus/item_ids")) << message_;
  ASSERT_TRUE(RunExtensionTest("context_menus/event_page")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ServiceWorkerContextMenus) {
  ScopedWorkerBasedExtensionsChannel worker_channel_override;
  ASSERT_TRUE(RunExtensionTestWithFlags("context_menus/event_page",
                                        kFlagRunAsServiceWorkerBasedExtension))
      << message_;
}

// crbug.com/51436 -- creating context menus from multiple script contexts
// should work.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContextMenusFromMultipleContexts) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("context_menus/add_from_multiple_contexts"))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  {
    // Tell the extension to update the page action state.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        extension->GetResourceURL("popup.html"));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  {
    // Tell the extension to update the page action state again.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        extension->GetResourceURL("popup2.html"));
    ASSERT_TRUE(catcher.GetNextResult());
  }
}

// Tests showing a single visible menu item in the top-level menu model, which
// includes actions like "Back", "View Page Source", "Inspect", etc.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest, ShowOneTopLevelItem) {
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
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest, HideTopLevelItem) {
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

// Tests hiding a parent menu item, when it is hidden and so are all of its
// children.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest,
                       HideTopLevelSubmenuItemIfHiddenAndChildrenHidden) {
  SetUpTestExtension();
  CallAPI("create({id: 'id', title: 'parent', visible: false});");
  CallAPI("create({title: 'child1', parentId: 'id', visible: false});");
  CallAPI("create({title: 'child2', parentId: 'id', visible: false});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(3);

  VerifyMenuItem("parent", top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, false);

  // Since the extension submenu is hidden, the previous separator should not be
  // in the model.
  EXPECT_NE(ui::MenuModel::TYPE_SEPARATOR,
            top_level_model_->GetTypeAt(top_level_index() - 1));

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(2, submodel->GetItemCount());

  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("child2", submodel, 1, ui::MenuModel::TYPE_COMMAND, false);
}

// Tests hiding a parent menu item, when it is hidden and some of its children
// are visible.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest,
                       HideTopLevelSubmenuItemIfHiddenAndSomeChildrenVisible) {
  SetUpTestExtension();
  CallAPI("create({id: 'id', title: 'parent', visible: false});");
  CallAPI("create({title: 'child1', parentId: 'id', visible: false});");
  CallAPI("create({title: 'child2', parentId: 'id', visible: true});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(3);

  VerifyMenuItem("parent", top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, false);

  // Since the extension submenu is hidden, the previous separator should not be
  // in the model.
  EXPECT_NE(ui::MenuModel::TYPE_SEPARATOR,
            top_level_model_->GetTypeAt(top_level_index() - 1));

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(2, submodel->GetItemCount());

  // Though the children's internal visibility state remains unchanged, the ui
  // code will hide the children if the parent is hidden.
  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("child2", submodel, 1, ui::MenuModel::TYPE_COMMAND, true);
}

// Tests showing a single top-level parent menu item, when it is visible, but
// all of its child items are hidden. The child items' hidden states are tested
// too. Recall that a top-level item can be either a parent item specified by
// the developer or parent item labeled with the extension's name. In this case,
// we test the former.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest,
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
  EXPECT_EQ(2, submodel->GetItemCount());

  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("child2", submodel, 1, ui::MenuModel::TYPE_COMMAND, false);
}

// Tests showing a top-level parent menu item as a submenu, when some of its
// child items are visibile. The child items' visibilities are tested too.
// Recall that a top-level item can be either a parent item specified by the
// developer or parent item labeled with the extension's name. In this case, we
// test the former.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest,
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
  EXPECT_EQ(2, submodel->GetItemCount());

  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_COMMAND, true);
  VerifyMenuItem("child2", submodel, 1, ui::MenuModel::TYPE_COMMAND, false);
}

// Tests showing a top-level parent menu item, when all of its child items are
// hidden. Recall that a top-level item can be either a parent item specified by
// the developer or parent item labeled with the extension's name. In this case,
// we test the latter. This extension-named top-level item should always be
// visible.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest,
                       ShowExtensionNamedTopLevelItemIfAllChildrenAreHidden) {
  SetUpTestExtension();
  CallAPI("create({title: 'item1', visible: false});");
  CallAPI("create({title: 'item2', visible: false});");
  CallAPI("create({title: 'item3', visible: false});");

  ASSERT_TRUE(SetupTopLevelMenuModel());

  VerifyNumContextMenuItems(3);

  VerifyMenuItem(extension()->name(), top_level_model_, top_level_index(),
                 ui::MenuModel::TYPE_SUBMENU, true);

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(3, submodel->GetItemCount());

  VerifyMenuItem("item1", submodel, 0, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("item2", submodel, 1, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("item3", submodel, 2, ui::MenuModel::TYPE_COMMAND, false);
}

// Tests showing a top-level parent menu item, when some of its child items are
// visible. The child items' visibilities are tested as well. Recall that a
// top-level item can be either a parent item specified by the developer or
// parent item labeled with the extension's name. In this case, we test the
// latter.
//
// Also, this tests that hiding a parent item should hide its children even if
// they are set as visible.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest,
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
  EXPECT_EQ(3, submodel->GetItemCount());

  VerifyMenuItem("item1", submodel, 0, ui::MenuModel::TYPE_COMMAND, true);
  VerifyMenuItem("item2", submodel, 1, ui::MenuModel::TYPE_COMMAND, true);
  VerifyMenuItem("item3", submodel, 2, ui::MenuModel::TYPE_SUBMENU, false);

  ui::MenuModel* item3_submodel = submodel->GetSubmenuModelAt(2);
  ASSERT_TRUE(item3_submodel);
  EXPECT_EQ(2, item3_submodel->GetItemCount());

  // Though the children's internal visibility state remains unchanged, the ui
  // code will hide the children if the parent is hidden.
  VerifyMenuItem("child1", item3_submodel, 0, ui::MenuModel::TYPE_COMMAND,
                 true);
  VerifyMenuItem("child2", item3_submodel, 1, ui::MenuModel::TYPE_COMMAND,
                 true);
}

// Tests that more than one extension named top-level parent menu item can be
// displayed in the context menu.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuApiTest,
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

}  // namespace extensions
