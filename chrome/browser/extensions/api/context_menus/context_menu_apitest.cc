// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "components/version_info/channel.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_host.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/models/menu_model.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/extension_menu_model_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

using ContextType = extensions::browser_test_util::ContextType;

class ExtensionContextMenuApiTest : public ExtensionApiTest {
 public:
  explicit ExtensionContextMenuApiTest(
      ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}
  ~ExtensionContextMenuApiTest() override = default;
  ExtensionContextMenuApiTest(const ExtensionContextMenuApiTest&) = delete;
  ExtensionContextMenuApiTest& operator=(const ExtensionContextMenuApiTest&) =
      delete;
};

class ExtensionContextMenuApiTestWithContextType
    : public ExtensionContextMenuApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionContextMenuApiTestWithContextType()
      : ExtensionContextMenuApiTest(GetParam()) {}
  ~ExtensionContextMenuApiTestWithContextType() override = default;
  ExtensionContextMenuApiTestWithContextType(
      const ExtensionContextMenuApiTestWithContextType&) = delete;
  ExtensionContextMenuApiTestWithContextType& operator=(
      const ExtensionContextMenuApiTestWithContextType&) = delete;
};

// Android only supports service worker.
#if !BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionContextMenuApiTestWithContextType,
                         ::testing::Values(ContextType::kPersistentBackground));
#endif
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionContextMenuApiTestWithContextType,
                         ::testing::Values(ContextType::kServiceWorker));

// These tests are run from lazy extension contexts, namely event-page or
// service worker extensions.
using ExtensionContextMenuApiLazyTest =
    ExtensionContextMenuApiTestWithContextType;

// Android only supports service worker.
#if !BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(EventPage,
                         ExtensionContextMenuApiLazyTest,
                         ::testing::Values(ContextType::kEventPage));
#endif
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionContextMenuApiLazyTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(ExtensionContextMenuApiLazyTest, ContextMenus) {
  ASSERT_TRUE(RunExtensionTest("context_menus/event_page")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionContextMenuApiTestWithContextType, Count) {
  ASSERT_TRUE(RunExtensionTest("context_menus/count")) << message_;
}

// crbug.com/51436 -- creating context menus from multiple script contexts
// should work.
IN_PROC_BROWSER_TEST_P(ExtensionContextMenuApiTestWithContextType,
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

IN_PROC_BROWSER_TEST_P(ExtensionContextMenuApiTestWithContextType,
                       ContextMenusBasics) {
  ASSERT_TRUE(RunExtensionTest("context_menus/basics")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionContextMenuApiTestWithContextType,
                       ContextMenusNoPerms) {
  ASSERT_TRUE(RunExtensionTest("context_menus/no_perms")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionContextMenuApiTestWithContextType,
                       ContextMenusMultipleIds) {
  ASSERT_TRUE(RunExtensionTest("context_menus/item_ids")) << message_;
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
    content::WebContents* background_page = GetBackgroundPage(extension->id());
    ASSERT_EQ(false, content::EvalJs(background_page, script));
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
  content::WebContents* GetBackgroundPage(const ExtensionId& extension_id) {
    return process_manager()
        ->GetBackgroundHostForExtension(extension_id)
        ->host_contents();
  }

  ProcessManager* process_manager() { return ProcessManager::Get(profile()); }

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

// Tests hiding a parent menu item, when it is hidden and so are all of its
// children.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuVisibilityApiTest,
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
  EXPECT_NE(ui::MenuModel::TYPE_SEPARATOR,
            top_level_model_->GetTypeAt(top_level_index() - 1));
#endif

  ui::MenuModel* submodel =
      top_level_model_->GetSubmenuModelAt(top_level_index());
  ASSERT_TRUE(submodel);
  EXPECT_EQ(2u, submodel->GetItemCount());

  VerifyMenuItem("child1", submodel, 0, ui::MenuModel::TYPE_COMMAND, false);
  VerifyMenuItem("child2", submodel, 1, ui::MenuModel::TYPE_COMMAND, false);
}

// Tests hiding a parent menu item, when it is hidden and some of its children
// are visible.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuVisibilityApiTest,
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
  EXPECT_NE(ui::MenuModel::TYPE_SEPARATOR,
            top_level_model_->GetTypeAt(top_level_index() - 1));
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

}  // namespace extensions
