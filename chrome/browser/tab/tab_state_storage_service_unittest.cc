// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/tab/collection_storage_observer.h"
#include "chrome/browser/tab/node_hash_cache.h"
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/storage_collection_synchronizer.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/fake_tab_interface.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {
namespace {

class TestPayload : public Payload {
 public:
  TestPayload() = default;
  ~TestPayload() override = default;
  std::vector<uint8_t> SerializePayload() const override { return {}; }
};

class TestStoragePackage : public StoragePackage {
 public:
  TestStoragePackage() = default;
  ~TestStoragePackage() override = default;

  std::vector<uint8_t> SerializePayload() const override { return {}; }
  std::vector<uint8_t> SerializeChildren() const override { return {}; }
};

class TestTabStoragePackager : public TabStoragePackager {
 public:
  TestTabStoragePackager() = default;
  ~TestTabStoragePackager() override = default;

  std::unique_ptr<StoragePackage> Package(const TabInterface* tab) override {
    return std::make_unique<TestStoragePackage>();
  }

  bool IsOffTheRecord(const TabCollection* collection) const override {
    return false;
  }

  std::string GetWindowTag(const TabCollection* collection) const override {
    return "test_window";
  }

  int package_tab_strip_count() const { return package_tab_strip_count_; }
  void reset_package_tab_strip_count() { package_tab_strip_count_ = 0; }

 protected:
  std::unique_ptr<Payload> PackageTabStripCollectionData(
      const TabStripCollection* collection,
      StorageIdMapping& mapping) override {
    ++package_tab_strip_count_;
    return std::make_unique<TestPayload>();
  }

 private:
  int package_tab_strip_count_ = 0;
};

class TabStateStorageServiceTest : public ::testing::Test {
 public:
  TabStateStorageServiceTest() = default;
  ~TabStateStorageServiceTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    auto packager = std::make_unique<TestTabStoragePackager>();
    packager_ = packager.get();
    RestoreEntityTrackerFactory tracker_factory = base::BindRepeating(
        [](OnTabAssociation,
           OnCollectionAssociation) -> std::unique_ptr<RestoreEntityTracker> {
          return nullptr;
        });

    service_ = std::make_unique<TabStateStorageService>(
        temp_dir_.GetPath(), /*support_off_the_record_data=*/false,
        std::move(packager), tracker_factory);
  }

  void TearDown() override { service_.reset(); }

  TabStateStorageService* service() { return service_.get(); }
  TestTabStoragePackager* packager() { return packager_; }

  void FlushOperations() {
    base::RunLoop run_loop;
    service_->WaitForAllPendingOperations(run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TabStateStorageService> service_;
  raw_ptr<TestTabStoragePackager> packager_ = nullptr;
};

// Tests that an empty ScopedBatch executes callbacks upon destruction.
TEST_F(TabStateStorageServiceTest, EmptyScopedBatchExecutesCallbacks) {
  bool callback_executed = false;
  {
    TabStateStorageService::ScopedBatch batch = service()->CreateScopedBatch();
    batch.AddCallback(base::BindOnce([](bool* executed) { *executed = true; },
                                     &callback_executed));
  }
  FlushOperations();
  EXPECT_TRUE(callback_executed);
}

// Tests nested ScopedBatch lifetimes to ensure updates are deferred until all
// open batches are destroyed.
TEST_F(TabStateStorageServiceTest, NestedScopedBatchLifetime) {
  bool callback1_executed = false;
  bool callback2_executed = false;

  TabStateStorageService::ScopedBatch batch1 = service()->CreateScopedBatch();
  batch1.AddCallback(base::BindOnce([](bool* executed) { *executed = true; },
                                    &callback1_executed));

  {
    TabStateStorageService::ScopedBatch batch2 = service()->CreateScopedBatch();
    batch2.AddCallback(base::BindOnce([](bool* executed) { *executed = true; },
                                      &callback2_executed));
  }

  // batch2 is destroyed, but batch1 is still alive. Callbacks should not have
  // run yet.
  EXPECT_FALSE(callback1_executed);
  EXPECT_FALSE(callback2_executed);

  // Destroy batch1.
  batch1 = TabStateStorageService::ScopedBatch();
  FlushOperations();

  EXPECT_TRUE(callback1_executed);
  EXPECT_TRUE(callback2_executed);
}

// Tests ScopedBatch operation coalescing when calling Save multiple times on
// the same collection.
TEST_F(TabStateStorageServiceTest, CoalesceSaveNodeDuplicates) {
  TabStripCollection collection;
  bool callback_executed = false;

  {
    TabStateStorageService::ScopedBatch batch = service()->CreateScopedBatch();
    batch.AddCallback(base::BindOnce([](bool* executed) { *executed = true; },
                                     &callback_executed));
    service()->Save(&collection);
    service()->Save(&collection);
    service()->Save(&collection);
  }

  FlushOperations();
  EXPECT_TRUE(callback_executed);
}

// Tests ScopedBatch operation coalescing: Save followed by SavePayload.
TEST_F(TabStateStorageServiceTest, CoalesceSaveNodeAndSavePayload) {
  TabStripCollection collection;
  bool callback_executed = false;

  {
    TabStateStorageService::ScopedBatch batch = service()->CreateScopedBatch();
    batch.AddCallback(base::BindOnce([](bool* executed) { *executed = true; },
                                     &callback_executed));
    service()->Save(&collection);
    service()->SavePayload(&collection);
  }

  FlushOperations();
  EXPECT_TRUE(callback_executed);
}

// Tests ScopedBatch operation coalescing: SaveChildren followed by SavePayload
// squashes into SaveNode.
TEST_F(TabStateStorageServiceTest,
       CoalesceSaveChildrenAndSavePayloadSquashesToSaveNode) {
  TabStripCollection collection;
  bool callback_executed = false;

  {
    TabStateStorageService::ScopedBatch batch = service()->CreateScopedBatch();
    batch.AddCallback(base::BindOnce([](bool* executed) { *executed = true; },
                                     &callback_executed));
    service()->SaveChildren(&collection);
    service()->SavePayload(&collection);
  }

  FlushOperations();
  EXPECT_TRUE(callback_executed);
}

// Tests ScopedBatch operation coalescing: Save followed by Remove.
TEST_F(TabStateStorageServiceTest, CoalesceSaveNodeThenRemoveNode) {
  TabStripCollection collection;
  bool callback_executed = false;

  {
    TabStateStorageService::ScopedBatch batch = service()->CreateScopedBatch();
    batch.AddCallback(base::BindOnce([](bool* executed) { *executed = true; },
                                     &callback_executed));
    service()->Save(&collection);
    service()->Remove(&collection);
  }

  FlushOperations();
  EXPECT_TRUE(callback_executed);
}

// Tests ScopedBatch operation coalescing: Remove followed by Save.
TEST_F(TabStateStorageServiceTest, CoalesceRemoveNodeThenSaveNode) {
  TabStripCollection collection;
  bool callback_executed = false;

  {
    TabStateStorageService::ScopedBatch batch = service()->CreateScopedBatch();
    batch.AddCallback(base::BindOnce([](bool* executed) { *executed = true; },
                                     &callback_executed));
    service()->Remove(&collection);
    service()->Save(&collection);
  }

  FlushOperations();
  EXPECT_TRUE(callback_executed);
}

// Tests high volume batched operations across multiple collections to verify
// state consistency.
TEST_F(TabStateStorageServiceTest, HighVolumeOperationCoalescing) {
  constexpr int kCollectionCount = 20;
  std::vector<std::unique_ptr<TabStripCollection>> collections;
  collections.reserve(kCollectionCount);
  for (int i = 0; i < kCollectionCount; ++i) {
    collections.push_back(std::make_unique<TabStripCollection>());
  }

  bool batch_committed = false;
  {
    TabStateStorageService::ScopedBatch batch = service()->CreateScopedBatch();
    batch.AddCallback(base::BindOnce([](bool* executed) { *executed = true; },
                                     &batch_committed));

    for (int cycle = 0; cycle < 5; ++cycle) {
      for (const auto& collection : collections) {
        service()->Save(collection.get());
        service()->SavePayload(collection.get());
        service()->SaveChildren(collection.get());
      }
    }
  }

  FlushOperations();
  EXPECT_TRUE(batch_committed);
}

TEST_F(TabStateStorageServiceTest, FullSavePopulatesNodeHashCache) {
  TabStripCollection tab_strip;
  StorageCollectionSynchronizer synchronizer(&tab_strip, service());

  split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();
  split_tabs::SplitTabVisualData visual_data(
      split_tabs::SplitTabLayout::kSideBySide, 0.5);
  auto split = std::make_unique<SplitTabCollection>(split_id, visual_data);
  split->AddTab(std::make_unique<FakeTabInterface>(), 0);
  SplitTabCollection* split_ptr = split.get();
  tab_strip.unpinned_collection()->AddCollection(std::move(split), 0);

  EXPECT_EQ(0u, synchronizer.hash_cache().size());

  synchronizer.FullSave(base::NullCallback());
  FlushOperations();

  // Root tab strip, pinned collection, unpinned collection, and the split
  // collection.
  EXPECT_EQ(4u, synchronizer.hash_cache().size());
  EXPECT_TRUE(
      synchronizer.hash_cache().HasNode(service()->GetStorageId(&tab_strip)));
  EXPECT_TRUE(synchronizer.hash_cache().HasNode(
      service()->GetStorageId(tab_strip.pinned_collection())));
  EXPECT_TRUE(synchronizer.hash_cache().HasNode(
      service()->GetStorageId(tab_strip.unpinned_collection())));
  EXPECT_TRUE(
      synchronizer.hash_cache().HasNode(service()->GetStorageId(split_ptr)));

  std::optional<NodeHashes> split_hashes =
      synchronizer.hash_cache().GetNodeHashes(
          service()->GetStorageId(split_ptr));
  ASSERT_TRUE(split_hashes.has_value());
  EXPECT_NE(NodeHashes::kEmptyHash, split_hashes->payload_hash);
  EXPECT_NE(NodeHashes::kEmptyHash, split_hashes->children_hash);
}

TEST_F(TabStateStorageServiceTest, SaveStructureSnapshot_DeltaDiffing) {
  TabStripCollection tab_strip;
  StorageCollectionSynchronizer synchronizer(&tab_strip, service());
  CollectionStorageObserver observer(&tab_strip, service());
  observer.SetSynchronizer(&synchronizer);
  tab_strip.AddObserver(&observer);

  split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();
  split_tabs::SplitTabVisualData visual_data(
      split_tabs::SplitTabLayout::kSideBySide, 0.5);
  auto split = std::make_unique<SplitTabCollection>(split_id, visual_data);
  split->AddTab(std::make_unique<FakeTabInterface>(), 0);
  SplitTabCollection* split_ptr = split.get();
  tab_strip.unpinned_collection()->AddCollection(std::move(split), 0);

  StorageId split_storage_id = service()->GetStorageId(split_ptr);
  StorageId tab_strip_storage_id = service()->GetStorageId(&tab_strip);

  // Initial snapshot writes all collections and populates NodeHashCache.
  EXPECT_EQ(0u, synchronizer.hash_cache().size());
  packager()->reset_package_tab_strip_count();

  observer.SaveStructureSnapshot(/*clear_unused_nodes=*/false);
  FlushOperations();

  EXPECT_EQ(4u, synchronizer.hash_cache().size());
  EXPECT_TRUE(synchronizer.hash_cache().HasNode(split_storage_id));
  EXPECT_TRUE(synchronizer.hash_cache().HasNode(tab_strip_storage_id));
  // Root tab strip was packaged once for Save during initial snapshot.
  EXPECT_EQ(1, packager()->package_tab_strip_count());

  std::optional<NodeHashes> initial_split_hashes =
      synchronizer.hash_cache().GetNodeHashes(split_storage_id);
  ASSERT_TRUE(initial_split_hashes.has_value());
  size_t original_payload_hash = initial_split_hashes->payload_hash;
  size_t original_children_hash = initial_split_hashes->children_hash;
  EXPECT_NE(NodeHashes::kEmptyHash, original_payload_hash);
  EXPECT_NE(NodeHashes::kEmptyHash, original_children_hash);

  // Subsequent snapshot persists root TabStripCollection payload (to
  // preserve platform active tab state), but bypasses unchanged child
  // collections.
  packager()->reset_package_tab_strip_count();
  observer.SaveStructureSnapshot(/*clear_unused_nodes=*/false);
  FlushOperations();

  // Root tab strip payload is packaged once per snapshot.
  EXPECT_EQ(1, packager()->package_tab_strip_count());
  std::optional<NodeHashes> noop_split_hashes =
      synchronizer.hash_cache().GetNodeHashes(split_storage_id);
  ASSERT_TRUE(noop_split_hashes.has_value());
  EXPECT_EQ(original_payload_hash, noop_split_hashes->payload_hash);
  EXPECT_EQ(original_children_hash, noop_split_hashes->children_hash);

  // Split visual metadata change writes only SavePayload for that split.
  split_ptr->data()->visual_data()->set_split_ratio(0.8);
  packager()->reset_package_tab_strip_count();
  observer.SaveStructureSnapshot(/*clear_unused_nodes=*/false);
  FlushOperations();

  EXPECT_EQ(1, packager()->package_tab_strip_count());
  std::optional<NodeHashes> payload_changed_hashes =
      synchronizer.hash_cache().GetNodeHashes(split_storage_id);
  ASSERT_TRUE(payload_changed_hashes.has_value());
  // Payload hash changed, but children hash remained identical.
  EXPECT_NE(original_payload_hash, payload_changed_hashes->payload_hash);
  EXPECT_EQ(original_children_hash, payload_changed_hashes->children_hash);

  // Child addition/move writes only SaveChildren for the affected
  // collection.
  split_ptr->AddTab(std::make_unique<FakeTabInterface>(), 1);
  packager()->reset_package_tab_strip_count();
  observer.SaveStructureSnapshot(/*clear_unused_nodes=*/false);
  FlushOperations();

  EXPECT_EQ(1, packager()->package_tab_strip_count());
  std::optional<NodeHashes> children_changed_hashes =
      synchronizer.hash_cache().GetNodeHashes(split_storage_id);
  ASSERT_TRUE(children_changed_hashes.has_value());
  // Children hash changed, but payload hash remained identical to the updated
  // payload.
  EXPECT_EQ(payload_changed_hashes->payload_hash,
            children_changed_hashes->payload_hash);
  EXPECT_NE(original_children_hash, children_changed_hashes->children_hash);

  // ClearSubTree immediately evicts the node from cache.
  EXPECT_TRUE(synchronizer.hash_cache().HasNode(split_storage_id));
  CollectionStorageObserver::ClearSubTree(split_ptr, service(),
                                          &synchronizer.hash_cache());
  EXPECT_FALSE(synchronizer.hash_cache().HasNode(split_storage_id));
  EXPECT_TRUE(
      tab_strip.unpinned_collection()->MaybeRemoveCollection(split_ptr));

  // PruneExcept removes unreferenced entries when clear_unused_nodes is true.
  StorageId synthetic_id = base::UnguessableToken::Create();
  synchronizer.hash_cache().SetNodeHashes(synthetic_id, NodeHashes{123u, 456u});
  EXPECT_TRUE(synchronizer.hash_cache().HasNode(synthetic_id));
  observer.SaveStructureSnapshot(/*clear_unused_nodes=*/true);
  FlushOperations();
  EXPECT_FALSE(synchronizer.hash_cache().HasNode(synthetic_id));
  tab_strip.RemoveObserver(&observer);
}

}  // namespace
}  // namespace tabs
