// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/context_menu_matcher.h"
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/test_extension_menu_icon_loader.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/utils/extension_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/models/simple_menu_model.h"

namespace extensions {

// A helper used to show all menu items added by the extension.
bool MenuItemHasAnyContext(const extensions::MenuItem* item) {
  return true;
}

class ContextMenuMatcherTest : public testing::Test {
 public:
  ContextMenuMatcherTest()
      : profile_(std::make_unique<TestingProfile>()),
        manager_(CreateMenuManager()),
        prefs_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

  ContextMenuMatcherTest(const ContextMenuMatcherTest&) = delete;
  ContextMenuMatcherTest& operator=(const ContextMenuMatcherTest&) = delete;

  // Returns a test item with the given string ID.
  std::unique_ptr<MenuItem> CreateTestItem(Extension* extension,
                                           const std::string& string_id,
                                           bool visible) {
    MenuItem::Id id(false, MenuItem::ExtensionKey(extension->id()));
    id.string_uid = string_id;
    return std::make_unique<MenuItem>(
        id, "test", false, visible, true, MenuItem::NORMAL,
        MenuItem::ContextList(MenuItem::LAUNCHER));
  }

  // Returns a test item with the given string ID for WebView.
  std::unique_ptr<MenuItem> CreateTestItem(Extension* extension,
                                           int webview_embedder_process_id,
                                           int webview_embedder_frame_id,
                                           int webview_instance_id,
                                           const std::string& string_id,
                                           bool visible) {
    const ExtensionId& extension_id = MaybeGetExtensionId(extension);
    MenuItem::Id id(false, MenuItem::ExtensionKey(
                               extension_id, webview_embedder_process_id,
                               webview_embedder_frame_id, webview_instance_id));
    id.string_uid = string_id;
    return std::make_unique<MenuItem>(
        id, "test", false, visible, true, MenuItem::NORMAL,
        MenuItem::ContextList(MenuItem::LAUNCHER));
  }

  // Creates and returns a test Extension.
  Extension* AddExtension(const std::string& name) {
    scoped_refptr<Extension> extension = prefs_.AddExtension(name);
    extensions_.push_back(extension);
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile_.get());
    registry->AddEnabled(extension);
    return extension.get();
  }

  // Creates and returns a menu manager.
  MenuManager* CreateMenuManager() {
    return static_cast<MenuManager*>(
        MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(),
            base::BindRepeating(
                &MenuManagerFactory::BuildServiceInstanceForTesting)));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  raw_ptr<MenuManager> manager_;
  ExtensionList extensions_;
  TestExtensionPrefs prefs_;
};

// Tests appending an extension item with an invisible submenu.
TEST_F(ContextMenuMatcherTest, AppendExtensionItemsWithInvisibleSubmenu) {
  Extension* extension = AddExtension("test");

  // Add a new item with an invisible child item.
  std::unique_ptr<MenuItem> parent = CreateTestItem(extension, "parent", false);
  MenuItem::Id parent_id = parent->id();
  int parent_index = 0;
  std::unique_ptr<MenuItem> child = CreateTestItem(extension, "child", false);
  int child_index = 1;
  ASSERT_TRUE(manager_->AddContextItem(extension, std::move(parent)));
  manager_->AddChildItem(parent_id, std::move(child));

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);

  std::unique_ptr<extensions::ContextMenuMatcher> extension_items =
      std::make_unique<extensions::ContextMenuMatcher>(
          profile_.get(), nullptr, menu_model.get(),
          base::BindRepeating(MenuItemHasAnyContext));

  std::u16string printable_selection_text;
  int index = 0;

  // Add the items associated with the test extension.
  extension_items->AppendExtensionItems(MenuItem::ExtensionKey(extension->id()),
                                        printable_selection_text, &index,
                                        false);
  // Verify both parent and child are hidden.
  EXPECT_FALSE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(parent_index)));
  EXPECT_FALSE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(child_index)));
}

// Tests appending an extension item with a visible submenu.
TEST_F(ContextMenuMatcherTest, AppendExtensionItemsWithVisibleSubmenu) {
  Extension* extension = AddExtension("test");

  // Add a parent item, with a visible child item.
  std::unique_ptr<MenuItem> parent = CreateTestItem(extension, "parent", true);
  MenuItem::Id parent_id = parent->id();
  int parent_index = 0;
  std::unique_ptr<MenuItem> child = CreateTestItem(extension, "child", true);
  int child_index = 1;
  ASSERT_TRUE(manager_->AddContextItem(extension, std::move(parent)));
  manager_->AddChildItem(parent_id, std::move(child));

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);

  std::unique_ptr<extensions::ContextMenuMatcher> extension_items =
      std::make_unique<extensions::ContextMenuMatcher>(
          profile_.get(), nullptr, menu_model.get(),
          base::BindRepeating(MenuItemHasAnyContext));

  // Add the items associated with the test extension.
  std::u16string printable_selection_text;
  int index = 0;
  extension_items->AppendExtensionItems(MenuItem::ExtensionKey(extension->id()),
                                        printable_selection_text, &index,
                                        false);

  // Verify both parent and child are visible.
  EXPECT_TRUE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(parent_index)));
  EXPECT_TRUE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(child_index)));
}

TEST_F(ContextMenuMatcherTest, AppendExtensionItemsGroupTitle) {
  Extension* extension = AddExtension("test");

  // Add a parent item, with a visible child item.
  std::unique_ptr<MenuItem> parent = CreateTestItem(extension, "parent", true);
  MenuItem::Id parent_id = parent->id();
  int parent_index = 0;
  std::unique_ptr<MenuItem> child = CreateTestItem(extension, "child", true);
  int child_index = 1;
  ASSERT_TRUE(manager_->AddContextItem(extension, std::move(parent)));
  manager_->AddChildItem(parent_id, std::move(child));

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);

  std::unique_ptr<extensions::ContextMenuMatcher> extension_items =
      std::make_unique<extensions::ContextMenuMatcher>(
          profile_.get(), nullptr, menu_model.get(),
          base::BindRepeating(MenuItemHasAnyContext));

  // Add the items associated with the test extension.
  int index = 0;
  std::u16string group_title = base::UTF8ToUTF16(extension->name());
  extension_items->AppendExtensionItems(MenuItem::ExtensionKey(extension->id()),
                                        std::u16string(), &index, false,
                                        group_title);

  // Verify both parent and child are visible.
  EXPECT_TRUE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(parent_index)));
  EXPECT_TRUE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(child_index)));
  EXPECT_EQ(menu_model->GetLabelAt(0), group_title);
}

TEST_F(ContextMenuMatcherTest,
       AppendExtensionItemsGroupTitleWithNullExtension) {
  static constexpr int kFakeWebViewEmbedderPid = 1;
  static constexpr int kFakeWebViewEmbedderFrameId = 1;
  static constexpr int kFakeWebViewInstanceId = 1;
  // Add a parent item, with a visible child item.
  std::unique_ptr<MenuItem> parent =
      CreateTestItem(/*extension=*/nullptr, kFakeWebViewEmbedderPid,
                     kFakeWebViewEmbedderFrameId, kFakeWebViewInstanceId,
                     "parent", /*visible=*/true);
  MenuItem::Id parent_id = parent->id();
  manager_->SetMenuIconLoader(parent->id().extension_key,
                              std::make_unique<TestExtensionMenuIconLoader>());

  int parent_index = 0;
  std::unique_ptr<MenuItem> child =
      CreateTestItem(/*extension=*/nullptr, kFakeWebViewEmbedderPid,
                     kFakeWebViewEmbedderFrameId, kFakeWebViewInstanceId,
                     "child", /*visible=*/true);
  int child_index = 1;
  ASSERT_TRUE(
      manager_->AddContextItem(/*extension=*/nullptr, std::move(parent)));
  manager_->AddChildItem(parent_id, std::move(child));

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);

  std::unique_ptr<extensions::ContextMenuMatcher> extension_items =
      std::make_unique<extensions::ContextMenuMatcher>(
          profile_.get(), nullptr, menu_model.get(),
          base::BindRepeating(MenuItemHasAnyContext));

  // Add the items associated with the test extension.
  int index = 0;
  std::u16string group_title = u"test";
  extension_items->AppendExtensionItems(
      MenuItem::ExtensionKey(/*extension_id=*/"", kFakeWebViewEmbedderPid,
                             kFakeWebViewEmbedderFrameId,
                             kFakeWebViewInstanceId),
      /*selection_text=*/u"test", &index, /*is_action_menu=*/false,
      group_title);

  // Verify both parent and child are visible.
  EXPECT_TRUE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(parent_index)));
  EXPECT_TRUE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(child_index)));
  EXPECT_EQ(menu_model->GetLabelAt(0), group_title);
}

// Tests appending a visible extension item with an invisible child.
// The child has an invisible submenu.
TEST_F(ContextMenuMatcherTest, AppendExtensionItemWithInvisibleSubmenu) {
  Extension* extension = AddExtension("test");

  // Add a visible parent item, with an invisible child item.
  std::unique_ptr<MenuItem> parent = CreateTestItem(extension, "parent", true);
  MenuItem::Id parent_id = parent->id();
  int parent_index = 0;
  std::unique_ptr<MenuItem> child1 = CreateTestItem(extension, "child1", false);
  int child1_index = 1;
  MenuItem::Id child1_id = child1->id();
  ASSERT_TRUE(manager_->AddContextItem(extension, std::move(parent)));
  manager_->AddChildItem(parent_id, std::move(child1));

  // Add two invisible item, child2 and child3, and make them child1's submenu
  // items
  std::unique_ptr<MenuItem> child2 = CreateTestItem(extension, "child2", false);
  int child2_index = 2;
  manager_->AddChildItem(child1_id, std::move(child2));

  std::unique_ptr<MenuItem> child3 = CreateTestItem(extension, "child3", false);
  int child3_index = 3;
  manager_->AddChildItem(child1_id, std::move(child3));

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);

  std::unique_ptr<extensions::ContextMenuMatcher> extension_items =
      std::make_unique<extensions::ContextMenuMatcher>(
          profile_.get(), nullptr, menu_model.get(),
          base::BindRepeating(MenuItemHasAnyContext));

  // Add the items associated with the test extension.
  std::u16string printable_selection_text;
  int index = 0;
  extension_items->AppendExtensionItems(MenuItem::ExtensionKey(extension->id()),
                                        printable_selection_text, &index,
                                        false);

  // Verify parent is visible.
  EXPECT_TRUE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(parent_index)));
  // Verify child1 and its submenu are all invisible.
  EXPECT_FALSE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(child1_index)));
  EXPECT_FALSE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(child2_index)));
  EXPECT_FALSE(extension_items->IsCommandIdVisible(
      extension_items->ConvertToExtensionsCustomCommandId(child3_index)));
}

TEST_F(ContextMenuMatcherTest, GetIconFromMenuIconLoader) {
  Extension* extension = AddExtension("test");

  std::unique_ptr<MenuItem> item =
      CreateTestItem(extension, "id", /*visible=*/true);
  MenuItem::Id item_id = item->id();
  auto menu_icon_loader = std::make_unique<TestExtensionMenuIconLoader>();
  TestExtensionMenuIconLoader* extension_menu_icon_loader =
      menu_icon_loader.get();

  manager_->SetMenuIconLoader(item_id.extension_key,
                              std::move(menu_icon_loader));
  manager_->AddContextItem(extension, std::move(item));
  EXPECT_EQ(1, extension_menu_icon_loader->load_icon_calls());

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(/*delegate=*/nullptr);
  auto extension_items = std::make_unique<extensions::ContextMenuMatcher>(
      profile_.get(), /*delegate=*/nullptr, menu_model.get(),
      base::BindRepeating(MenuItemHasAnyContext));

  // Add the items associated with the test extension.
  int index = 0;
  extension_items->AppendExtensionItems(MenuItem::ExtensionKey(extension->id()),
                                        std::u16string(), &index,
                                        /*is_action_menu=*/false);
  EXPECT_EQ(1, extension_menu_icon_loader->get_icon_calls());
}

}  // namespace extensions
