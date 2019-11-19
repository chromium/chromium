// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/menu_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::DeleteArg;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;

namespace extensions {

namespace context_menus = api::context_menus;

// Base class for tests.
class MenuManagerTest : public testing::Test {
 public:
  MenuManagerTest()
      : profile_(new TestingProfile()),
        manager_(profile_.get(),
                 ExtensionSystem::Get(profile_.get())->state_store()),
        prefs_(base::ThreadTaskRunnerHandle::Get()),
        next_id_(1) {}

  void TearDown() override {
    prefs_.pref_service()->CommitPendingWrite();
    base::RunLoop().RunUntilIdle();
  }

  // Returns a test item.
  std::unique_ptr<MenuItem> CreateTestItem(Extension* extension,
                                           bool incognito = false) {
    MenuItem::Type type = MenuItem::NORMAL;
    MenuItem::ContextList contexts(MenuItem::ALL);
    const MenuItem::ExtensionKey key(extension->id());
    MenuItem::Id id(incognito, key);
    id.uid = next_id_++;
    return std::make_unique<MenuItem>(id, "test", false, true, true, type,
                                      contexts);
  }

  // Returns a test item with the given string ID.
  std::unique_ptr<MenuItem> CreateTestItemWithID(Extension* extension,
                                                 const std::string& string_id) {
    MenuItem::Type type = MenuItem::NORMAL;
    MenuItem::ContextList contexts(MenuItem::ALL);
    const MenuItem::ExtensionKey key(extension->id());
    MenuItem::Id id(false, key);
    id.string_uid = string_id;
    return std::make_unique<MenuItem>(id, "test", false, true, true, type,
                                      contexts);
  }

  // Creates and returns a test Extension. The caller does *not* own the return
  // value.
  Extension* AddExtension(const std::string& name) {
    scoped_refptr<Extension> extension = prefs_.AddExtension(name);
    extensions_.push_back(extension);
    return extension.get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  MenuManager manager_;
  ExtensionList extensions_;
  TestExtensionPrefs prefs_;
  int next_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuManagerTest);
};

// Tests adding, getting, and removing items.
TEST_F(MenuManagerTest, AddGetRemoveItems) {
  Extension* extension = AddExtension("test");

  // Add a new item, make sure you can get it back.
  std::unique_ptr<MenuItem> item1 = CreateTestItem(extension);
  ASSERT_TRUE(item1 != NULL);
  MenuItem* item1_ptr = item1.get();
  ASSERT_TRUE(manager_.AddContextItem(extension, std::move(item1)));
  ASSERT_EQ(item1_ptr, manager_.GetItemById(item1_ptr->id()));
  const MenuItem::OwnedList* items =
      manager_.MenuItems(item1_ptr->id().extension_key);
  ASSERT_EQ(1u, items->size());
  ASSERT_EQ(item1_ptr, items->at(0).get());

  // Add a second item, make sure it comes back too.
  std::unique_ptr<MenuItem> item2 = CreateTestItemWithID(extension, "id2");
  MenuItem* item2_ptr = item2.get();
  ASSERT_TRUE(manager_.AddContextItem(extension, std::move(item2)));
  ASSERT_EQ(item2_ptr, manager_.GetItemById(item2_ptr->id()));
  items = manager_.MenuItems(item2_ptr->id().extension_key);
  ASSERT_EQ(2u, items->size());
  ASSERT_EQ(item1_ptr, items->at(0).get());
  ASSERT_EQ(item2_ptr, items->at(1).get());

  // Try adding item 3, then removing it.
  std::unique_ptr<MenuItem> item3 = CreateTestItem(extension);
  MenuItem* item3_ptr = item3.get();
  MenuItem::Id id3 = item3_ptr->id();
  const MenuItem::ExtensionKey extension_key3(item3_ptr->id().extension_key);
  ASSERT_TRUE(manager_.AddContextItem(extension, std::move(item3)));
  ASSERT_EQ(item3_ptr, manager_.GetItemById(id3));
  ASSERT_EQ(3u, manager_.MenuItems(extension_key3)->size());
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id3));
  ASSERT_EQ(NULL, manager_.GetItemById(id3));
  ASSERT_EQ(2u, manager_.MenuItems(extension_key3)->size());

  // Make sure removing a non-existent item returns false.
  const MenuItem::ExtensionKey key(extension->id());
  MenuItem::Id id(false, key);
  id.uid = id3.uid + 50;
  ASSERT_FALSE(manager_.RemoveContextMenuItem(id));

  // Make sure adding an item with the same string ID returns false.
  std::unique_ptr<MenuItem> item2too = CreateTestItemWithID(extension, "id2");
  ASSERT_FALSE(manager_.AddContextItem(extension, std::move(item2too)));

  // But the same string ID should not collide with another extension.
  Extension* extension2 = AddExtension("test2");
  std::unique_ptr<MenuItem> item2other =
      CreateTestItemWithID(extension2, "id2");
  ASSERT_TRUE(manager_.AddContextItem(extension2, std::move(item2other)));
}

// Test adding/removing child items.
TEST_F(MenuManagerTest, ChildFunctions) {
  Extension* extension1 = AddExtension("1111");
  Extension* extension2 = AddExtension("2222");
  Extension* extension3 = AddExtension("3333");

  std::unique_ptr<MenuItem> item1 = CreateTestItem(extension1);
  MenuItem* item1_ptr = item1.get();
  std::unique_ptr<MenuItem> item2 = CreateTestItem(extension2);
  MenuItem* item2_ptr = item2.get();
  std::unique_ptr<MenuItem> item2_child =
      CreateTestItemWithID(extension2, "2child");
  MenuItem* item2_child_ptr = item2_child.get();
  std::unique_ptr<MenuItem> item2_grandchild = CreateTestItem(extension2);
  std::unique_ptr<MenuItem> item3 = CreateTestItem(extension3);

  // Add in the first two items.
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item1)));
  ASSERT_TRUE(manager_.AddContextItem(extension2, std::move(item2)));

  MenuItem::Id id1 = item1_ptr->id();
  MenuItem::Id id2 = item2_ptr->id();

  // Try adding item3 as a child of item2 - this should fail because item3 has
  // a different extension id.
  ASSERT_FALSE(manager_.AddChildItem(id2, std::move(item3)));

  // Add item2_child as a child of item2.
  MenuItem::Id id2_child = item2_child->id();
  ASSERT_TRUE(manager_.AddChildItem(id2, std::move(item2_child)));
  ASSERT_EQ(1u, item2_ptr->children().size());
  ASSERT_EQ(0u, item1_ptr->children().size());
  ASSERT_EQ(item2_child_ptr, manager_.GetItemById(id2_child));

  ASSERT_EQ(1u, manager_.MenuItems(item1_ptr->id().extension_key)->size());
  ASSERT_EQ(item1_ptr,
            manager_.MenuItems(item1_ptr->id().extension_key)->at(0).get());

  // Add item2_grandchild as a child of item2_child, then remove it.
  MenuItem::Id id2_grandchild = item2_grandchild->id();
  ASSERT_TRUE(manager_.AddChildItem(id2_child, std::move(item2_grandchild)));
  ASSERT_EQ(1u, item2_ptr->children().size());
  ASSERT_EQ(1u, item2_child_ptr->children().size());
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id2_grandchild));

  // We should only get 1 thing back when asking for item2's extension id, since
  // it has a child item.
  ASSERT_EQ(1u, manager_.MenuItems(item2_ptr->id().extension_key)->size());
  ASSERT_EQ(item2_ptr,
            manager_.MenuItems(item2_ptr->id().extension_key)->at(0).get());

  // Remove child2_item.
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id2_child));
  ASSERT_EQ(1u, manager_.MenuItems(item2_ptr->id().extension_key)->size());
  ASSERT_EQ(item2_ptr,
            manager_.MenuItems(item2_ptr->id().extension_key)->at(0).get());
  ASSERT_EQ(0u, item2_ptr->children().size());
}

TEST_F(MenuManagerTest, PopulateFromValue) {
  Extension* extension = AddExtension("test");

  bool incognito = true;
  int type = MenuItem::CHECKBOX;
  std::string title("TITLE");
  bool checked = true;
  bool visible = true;
  bool enabled = true;
  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::SELECTION);
  int contexts_value = 0;
  ASSERT_TRUE(contexts.ToValue()->GetAsInteger(&contexts_value));

  auto document_url_patterns = std::make_unique<base::ListValue>();
  document_url_patterns->AppendString("http://www.google.com/*");
  document_url_patterns->AppendString("http://www.reddit.com/*");

  auto target_url_patterns = std::make_unique<base::ListValue>();
  target_url_patterns->AppendString("http://www.yahoo.com/*");
  target_url_patterns->AppendString("http://www.facebook.com/*");

  base::DictionaryValue value;
  value.SetBoolean("incognito", incognito);
  value.SetString("string_uid", std::string());
  value.SetInteger("type", type);
  value.SetString("title", title);
  value.SetBoolean("checked", checked);
  value.SetBoolean("visible", visible);
  value.SetBoolean("enabled", enabled);
  value.SetInteger("contexts", contexts_value);
  std::string error;
  URLPatternSet document_url_pattern_set;
  document_url_pattern_set.Populate(*document_url_patterns,
                                    URLPattern::SCHEME_ALL, true, &error);
  value.Set("document_url_patterns", std::move(document_url_patterns));
  URLPatternSet target_url_pattern_set;
  target_url_pattern_set.Populate(*target_url_patterns, URLPattern::SCHEME_ALL,
                                  true, &error);
  value.Set("target_url_patterns", std::move(target_url_patterns));

  std::unique_ptr<MenuItem> item(
      MenuItem::Populate(extension->id(), value, &error));
  ASSERT_TRUE(item.get());

  EXPECT_EQ(extension->id(), item->extension_id());
  EXPECT_EQ(incognito, item->incognito());
  EXPECT_EQ(title, item->title());
  EXPECT_EQ(checked, item->checked());
  EXPECT_EQ(item->checked(), item->checked());
  EXPECT_EQ(visible, item->visible());
  EXPECT_EQ(enabled, item->enabled());
  EXPECT_EQ(contexts, item->contexts());

  EXPECT_EQ(document_url_pattern_set, item->document_url_patterns());

  EXPECT_EQ(target_url_pattern_set, item->target_url_patterns());
}

// Tests that deleting a parent properly removes descendants.
TEST_F(MenuManagerTest, DeleteParent) {
  Extension* extension = AddExtension("1111");

  // Set up 5 items to add.
  std::unique_ptr<MenuItem> item1 = CreateTestItem(extension);
  std::unique_ptr<MenuItem> item2 = CreateTestItem(extension);
  std::unique_ptr<MenuItem> item3 = CreateTestItemWithID(extension, "id3");
  std::unique_ptr<MenuItem> item4 = CreateTestItemWithID(extension, "id4");
  std::unique_ptr<MenuItem> item5 = CreateTestItem(extension);
  std::unique_ptr<MenuItem> item6 = CreateTestItem(extension);
  MenuItem* item1_ptr = item1.get();
  MenuItem* item2_ptr = item2.get();
  MenuItem* item3_ptr = item3.get();
  MenuItem* item4_ptr = item4.get();
  MenuItem* item5_ptr = item5.get();
  MenuItem* item6_ptr = item6.get();
  MenuItem::Id item1_id = item1->id();
  MenuItem::Id item2_id = item2->id();
  MenuItem::Id item3_id = item3->id();
  MenuItem::Id item4_id = item4->id();
  MenuItem::Id item5_id = item5->id();
  MenuItem::Id item6_id = item6->id();
  const MenuItem::ExtensionKey key(extension->id());

  // Add the items in the hierarchy
  // item1 -> item2 -> item3 -> item4 -> item5 -> item6.
  ASSERT_TRUE(manager_.AddContextItem(extension, std::move(item1)));
  ASSERT_TRUE(manager_.AddChildItem(item1_id, std::move(item2)));
  ASSERT_TRUE(manager_.AddChildItem(item2_id, std::move(item3)));
  ASSERT_TRUE(manager_.AddChildItem(item3_id, std::move(item4)));
  ASSERT_TRUE(manager_.AddChildItem(item4_id, std::move(item5)));
  ASSERT_TRUE(manager_.AddChildItem(item5_id, std::move(item6)));
  ASSERT_EQ(item1_ptr, manager_.GetItemById(item1_id));
  ASSERT_EQ(item2_ptr, manager_.GetItemById(item2_id));
  ASSERT_EQ(item3_ptr, manager_.GetItemById(item3_id));
  ASSERT_EQ(item4_ptr, manager_.GetItemById(item4_id));
  ASSERT_EQ(item5_ptr, manager_.GetItemById(item5_id));
  ASSERT_EQ(item6_ptr, manager_.GetItemById(item6_id));
  ASSERT_EQ(1u, manager_.MenuItems(key)->size());
  ASSERT_EQ(6u, manager_.items_by_id_.size());

  // Remove item6 (a leaf node).
  ASSERT_TRUE(manager_.RemoveContextMenuItem(item6_id));
  ASSERT_EQ(item1_ptr, manager_.GetItemById(item1_id));
  ASSERT_EQ(item2_ptr, manager_.GetItemById(item2_id));
  ASSERT_EQ(item3_ptr, manager_.GetItemById(item3_id));
  ASSERT_EQ(item4_ptr, manager_.GetItemById(item4_id));
  ASSERT_EQ(item5_ptr, manager_.GetItemById(item5_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item6_id));
  ASSERT_EQ(1u, manager_.MenuItems(key)->size());
  ASSERT_EQ(5u, manager_.items_by_id_.size());

  // Remove item4 and make sure item5 is gone as well.
  ASSERT_TRUE(manager_.RemoveContextMenuItem(item4_id));
  ASSERT_EQ(item1_ptr, manager_.GetItemById(item1_id));
  ASSERT_EQ(item2_ptr, manager_.GetItemById(item2_id));
  ASSERT_EQ(item3_ptr, manager_.GetItemById(item3_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item4_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item5_id));
  ASSERT_EQ(1u, manager_.MenuItems(key)->size());
  ASSERT_EQ(3u, manager_.items_by_id_.size());

  // Now remove item1 and make sure item2 and item3 are gone as well.
  ASSERT_TRUE(manager_.RemoveContextMenuItem(item1_id));
  ASSERT_EQ(NULL, manager_.MenuItems(key));
  ASSERT_EQ(0u, manager_.items_by_id_.size());
  ASSERT_EQ(NULL, manager_.GetItemById(item1_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item2_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item3_id));
}

// Tests changing parents.
TEST_F(MenuManagerTest, ChangeParent) {
  Extension* extension1 = AddExtension("1111");

  // First create two items and add them both to the manager.
  std::unique_ptr<MenuItem> item1 = CreateTestItem(extension1);
  std::unique_ptr<MenuItem> item2 = CreateTestItem(extension1);
  MenuItem* item1_ptr = item1.get();
  MenuItem* item2_ptr = item2.get();

  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item1)));
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item2)));

  const MenuItem::OwnedList* items =
      manager_.MenuItems(item1_ptr->id().extension_key);
  ASSERT_EQ(2u, items->size());
  ASSERT_EQ(item1_ptr, items->at(0).get());
  ASSERT_EQ(item2_ptr, items->at(1).get());

  // Now create a third item, initially add it as a child of item1, then move
  // it to be a child of item2.
  std::unique_ptr<MenuItem> item3 = CreateTestItem(extension1);
  MenuItem* item3_ptr = item3.get();

  ASSERT_TRUE(manager_.AddChildItem(item1_ptr->id(), std::move(item3)));
  ASSERT_EQ(1u, item1_ptr->children().size());
  ASSERT_EQ(item3_ptr, item1_ptr->children()[0].get());

  ASSERT_TRUE(manager_.ChangeParent(item3_ptr->id(), &item2_ptr->id()));
  ASSERT_EQ(0u, item1_ptr->children().size());
  ASSERT_EQ(1u, item2_ptr->children().size());
  ASSERT_EQ(item3_ptr, item2_ptr->children()[0].get());

  // Move item2 to be a child of item1.
  ASSERT_TRUE(manager_.ChangeParent(item2_ptr->id(), &item1_ptr->id()));
  ASSERT_EQ(1u, item1_ptr->children().size());
  ASSERT_EQ(item2_ptr, item1_ptr->children()[0].get());
  ASSERT_EQ(1u, item2_ptr->children().size());
  ASSERT_EQ(item3_ptr, item2_ptr->children()[0].get());

  // Since item2 was a top-level item but is no longer, we should only have 1
  // top-level item.
  items = manager_.MenuItems(item1_ptr->id().extension_key);
  ASSERT_EQ(1u, items->size());
  ASSERT_EQ(item1_ptr, items->at(0).get());

  // Move item3 back to being a child of item1, so it's now a sibling of item2.
  ASSERT_TRUE(manager_.ChangeParent(item3_ptr->id(), &item1_ptr->id()));
  ASSERT_EQ(2u, item1_ptr->children().size());
  ASSERT_EQ(item2_ptr, item1_ptr->children()[0].get());
  ASSERT_EQ(item3_ptr, item1_ptr->children()[1].get());

  // Try switching item3 to be the parent of item1 - this should fail.
  ASSERT_FALSE(manager_.ChangeParent(item1_ptr->id(), &item3_ptr->id()));
  ASSERT_EQ(0u, item3_ptr->children().size());
  ASSERT_EQ(2u, item1_ptr->children().size());
  ASSERT_EQ(item2_ptr, item1_ptr->children()[0].get());
  ASSERT_EQ(item3_ptr, item1_ptr->children()[1].get());
  items = manager_.MenuItems(item1_ptr->id().extension_key);
  ASSERT_EQ(1u, items->size());
  ASSERT_EQ(item1_ptr, items->at(0).get());

  // Move item2 to be a top-level item.
  ASSERT_TRUE(manager_.ChangeParent(item2_ptr->id(), NULL));
  items = manager_.MenuItems(item1_ptr->id().extension_key);
  ASSERT_EQ(2u, items->size());
  ASSERT_EQ(item1_ptr, items->at(0).get());
  ASSERT_EQ(item2_ptr, items->at(1).get());
  ASSERT_EQ(1u, item1_ptr->children().size());
  ASSERT_EQ(item3_ptr, item1_ptr->children()[0].get());

  // Make sure you can't move a node to be a child of another extension's item.
  Extension* extension2 = AddExtension("2222");
  std::unique_ptr<MenuItem> item4 = CreateTestItem(extension2);
  MenuItem* item4_ptr = item4.get();
  ASSERT_TRUE(manager_.AddContextItem(extension2, std::move(item4)));
  ASSERT_FALSE(manager_.ChangeParent(item4_ptr->id(), &item1_ptr->id()));
  ASSERT_FALSE(manager_.ChangeParent(item1_ptr->id(), &item4_ptr->id()));

  // Make sure you can't make an item be its own parent.
  ASSERT_FALSE(manager_.ChangeParent(item1_ptr->id(), &item1_ptr->id()));
}

// Tests that we properly remove an extension's menu item when that extension is
// unloaded.
TEST_F(MenuManagerTest, ExtensionUnloadRemovesMenuItems) {
  content::NotificationService* notifier =
      content::NotificationService::current();
  ASSERT_TRUE(notifier != NULL);

  // Create a test extension.
  Extension* extension1 = AddExtension("1111");

  // Create an MenuItem and put it into the manager.
  std::unique_ptr<MenuItem> item1 = CreateTestItem(extension1);
  MenuItem* item1_ptr = item1.get();
  MenuItem::Id id1 = item1->id();
  ASSERT_EQ(extension1->id(), item1->extension_id());
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item1)));
  ASSERT_EQ(
      1u, manager_.MenuItems(MenuItem::ExtensionKey(extension1->id()))->size());

  // Create a menu item with a different extension id and add it to the manager.
  Extension* extension2 = AddExtension("2222");
  std::unique_ptr<MenuItem> item2 = CreateTestItem(extension2);
  MenuItem* item2_ptr = item2.get();
  ASSERT_NE(item1_ptr->extension_id(), item2->extension_id());
  ASSERT_TRUE(manager_.AddContextItem(extension2, std::move(item2)));

  // Notify that the extension was unloaded, and make sure the right item is
  // gone.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_.get());
  registry->TriggerOnUnloaded(extension1, UnloadedExtensionReason::DISABLE);

  ASSERT_EQ(NULL, manager_.MenuItems(MenuItem::ExtensionKey(extension1->id())));
  ASSERT_EQ(
      1u, manager_.MenuItems(MenuItem::ExtensionKey(extension2->id()))->size());
  ASSERT_TRUE(manager_.GetItemById(id1) == NULL);
  ASSERT_TRUE(manager_.GetItemById(item2_ptr->id()) != NULL);
}

namespace {

// A mock message service for tests of MenuManager::ExecuteCommand.
class MockEventRouter : public EventRouter {
 public:
  explicit MockEventRouter(Profile* profile) : EventRouter(profile, NULL) {}

  MOCK_METHOD6(DispatchEventToExtensionMock,
               void(const std::string& extension_id,
                    const std::string& event_name,
                    base::ListValue* event_args,
                    content::BrowserContext* source_context,
                    const GURL& event_url,
                    EventRouter::UserGestureState state));

  void DispatchEventToExtension(const std::string& extension_id,
                                std::unique_ptr<Event> event) override {
    DispatchEventToExtensionMock(extension_id,
                                 event->event_name,
                                 event->event_args.release(),
                                 event->restrict_to_browser_context,
                                 event->event_url,
                                 event->user_gesture);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventRouter);
};

// MockEventRouter factory function
std::unique_ptr<KeyedService> MockEventRouterFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<MockEventRouter>(static_cast<Profile*>(context));
}

}  // namespace

// Tests the RemoveAll functionality.
TEST_F(MenuManagerTest, RemoveAll) {
  // Try removing all items for an extension id that doesn't have any items.
  manager_.RemoveAllContextItems(MenuItem::ExtensionKey("CCCC"));

  // Add 2 top-level and one child item for extension 1.
  Extension* extension1 = AddExtension("1111");
  std::unique_ptr<MenuItem> item1 = CreateTestItem(extension1);
  std::unique_ptr<MenuItem> item2 = CreateTestItem(extension1);
  std::unique_ptr<MenuItem> item3 = CreateTestItem(extension1);
  MenuItem* item1_ptr = item1.get();
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item1)));
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item2)));
  ASSERT_TRUE(manager_.AddChildItem(item1_ptr->id(), std::move(item3)));

  // Add one top-level item for extension 2.
  Extension* extension2 = AddExtension("2222");
  std::unique_ptr<MenuItem> item4 = CreateTestItem(extension2);
  ASSERT_TRUE(manager_.AddContextItem(extension2, std::move(item4)));

  const MenuItem::ExtensionKey key1(extension1->id());
  const MenuItem::ExtensionKey key2(extension2->id());
  EXPECT_EQ(2u, manager_.MenuItems(key1)->size());
  EXPECT_EQ(1u, manager_.MenuItems(key2)->size());

  // Remove extension2's item.
  manager_.RemoveAllContextItems(key2);
  EXPECT_EQ(2u, manager_.MenuItems(key1)->size());
  EXPECT_EQ(NULL, manager_.MenuItems(key2));

  // Remove extension1's items.
  manager_.RemoveAllContextItems(key1);
  EXPECT_EQ(NULL, manager_.MenuItems(key1));
}

// Tests that removing all items one-by-one doesn't leave an entry around.
TEST_F(MenuManagerTest, RemoveOneByOne) {
  // Add 2 test items.
  Extension* extension1 = AddExtension("1111");
  std::unique_ptr<MenuItem> item1 = CreateTestItem(extension1);
  std::unique_ptr<MenuItem> item2 = CreateTestItem(extension1);
  std::unique_ptr<MenuItem> item3 = CreateTestItemWithID(extension1, "id3");
  MenuItem::Id item1_id = item1->id();
  MenuItem::Id item2_id = item2->id();
  MenuItem::Id item3_id = item3->id();
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item1)));
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item2)));
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item3)));

  ASSERT_FALSE(manager_.context_items_.empty());

  manager_.RemoveContextMenuItem(item3_id);
  manager_.RemoveContextMenuItem(item1_id);
  manager_.RemoveContextMenuItem(item2_id);

  ASSERT_TRUE(manager_.context_items_.empty());
}

TEST_F(MenuManagerTest, ExecuteCommand) {
  TestingProfile profile;
  MockEventRouter* mock_event_router = static_cast<MockEventRouter*>(
      EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile, base::BindRepeating(&MockEventRouterFactoryFunction)));

  content::ContextMenuParams params;
  params.media_type = blink::ContextMenuDataMediaType::kImage;
  params.src_url = GURL("http://foo.bar/image.png");
  params.page_url = GURL("http://foo.bar");
  params.selection_text = base::ASCIIToUTF16("Hello World");
  params.is_editable = false;

  Extension* extension = AddExtension("test");
  std::unique_ptr<MenuItem> parent = CreateTestItem(extension);
  std::unique_ptr<MenuItem> item = CreateTestItem(extension);
  MenuItem* item_ptr = item.get();
  MenuItem::Id parent_id = parent->id();
  MenuItem::Id id = item->id();
  ASSERT_TRUE(manager_.AddContextItem(extension, std::move(parent)));
  ASSERT_TRUE(manager_.AddChildItem(parent_id, std::move(item)));

  // Use the magic of googlemock to save a parameter to our mock's
  // DispatchEventToExtension method into event_args.
  base::ListValue* list = NULL;
  {
    InSequence s;
    EXPECT_CALL(*mock_event_router,
                DispatchEventToExtensionMock(
                    item_ptr->extension_id(), MenuManager::kOnContextMenus, _,
                    &profile, GURL(), EventRouter::USER_GESTURE_ENABLED))
        .Times(1)
        .WillOnce(SaveArg<2>(&list));
    EXPECT_CALL(
        *mock_event_router,
        DispatchEventToExtensionMock(
            item_ptr->extension_id(), context_menus::OnClicked::kEventName, _,
            &profile, GURL(), EventRouter::USER_GESTURE_ENABLED))
        .Times(1)
        .WillOnce(DeleteArg<2>());
  }
  manager_.ExecuteCommand(&profile, nullptr /* web_contents */,
                          nullptr /* render_frame_host */, params, id);

  ASSERT_EQ(2u, list->GetSize());

  base::DictionaryValue* info;
  ASSERT_TRUE(list->GetDictionary(0, &info));

  int tmp_id = 0;
  ASSERT_TRUE(info->GetInteger("menuItemId", &tmp_id));
  ASSERT_EQ(id.uid, tmp_id);
  ASSERT_TRUE(info->GetInteger("parentMenuItemId", &tmp_id));
  ASSERT_EQ(parent_id.uid, tmp_id);

  std::string tmp;
  ASSERT_TRUE(info->GetString("mediaType", &tmp));
  ASSERT_EQ("image", tmp);
  ASSERT_TRUE(info->GetString("srcUrl", &tmp));
  ASSERT_EQ(params.src_url.spec(), tmp);
  ASSERT_TRUE(info->GetString("pageUrl", &tmp));
  ASSERT_EQ(params.page_url.spec(), tmp);

  base::string16 tmp16;
  ASSERT_TRUE(info->GetString("selectionText", &tmp16));
  ASSERT_EQ(params.selection_text, tmp16);

  bool bool_tmp = true;
  ASSERT_TRUE(info->GetBoolean("editable", &bool_tmp));
  ASSERT_EQ(params.is_editable, bool_tmp);

  delete list;
}

// Test that there is always only one radio item selected.
TEST_F(MenuManagerTest, SanitizeRadioButtons) {
  Extension* extension = AddExtension("test");

  // A single unchecked item should get checked.
  std::unique_ptr<MenuItem> item1 = CreateTestItem(extension);
  MenuItem* item1_ptr = item1.get();

  item1_ptr->set_type(MenuItem::RADIO);
  item1_ptr->SetChecked(false);
  ASSERT_FALSE(item1_ptr->checked());
  manager_.AddContextItem(extension, std::move(item1));
  ASSERT_TRUE(item1_ptr->checked());

  // In a run of two unchecked items, the first should get selected.
  item1_ptr->SetChecked(false);
  std::unique_ptr<MenuItem> item2 = CreateTestItem(extension);
  MenuItem* item2_ptr = item2.get();
  item2_ptr->set_type(MenuItem::RADIO);
  item2_ptr->SetChecked(false);
  ASSERT_FALSE(item1_ptr->checked());
  ASSERT_FALSE(item2_ptr->checked());
  manager_.AddContextItem(extension, std::move(item2));
  ASSERT_TRUE(item1_ptr->checked());
  ASSERT_FALSE(item2_ptr->checked());

  // If multiple items are checked and one of the items is updated to be
  // checked, then all other items should be unchecked.
  //
  // Note, this case of multiple checked items (i.e. SetChecked() called more
  // than once) followed by a call to ItemUpdated() would never happen in
  // practice. In this hypothetical scenario, the item that was updated the
  // latest via ItemUpdated() should remain checked.
  //
  // Begin with two items checked.
  item1_ptr->SetChecked(true);
  item2_ptr->SetChecked(true);
  ASSERT_TRUE(item1_ptr->checked());
  ASSERT_TRUE(item2_ptr->checked());
  // Updating item1 to be checked should result in item2 being unchecked.
  manager_.ItemUpdated(item1_ptr->id());
  // Item 1 should be selected as it was updated the latest.
  ASSERT_TRUE(item1_ptr->checked());
  ASSERT_FALSE(item2_ptr->checked());

  // If the checked item is removed, the new first item should get checked.
  item1_ptr->SetChecked(false);
  item2_ptr->SetChecked(true);
  ASSERT_FALSE(item1_ptr->checked());
  ASSERT_TRUE(item2_ptr->checked());
  manager_.RemoveContextMenuItem(item2_ptr->id());
  item2_ptr = NULL;
  ASSERT_TRUE(item1_ptr->checked());

  // If a checked item is added to a run that already has a checked item,
  // then the new item should get checked.
  item1_ptr->SetChecked(true);
  std::unique_ptr<MenuItem> new_item = CreateTestItem(extension);
  MenuItem* new_item_ptr = new_item.get();
  new_item_ptr->set_type(MenuItem::RADIO);
  new_item_ptr->SetChecked(true);
  ASSERT_TRUE(item1_ptr->checked());
  ASSERT_TRUE(new_item_ptr->checked());
  manager_.AddContextItem(extension, std::move(new_item));
  ASSERT_FALSE(item1_ptr->checked());
  ASSERT_TRUE(new_item_ptr->checked());

  // Make sure that children are checked as well.
  std::unique_ptr<MenuItem> parent = CreateTestItem(extension);
  MenuItem* parent_ptr = parent.get();
  manager_.AddContextItem(extension, std::move(parent));
  std::unique_ptr<MenuItem> child1 = CreateTestItem(extension);
  MenuItem* child1_ptr = child1.get();
  child1_ptr->set_type(MenuItem::RADIO);
  child1_ptr->SetChecked(false);
  std::unique_ptr<MenuItem> child2 = CreateTestItem(extension);
  MenuItem* child2_ptr = child2.get();
  child2_ptr->set_type(MenuItem::RADIO);
  child2_ptr->SetChecked(true);
  ASSERT_FALSE(child1_ptr->checked());
  ASSERT_TRUE(child2_ptr->checked());

  manager_.AddChildItem(parent_ptr->id(), std::move(child1));
  ASSERT_TRUE(child1_ptr->checked());

  manager_.AddChildItem(parent_ptr->id(), std::move(child2));
  ASSERT_FALSE(child1_ptr->checked());
  ASSERT_TRUE(child2_ptr->checked());

  // Removing the checked item from the children should cause the
  // remaining child to be checked.
  manager_.RemoveContextMenuItem(child2_ptr->id());
  child2_ptr = NULL;
  ASSERT_TRUE(child1_ptr->checked());

  // This should NOT cause |new_item| to be deselected because
  // |parent| will be separating the two runs of radio items.
  manager_.ChangeParent(child1_ptr->id(), NULL);
  ASSERT_TRUE(new_item_ptr->checked());
  ASSERT_TRUE(child1_ptr->checked());

  // Removing |parent| should cause only |child1| to be selected.
  manager_.RemoveContextMenuItem(parent_ptr->id());
  parent_ptr = NULL;
  ASSERT_FALSE(new_item_ptr->checked());
  ASSERT_TRUE(child1_ptr->checked());
}

// If a context menu has multiple radio lists, then they should all be properly
// sanitized. More specifically, on initialization of the context menu, the
// first item of each list should be checked.
TEST_F(MenuManagerTest, SanitizeContextMenuWithMultipleRadioLists) {
  Extension* extension = AddExtension("test");

  // Create a radio list with two radio buttons.
  // Create first radio button.
  std::unique_ptr<MenuItem> radio1 = CreateTestItem(extension);
  MenuItem* radio1_ptr = radio1.get();
  radio1_ptr->set_type(MenuItem::RADIO);
  manager_.AddContextItem(extension, std::move(radio1));
  // Create second radio button.
  std::unique_ptr<MenuItem> radio2 = CreateTestItem(extension);
  MenuItem* radio2_ptr = radio2.get();
  radio2_ptr->set_type(MenuItem::RADIO);
  manager_.AddContextItem(extension, std::move(radio2));
  // Ensure that in the first radio list, only radio1 is checked.
  ASSERT_TRUE(radio1_ptr->checked());
  ASSERT_FALSE(radio2_ptr->checked());

  // Add a normal item to separate the first radio list from the second radio
  // list to created next.
  std::unique_ptr<MenuItem> normal_item1 = CreateTestItem(extension);
  normal_item1->set_type(MenuItem::NORMAL);
  manager_.AddContextItem(extension, std::move(normal_item1));

  // Create another radio list of two radio items.
  // Create first radio button.
  std::unique_ptr<MenuItem> radio3 = CreateTestItem(extension);
  MenuItem* radio3_ptr = radio3.get();
  radio3_ptr->set_type(MenuItem::RADIO);
  manager_.AddContextItem(extension, std::move(radio3));
  // Create second radio button.
  std::unique_ptr<MenuItem> radio4 = CreateTestItem(extension);
  MenuItem* radio4_ptr = radio4.get();
  radio4_ptr->set_type(MenuItem::RADIO);
  manager_.AddContextItem(extension, std::move(radio4));

  // Ensure that in the second radio list, only radio3 is checked.
  ASSERT_TRUE(radio3_ptr->checked());
  ASSERT_FALSE(radio4_ptr->checked());
}

// Tests the RemoveAllIncognitoContextItems functionality.
TEST_F(MenuManagerTest, RemoveAllIncognito) {
  Extension* extension1 = AddExtension("1111");
  // Add 2 top-level and one child item for extension 1
  // with incognito 'true'.
  std::unique_ptr<MenuItem> item1 = CreateTestItem(extension1, true);
  std::unique_ptr<MenuItem> item2 = CreateTestItem(extension1, true);
  std::unique_ptr<MenuItem> item3 = CreateTestItem(extension1, true);
  MenuItem::Id item1_id = item1->id();
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item1)));
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item2)));
  ASSERT_TRUE(manager_.AddChildItem(item1_id, std::move(item3)));

  // Add 2 top-level and one child item for extension 1
  // with incognito 'false'.
  std::unique_ptr<MenuItem> item4 = CreateTestItem(extension1);
  std::unique_ptr<MenuItem> item5 = CreateTestItem(extension1);
  std::unique_ptr<MenuItem> item6 = CreateTestItem(extension1);
  MenuItem::Id item4_id = item4->id();
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item4)));
  ASSERT_TRUE(manager_.AddContextItem(extension1, std::move(item5)));
  ASSERT_TRUE(manager_.AddChildItem(item4_id, std::move(item6)));

  // Add one top-level item for extension 2.
  Extension* extension2 = AddExtension("2222");
  std::unique_ptr<MenuItem> item7 = CreateTestItem(extension2);
  ASSERT_TRUE(manager_.AddContextItem(extension2, std::move(item7)));

  const MenuItem::ExtensionKey key1(extension1->id());
  const MenuItem::ExtensionKey key2(extension2->id());
  EXPECT_EQ(4u, manager_.MenuItems(key1)->size());
  EXPECT_EQ(1u, manager_.MenuItems(key2)->size());

  // Remove all context menu items with incognito true.
  manager_.RemoveAllIncognitoContextItems();
  EXPECT_EQ(2u, manager_.MenuItems(key1)->size());
  EXPECT_EQ(1u, manager_.MenuItems(key2)->size());
}

}  // namespace extensions
