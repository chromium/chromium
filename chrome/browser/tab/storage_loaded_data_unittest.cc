// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_loaded_data.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/token.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/tab_group_collection_state.pb.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/protocol/tab_strip_collection_state.pb.h"
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "components/tab_groups/tab_group_color.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace tabs {
namespace {

class TestRestoreEntityTracker : public RestoreEntityTracker {
 public:
  TestRestoreEntityTracker() = default;
  ~TestRestoreEntityTracker() override = default;

  void SetLoadingContext(StorageLoadingContext* context) override {}
  void RegisterCollection(StorageId storage_id,
                          TabStorageType type,
                          const tabs_pb::Children& children,
                          std::optional<base::Token> collection_specific_id,
                          base::PassKey<TabStateStorageDatabase>) override {
    std::vector<StorageId> ids;
    for (const auto& token_proto : children.storage_id()) {
      ids.push_back(StorageIdFromTokenProto(token_proto));
    }
    registered_collections_[storage_id] = std::move(ids);
  }
  void RegisterTab(StorageId storage_id,
                   const tabs_pb::TabState& tab_state,
                   base::PassKey<TabStateStorageDatabase>) override {}
  bool AssociateTab(const TabInterface* tab) override { return true; }
  bool AssociateCollection(const TabCollection* collection) override {
    return true;
  }
  bool HasTabBeenAssociated(const TabInterface* tab) override { return true; }
  bool HasCollectionBeenAssociated(TabCollection::Handle handle) override {
    return true;
  }
  bool HasNothingToAssociate() override { return false; }

  const std::vector<StorageId>& GetRegisteredChildren(StorageId id) const {
    static const std::vector<StorageId> kEmpty;
    auto it = registered_collections_.find(id);
    return it != registered_collections_.end() ? it->second : kEmpty;
  }

 private:
  absl::flat_hash_map<StorageId, std::vector<StorageId>>
      registered_collections_;
};

std::vector<uint8_t> SerializeChildren(const std::vector<StorageId>& ids) {
  tabs_pb::Children proto;
  for (const auto& id : ids) {
    StorageIdToTokenProto(id, proto.add_storage_id());
  }
  std::vector<uint8_t> bytes(proto.ByteSizeLong());
  proto.SerializeToArray(bytes.data(), bytes.size());
  return bytes;
}

std::vector<uint8_t> SerializeTabState(int id) {
  tabs_pb::TabState proto;
  proto.set_tab_id(id);
  std::vector<uint8_t> bytes(proto.ByteSizeLong());
  proto.SerializeToArray(bytes.data(), bytes.size());
  return bytes;
}

std::vector<uint8_t> SerializeTabStripState(
    std::optional<StorageId> active_id) {
  tabs_pb::TabStripCollectionState proto;
  if (active_id) {
    StorageIdToTokenProto(*active_id, proto.mutable_active_tab_storage_id());
  }
  std::vector<uint8_t> bytes(proto.ByteSizeLong());
  proto.SerializeToArray(bytes.data(), bytes.size());
  return bytes;
}

std::vector<uint8_t> SerializeGroupState(const base::Token& group_id,
                                         const std::string& title,
                                         int color) {
  tabs_pb::TabGroupCollectionState proto;
  proto.set_group_id_high(group_id.high());
  proto.set_group_id_low(group_id.low());
  proto.set_title(title);
  proto.set_color(color);
  proto.set_is_collapsed(false);
  std::vector<uint8_t> bytes(proto.ByteSizeLong());
  proto.SerializeToArray(bytes.data(), bytes.size());
  return bytes;
}

class StorageLoadedDataTest : public ::testing::Test {
 public:
  StorageLoadedDataTest() = default;
  ~StorageLoadedDataTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = std::make_unique<TabStateStorageDatabase>(
        temp_dir_.GetPath(), /*support_off_the_record_data=*/false);
    ASSERT_TRUE(db_->Initialize());
  }

  void TearDown() override { db_.reset(); }

  TabStateStorageDatabase* db() { return db_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TabStateStorageDatabase> db_;
};

// Tests loading of a standard canonical hierarchy (Root -> Unpinned -> Tabs).
TEST_F(StorageLoadedDataTest, CanonicalHierarchy_LoadedSuccessfully) {
  StorageId root_id = StorageId::Create();
  StorageId unpinned_id = StorageId::Create();
  StorageId tab1_id = StorageId::Create();
  StorageId tab2_id = StorageId::Create();
  StorageId tab3_id = StorageId::Create();

  auto* tx = db()->CreateTransaction();
  db()->SaveNode(tx, root_id, "win0", false, TabStorageType::kTabStrip,
                 SerializeTabStripState(tab2_id), /*children=*/{});
  db()->SaveNodeChildren(tx, root_id, SerializeChildren({unpinned_id}));

  db()->SaveNode(tx, unpinned_id, "win0", false, TabStorageType::kUnpinned, {},
                 /*children=*/{});
  db()->SaveNodeChildren(tx, unpinned_id,
                         SerializeChildren({tab1_id, tab2_id, tab3_id}));

  db()->SaveNode(tx, tab1_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(1), /*children=*/{});
  db()->SaveNode(tx, tab2_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(2), /*children=*/{});
  db()->SaveNode(tx, tab3_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(3), /*children=*/{});
  ASSERT_TRUE(db()->CloseTransaction(tx));

  auto builder = std::make_unique<StorageLoadedData::Builder>(
      "win0", false, std::make_unique<TestRestoreEntityTracker>());
  std::unique_ptr<StorageLoadedData> data =
      db()->LoadAllNodes("win0", false, std::move(builder));

  ASSERT_NE(data, nullptr);
  const auto& loaded_tabs = data->GetLoadedTabs();
  ASSERT_EQ(loaded_tabs.size(), 3u);
  EXPECT_EQ(loaded_tabs[0].tab_id(), 1);
  EXPECT_EQ(loaded_tabs[1].tab_id(), 2);
  EXPECT_EQ(loaded_tabs[2].tab_id(), 3);
  EXPECT_EQ(data->GetActiveTabIndex(), 1);
}

// Tests active tab index resolution when no active tab is configured.
TEST_F(StorageLoadedDataTest, ActiveTabResolution_NoActiveTab) {
  StorageId root_id = StorageId::Create();
  StorageId unpinned_id = StorageId::Create();
  StorageId tab1_id = StorageId::Create();

  auto* tx = db()->CreateTransaction();
  db()->SaveNode(tx, root_id, "win0", false, TabStorageType::kTabStrip,
                 SerializeTabStripState(std::nullopt), /*children=*/{});
  db()->SaveNodeChildren(tx, root_id, SerializeChildren({unpinned_id}));

  db()->SaveNode(tx, unpinned_id, "win0", false, TabStorageType::kUnpinned, {},
                 /*children=*/{});
  db()->SaveNodeChildren(tx, unpinned_id, SerializeChildren({tab1_id}));

  db()->SaveNode(tx, tab1_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(1), /*children=*/{});
  ASSERT_TRUE(db()->CloseTransaction(tx));

  auto builder = std::make_unique<StorageLoadedData::Builder>(
      "win0", false, std::make_unique<TestRestoreEntityTracker>());
  std::unique_ptr<StorageLoadedData> data =
      db()->LoadAllNodes("win0", false, std::move(builder));

  ASSERT_NE(data, nullptr);
  EXPECT_FALSE(data->GetActiveTabIndex().has_value());
}

// Tests active tab index resolution when the active tab is present.
TEST_F(StorageLoadedDataTest, ActiveTabResolution_ActiveTabPresent) {
  StorageId root_id = StorageId::Create();
  StorageId unpinned_id = StorageId::Create();
  StorageId tab1_id = StorageId::Create();
  StorageId tab2_id = StorageId::Create();

  auto* tx = db()->CreateTransaction();
  db()->SaveNode(tx, root_id, "win0", false, TabStorageType::kTabStrip,
                 SerializeTabStripState(tab2_id), /*children=*/{});
  db()->SaveNodeChildren(tx, root_id, SerializeChildren({unpinned_id}));

  db()->SaveNode(tx, unpinned_id, "win0", false, TabStorageType::kUnpinned, {},
                 /*children=*/{});
  db()->SaveNodeChildren(tx, unpinned_id,
                         SerializeChildren({tab1_id, tab2_id}));

  db()->SaveNode(tx, tab1_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(1), /*children=*/{});
  db()->SaveNode(tx, tab2_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(2), /*children=*/{});
  ASSERT_TRUE(db()->CloseTransaction(tx));

  auto builder = std::make_unique<StorageLoadedData::Builder>(
      "win0", false, std::make_unique<TestRestoreEntityTracker>());
  std::unique_ptr<StorageLoadedData> data =
      db()->LoadAllNodes("win0", false, std::move(builder));

  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data->GetActiveTabIndex(), 1);
}

// Tests deserialization of tab groups with titles and colors.
TEST_F(StorageLoadedDataTest, GroupVisualData_LoadedSuccessfully) {
  StorageId root_id = StorageId::Create();
  StorageId unpinned_id = StorageId::Create();
  StorageId group1_id = StorageId::Create();
  StorageId tab1_id = StorageId::Create();
  base::Token group_token = base::Token::CreateRandom();

  auto* tx = db()->CreateTransaction();
  db()->SaveNode(tx, root_id, "win0", false, TabStorageType::kTabStrip,
                 SerializeTabStripState(std::nullopt), /*children=*/{});
  db()->SaveNodeChildren(tx, root_id, SerializeChildren({unpinned_id}));

  db()->SaveNode(tx, unpinned_id, "win0", false, TabStorageType::kUnpinned, {},
                 /*children=*/{});
  db()->SaveNodeChildren(tx, unpinned_id, SerializeChildren({group1_id}));

  db()->SaveNode(tx, group1_id, "win0", false, TabStorageType::kGroup,
                 SerializeGroupState(group_token, "Work", 1), /*children=*/{});
  db()->SaveNodeChildren(tx, group1_id, SerializeChildren({tab1_id}));

  db()->SaveNode(tx, tab1_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(1), /*children=*/{});
  ASSERT_TRUE(db()->CloseTransaction(tx));

  auto builder = std::make_unique<StorageLoadedData::Builder>(
      "win0", false, std::make_unique<TestRestoreEntityTracker>());
  std::unique_ptr<StorageLoadedData> data =
      db()->LoadAllNodes("win0", false, std::move(builder));

  ASSERT_NE(data, nullptr);
  const auto& loaded_groups = data->GetLoadedGroups();
  ASSERT_EQ(loaded_groups.size(), 1u);
  EXPECT_EQ(loaded_groups[0]->tab_group_id_, group_token);
  EXPECT_EQ(loaded_groups[0]->title_, u"Work");
  EXPECT_EQ(loaded_groups[0]->color_,
            static_cast<tab_groups::TabGroupColorId>(1));
}

// Baseline reconciliation: when a newly opened tab is appended at the end of
// divergent children, reconciliation succeeds without error.
TEST_F(StorageLoadedDataTest,
       DivergentReconcile_NewTabAppended_PassesBaseline) {
  StorageId root_id = StorageId::Create();
  StorageId unpinned_id = StorageId::Create();
  StorageId tab1_id = StorageId::Create();
  StorageId tab2_id = StorageId::Create();
  StorageId tab3_id = StorageId::Create();

  auto* tx = db()->CreateTransaction();
  db()->SaveNode(tx, root_id, "win0", false, TabStorageType::kTabStrip,
                 SerializeTabStripState(std::nullopt), /*children=*/{});
  db()->SaveNodeChildren(tx, root_id, SerializeChildren({unpinned_id}));

  db()->SaveNode(tx, unpinned_id, "win0", false, TabStorageType::kUnpinned, {},
                 /*children=*/{});
  db()->SaveNodeChildren(tx, unpinned_id,
                         SerializeChildren({tab1_id, tab2_id}));

  db()->SaveNode(tx, tab1_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(1), /*children=*/{});
  db()->SaveNode(tx, tab2_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(2), /*children=*/{});
  db()->SaveNode(tx, tab3_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(3), /*children=*/{});

  // Tab 3 was appended to divergent children.
  db()->SaveDivergentNode(tx, unpinned_id, "win0", false,
                          SerializeChildren({tab1_id, tab2_id, tab3_id}));
  ASSERT_TRUE(db()->CloseTransaction(tx));

  auto builder = std::make_unique<StorageLoadedData::Builder>(
      "win0", false, std::make_unique<TestRestoreEntityTracker>());
  std::unique_ptr<StorageLoadedData> data =
      db()->LoadAllNodes("win0", false, std::move(builder));

  ASSERT_NE(data, nullptr);
  const auto& loaded_tabs = data->GetLoadedTabs();
  ASSERT_EQ(loaded_tabs.size(), 3u);
  EXPECT_EQ(loaded_tabs[0].tab_id(), 1);
  EXPECT_EQ(loaded_tabs[1].tab_id(), 2);
  EXPECT_EQ(loaded_tabs[2].tab_id(), 3);
}

// Baseline reconciliation: when a trailing canonical tab was deleted
// mid-restore.
TEST_F(StorageLoadedDataTest,
       DivergentReconcile_TrailingTabDeleted_PassesBaseline) {
  StorageId root_id = StorageId::Create();
  StorageId unpinned_id = StorageId::Create();
  StorageId tab1_id = StorageId::Create();
  StorageId tab2_id = StorageId::Create();
  StorageId tab3_id = StorageId::Create();

  auto* tx = db()->CreateTransaction();
  db()->SaveNode(tx, root_id, "win0", false, TabStorageType::kTabStrip,
                 SerializeTabStripState(std::nullopt), /*children=*/{});
  db()->SaveNodeChildren(tx, root_id, SerializeChildren({unpinned_id}));

  db()->SaveNode(tx, unpinned_id, "win0", false, TabStorageType::kUnpinned, {},
                 /*children=*/{});
  db()->SaveNodeChildren(tx, unpinned_id,
                         SerializeChildren({tab1_id, tab2_id, tab3_id}));

  db()->SaveNode(tx, tab1_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(1), /*children=*/{});
  db()->SaveNode(tx, tab2_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(2), /*children=*/{});
  // Tab 3 deleted from nodes table.

  db()->SaveDivergentNode(tx, unpinned_id, "win0", false,
                          SerializeChildren({tab1_id, tab2_id}));
  ASSERT_TRUE(db()->CloseTransaction(tx));

  auto builder = std::make_unique<StorageLoadedData::Builder>(
      "win0", false, std::make_unique<TestRestoreEntityTracker>());
  std::unique_ptr<StorageLoadedData> data =
      db()->LoadAllNodes("win0", false, std::move(builder));

  ASSERT_NE(data, nullptr);
  const auto& loaded_tabs = data->GetLoadedTabs();
  ASSERT_EQ(loaded_tabs.size(), 2u);
  EXPECT_EQ(loaded_tabs[0].tab_id(), 1);
  EXPECT_EQ(loaded_tabs[1].tab_id(), 2);
}
// Tests that moving a tab out of a group mid-restore preserves the move across
// reconciliation and does not duplicate the tab back into the group.
TEST_F(StorageLoadedDataTest, Reconcile_MidRestoreMove_Preserved) {
  StorageId root_id = StorageId::Create();
  StorageId unpinned_id = StorageId::Create();
  StorageId group1_id = StorageId::Create();
  StorageId tab1_id = StorageId::Create();
  StorageId tab2_id = StorageId::Create();
  StorageId tab3_id = StorageId::Create();

  auto* tx = db()->CreateTransaction();
  db()->SaveNode(tx, root_id, "win0", false, TabStorageType::kTabStrip,
                 SerializeTabStripState(std::nullopt), /*children=*/{});
  db()->SaveNodeChildren(tx, root_id, SerializeChildren({unpinned_id}));

  db()->SaveNode(tx, unpinned_id, "win0", false, TabStorageType::kUnpinned, {},
                 /*children=*/{});
  // Canonical: Unpinned contains [Group1, Tab3]. Group1 contains [Tab1, Tab2].
  db()->SaveNodeChildren(tx, unpinned_id,
                         SerializeChildren({group1_id, tab3_id}));

  db()->SaveNode(tx, group1_id, "win0", false, TabStorageType::kGroup, {},
                 /*children=*/{});
  db()->SaveNodeChildren(tx, group1_id, SerializeChildren({tab1_id, tab2_id}));

  db()->SaveNode(tx, tab1_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(1), /*children=*/{});
  db()->SaveNode(tx, tab2_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(2), /*children=*/{});
  db()->SaveNode(tx, tab3_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(3), /*children=*/{});

  // User moved Tab1 out of Group1 into Unpinned collection mid-restore.
  // Divergent Unpinned: [Group1, tab1_id, tab3_id]
  db()->SaveDivergentNode(tx, unpinned_id, "win0", false,
                          SerializeChildren({group1_id, tab1_id, tab3_id}));
  // Divergent Group1: [tab2_id]
  db()->SaveDivergentNode(tx, group1_id, "win0", false,
                          SerializeChildren({tab2_id}));
  ASSERT_TRUE(db()->CloseTransaction(tx));

  auto builder = std::make_unique<StorageLoadedData::Builder>(
      "win0", false, std::make_unique<TestRestoreEntityTracker>());
  std::unique_ptr<StorageLoadedData> data =
      db()->LoadAllNodes("win0", false, std::move(builder));

  ASSERT_NE(data, nullptr);
  const auto& loaded_tabs = data->GetLoadedTabs();
  ASSERT_EQ(loaded_tabs.size(), 3u);

  // Group1 has Tab2; Unpinned has Tab1 and Tab3.
  EXPECT_EQ(loaded_tabs[0].tab_id(), 2);
  EXPECT_EQ(loaded_tabs[1].tab_id(), 1);
  EXPECT_EQ(loaded_tabs[2].tab_id(), 3);
}

// Tests that when an active tab is restored out-of-order first into a divergent
// collection vector, ReconcileDivergentNodes does not duplicate the active tab
// and does not drop the first un-restored canonical tab.
TEST_F(StorageLoadedDataTest, Reconcile_OutOfOrderActiveTab_Deduplicated) {
  StorageId root_id = StorageId::Create();
  StorageId unpinned_id = StorageId::Create();
  StorageId tab1_id = StorageId::Create();
  StorageId tab2_id = StorageId::Create();
  StorageId tab3_id = StorageId::Create();

  auto* tx = db()->CreateTransaction();
  // 1. Root collection and Unpinned collection.
  db()->SaveNode(tx, root_id, "win0", false, TabStorageType::kTabStrip,
                 SerializeTabStripState(tab3_id), /*children=*/{});
  db()->SaveNodeChildren(tx, root_id, SerializeChildren({unpinned_id}));

  db()->SaveNode(tx, unpinned_id, "win0", false, TabStorageType::kUnpinned, {},
                 /*children=*/{});
  // Canonical unpinned children: [Tab1, Tab2, Tab3]
  db()->SaveNodeChildren(tx, unpinned_id,
                         SerializeChildren({tab1_id, tab2_id, tab3_id}));

  // 2. Tab payloads.
  db()->SaveNode(tx, tab1_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(1), /*children=*/{});
  db()->SaveNode(tx, tab2_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(2), /*children=*/{});
  db()->SaveNode(tx, tab3_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(3), /*children=*/{});

  // 3. Out-of-order restoration: Tab 3 is restored first into divergent nodes.
  db()->SaveDivergentNode(tx, unpinned_id, "win0", false,
                          SerializeChildren({tab3_id}));
  ASSERT_TRUE(db()->CloseTransaction(tx));

  // 4. LoadAllNodes should reconcile divergent nodes with canonical nodes.
  auto builder = std::make_unique<StorageLoadedData::Builder>(
      "win0", false, std::make_unique<TestRestoreEntityTracker>());
  std::unique_ptr<StorageLoadedData> data =
      db()->LoadAllNodes("win0", false, std::move(builder));

  ASSERT_NE(data, nullptr);
  const auto& loaded_tabs = data->GetLoadedTabs();
  ASSERT_EQ(loaded_tabs.size(), 3u);

  // Tab 3 was restored first, followed by remaining canonical tabs (Tab 1, Tab
  // 2). Neither Tab 1 nor Tab 2 should be dropped, and Tab 3 must not be
  // duplicated.
  EXPECT_EQ(loaded_tabs[0].tab_id(), 3);
  EXPECT_EQ(loaded_tabs[1].tab_id(), 1);
  EXPECT_EQ(loaded_tabs[2].tab_id(), 2);
}

// Tests that an explicitly deleted node mid-restore is excluded from
// reconciliation.
TEST_F(StorageLoadedDataTest, Reconcile_DeletedNode_Dropped) {
  StorageId root_id = StorageId::Create();
  StorageId unpinned_id = StorageId::Create();
  StorageId tab1_id = StorageId::Create();
  StorageId tab2_id = StorageId::Create();
  StorageId tab3_id = StorageId::Create();

  auto* tx = db()->CreateTransaction();
  db()->SaveNode(tx, root_id, "win0", false, TabStorageType::kTabStrip,
                 SerializeTabStripState(std::nullopt), /*children=*/{});
  db()->SaveNodeChildren(tx, root_id, SerializeChildren({unpinned_id}));

  db()->SaveNode(tx, unpinned_id, "win0", false, TabStorageType::kUnpinned, {},
                 /*children=*/{});
  // Canonical: Unpinned has [Tab1, Tab2, Tab3].
  db()->SaveNodeChildren(tx, unpinned_id,
                         SerializeChildren({tab1_id, tab2_id, tab3_id}));

  db()->SaveNode(tx, tab1_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(1), /*children=*/{});
  // Tab2 row was DELETED from nodes table!
  db()->SaveNode(tx, tab3_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(3), /*children=*/{});

  // Divergent nodes recorded Tab1.
  db()->SaveDivergentNode(tx, unpinned_id, "win0", false,
                          SerializeChildren({tab1_id}));
  ASSERT_TRUE(db()->CloseTransaction(tx));

  auto builder = std::make_unique<StorageLoadedData::Builder>(
      "win0", false, std::make_unique<TestRestoreEntityTracker>());
  std::unique_ptr<StorageLoadedData> data =
      db()->LoadAllNodes("win0", false, std::move(builder));

  ASSERT_NE(data, nullptr);
  const auto& loaded_tabs = data->GetLoadedTabs();
  // Tab2 was deleted, so only Tab1 and Tab3 should be loaded.
  ASSERT_EQ(loaded_tabs.size(), 2u);
  EXPECT_EQ(loaded_tabs[0].tab_id(), 1);
  EXPECT_EQ(loaded_tabs[1].tab_id(), 3);
}

// Tests that when a tab belonging to an un-restored group (which has no entry
// in divergent_children_map_) is restored out-of-order into the unpinned
// collection's divergent children, it is filtered out of the canonical group
// and not duplicated.
TEST_F(StorageLoadedDataTest,
       Reconcile_MoveFromUnrestoredCollection_Deduplicated) {
  StorageId root_id = StorageId::Create();
  StorageId unpinned_id = StorageId::Create();
  StorageId group1_id = StorageId::Create();
  StorageId tab1_id = StorageId::Create();
  StorageId tab2_id = StorageId::Create();
  StorageId tab3_id = StorageId::Create();

  auto* tx = db()->CreateTransaction();
  db()->SaveNode(tx, root_id, "win0", false, TabStorageType::kTabStrip,
                 SerializeTabStripState(std::nullopt), /*children=*/{});
  db()->SaveNodeChildren(tx, root_id, SerializeChildren({unpinned_id}));

  db()->SaveNode(tx, unpinned_id, "win0", false, TabStorageType::kUnpinned, {},
                 /*children=*/{});
  // Canonical: Unpinned contains [Group1, Tab3]. Group1 contains [Tab1, Tab2].
  db()->SaveNodeChildren(tx, unpinned_id,
                         SerializeChildren({group1_id, tab3_id}));

  db()->SaveNode(tx, group1_id, "win0", false, TabStorageType::kGroup, {},
                 /*children=*/{});
  db()->SaveNodeChildren(tx, group1_id, SerializeChildren({tab1_id, tab2_id}));

  db()->SaveNode(tx, tab1_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(1), /*children=*/{});
  db()->SaveNode(tx, tab2_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(2), /*children=*/{});
  db()->SaveNode(tx, tab3_id, "win0", false, TabStorageType::kTab,
                 SerializeTabState(3), /*children=*/{});

  // Out-of-order active tab restore: Tab1 (from Group1) was restored first
  // into Unpinned before Group1 was instantiated in memory, so Group1 has NO
  // divergent entry.
  db()->SaveDivergentNode(tx, unpinned_id, "win0", false,
                          SerializeChildren({tab1_id}));
  ASSERT_TRUE(db()->CloseTransaction(tx));

  auto tracker = std::make_unique<TestRestoreEntityTracker>();
  auto builder = std::make_unique<StorageLoadedData::Builder>(
      "win0", false, std::move(tracker));
  std::unique_ptr<StorageLoadedData> data =
      db()->LoadAllNodes("win0", false, std::move(builder));

  ASSERT_NE(data, nullptr);
  const auto& loaded_tabs = data->GetLoadedTabs();
  ASSERT_EQ(loaded_tabs.size(), 3u);
  EXPECT_EQ(loaded_tabs[0].tab_id(), 1);
  EXPECT_EQ(loaded_tabs[1].tab_id(), 2);
  EXPECT_EQ(loaded_tabs[2].tab_id(), 3);

  // Verify the persisted state on a second load to ensure group1's children
  // in the database actually had tab1 filtered out.
  auto tracker2 = std::make_unique<TestRestoreEntityTracker>();
  auto* tracker2_ptr = tracker2.get();
  auto builder2 = std::make_unique<StorageLoadedData::Builder>(
      "win0", false, std::move(tracker2));
  std::unique_ptr<StorageLoadedData> data2 =
      db()->LoadAllNodes("win0", false, std::move(builder2));
  ASSERT_NE(data2, nullptr);
  const auto& loaded_tabs2 = data2->GetLoadedTabs();
  ASSERT_EQ(loaded_tabs2.size(), 3u);
  EXPECT_EQ(loaded_tabs2[0].tab_id(), 1);
  EXPECT_EQ(loaded_tabs2[1].tab_id(), 2);
  EXPECT_EQ(loaded_tabs2[2].tab_id(), 3);
  EXPECT_EQ(tracker2_ptr->GetRegisteredChildren(group1_id),
            std::vector<StorageId>{tab2_id});
}

}  // namespace
}  // namespace tabs
