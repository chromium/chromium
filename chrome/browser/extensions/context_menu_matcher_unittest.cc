// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/context_menu_matcher.h"

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gmock/include/gmock/gmock.h"

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

}  // namespace extensions
