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
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {
namespace {

class TestPayload : public Payload {
 public:
  TestPayload() = default;
  ~TestPayload() override = default;
  std::vector<uint8_t> SerializePayload() const override { return {}; }
};

class TestTabStoragePackager : public TabStoragePackager {
 public:
  TestTabStoragePackager() = default;
  ~TestTabStoragePackager() override = default;

  std::unique_ptr<StoragePackage> Package(const TabInterface* tab) override {
    return nullptr;
  }

  bool IsOffTheRecord(const TabCollection* collection) const override {
    return false;
  }

  std::string GetWindowTag(const TabCollection* collection) const override {
    return "test_window";
  }

 protected:
  std::unique_ptr<Payload> PackageTabStripCollectionData(
      const TabStripCollection* collection,
      StorageIdMapping& mapping) override {
    return std::make_unique<TestPayload>();
  }
};

class TabStateStorageServiceTest : public ::testing::Test {
 public:
  TabStateStorageServiceTest() = default;
  ~TabStateStorageServiceTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    auto packager = std::make_unique<TestTabStoragePackager>();
    TabCanonicalizer canonicalizer =
        base::BindRepeating([](const TabInterface* tab) { return tab; });
    RestoreEntityTrackerFactory tracker_factory = base::BindRepeating(
        [](OnTabAssociation,
           OnCollectionAssociation) -> std::unique_ptr<RestoreEntityTracker> {
          return nullptr;
        });

    service_ = std::make_unique<TabStateStorageService>(
        temp_dir_.GetPath(), /*support_off_the_record_data=*/false,
        std::move(packager), canonicalizer, tracker_factory);
  }

  void TearDown() override { service_.reset(); }

  TabStateStorageService* service() { return service_.get(); }

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

}  // namespace
}  // namespace tabs
