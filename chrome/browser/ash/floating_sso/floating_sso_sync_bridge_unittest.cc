// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"

#include <map>
#include <memory>
#include <optional>

#include "base/barrier_closure.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/floating_sso/cookie_sync_test_util.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::floating_sso {

namespace {

using testing::_;

constexpr char kKeyForTests[] = "test_key_value";

syncer::EntityData MakeEntityData(const sync_pb::CookieSpecifics& specifics) {
  syncer::EntityData entity_data;
  entity_data.specifics.mutable_cookie()->CopyFrom(specifics);
  entity_data.name = specifics.unique_key();
  return entity_data;
}

void CommitToStoreAndWait(
    syncer::DataTypeStore* store,
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch) {
  base::test::TestFuture<const std::optional<syncer::ModelError>&> future;

  store->CommitWriteBatch(std::move(batch), future.GetCallback());
  const std::optional<syncer::ModelError>& error = future.Get();
  EXPECT_FALSE(error.has_value()) << error->ToString();
}

}  // namespace

class FloatingSsoSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    // Create a store for test and add some initial data to it.
    store_ = syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();

    // Create a bridge and then wait until it finishes reading initial data from
    // the store.
    bridge_ = std::make_unique<FloatingSsoSyncBridge>(
        processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));

    for (size_t i = 0; i < kNamesForTests.size(); ++i) {
      sync_pb::CookieSpecifics specifics =
          CreatePredefinedCookieSpecificsForTest(i);
      bridge_->AddOrUpdateCookie(specifics);
    }

    ASSERT_TRUE(base::test::RunUntil([&bridge = bridge_]() {
      return bridge->IsInitialDataReadFinishedForTest();
    }));
  }

  FloatingSsoSyncBridge& bridge() { return *bridge_; }
  syncer::MockDataTypeLocalChangeProcessor& processor() { return processor_; }

 private:
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  std::unique_ptr<FloatingSsoSyncBridge> bridge_;
};

TEST_F(FloatingSsoSyncBridgeTest, GetStorageKey) {
  syncer::EntityData entity;
  entity.specifics.mutable_cookie()->set_unique_key(kKeyForTests);
  EXPECT_EQ(kKeyForTests, bridge().GetStorageKey(entity));
}

TEST_F(FloatingSsoSyncBridgeTest, GetClientTag) {
  syncer::EntityData entity;
  entity.specifics.mutable_cookie()->set_unique_key(kKeyForTests);
  EXPECT_EQ(kKeyForTests, bridge().GetClientTag(entity));
}

TEST_F(FloatingSsoSyncBridgeTest, InitialEntities) {
  const auto& entries = bridge().CookieSpecificsEntriesForTest();
  EXPECT_EQ(entries.size(), kNamesForTests.size());
  for (size_t i = 0; i < kNamesForTests.size(); ++i) {
    EXPECT_THAT(
        entries.at(kUniqueKeysForTests[i]),
        base::test::EqualsProto(CreatePredefinedCookieSpecificsForTest(i)));
  }
}

TEST_F(FloatingSsoSyncBridgeTest, GetDataForCommit) {
  std::unique_ptr<syncer::DataBatch> data_batch = bridge().GetDataForCommit(
      {kUniqueKeysForTests[1], kUniqueKeysForTests[3]});

  ASSERT_TRUE(data_batch);
  for (size_t i : {1, 3}) {
    ASSERT_TRUE(data_batch->HasNext());
    syncer::KeyAndData key_and_data = data_batch->Next();
    EXPECT_EQ(kUniqueKeysForTests[i], key_and_data.first);
    EXPECT_EQ(kUniqueKeysForTests[i], key_and_data.second->name);
    EXPECT_THAT(
        key_and_data.second->specifics.cookie(),
        base::test::EqualsProto(CreatePredefinedCookieSpecificsForTest(i)));
  }
  // Batch should have no other elements except for the two handled above.
  EXPECT_FALSE(data_batch->HasNext());
}

TEST_F(FloatingSsoSyncBridgeTest, GetDataForDebugging) {
  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge().GetAllDataForDebugging();
  ASSERT_TRUE(data_batch);
  const auto& entries = bridge().CookieSpecificsEntriesForTest();
  size_t batch_size = 0;
  // Check that `data_batch` and `entries` contain the same data.
  while (data_batch->HasNext()) {
    batch_size += 1;
    const syncer::KeyAndData key_and_data = data_batch->Next();
    auto it = entries.find(key_and_data.first);
    ASSERT_NE(it, entries.end());
    EXPECT_EQ(key_and_data.second->name, it->first);
    EXPECT_THAT(key_and_data.second->specifics.cookie(),
                base::test::EqualsProto(it->second));
  }
  EXPECT_EQ(batch_size, entries.size());
}

// Verify that local data doesn't change after applying an incremental change
// with an empty change list.
TEST_F(FloatingSsoSyncBridgeTest, ApplyEmptyChange) {
  auto initial_entries_copy = bridge().CookieSpecificsEntriesForTest();
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       syncer::EntityChangeList());
  const auto& current_entries = bridge().CookieSpecificsEntriesForTest();
  EXPECT_EQ(initial_entries_copy.size(), current_entries.size());
  for (const auto& [key, specifics] : current_entries) {
    EXPECT_THAT(specifics,
                base::test::EqualsProto(initial_entries_copy.at(key)));
  }
}

TEST_F(FloatingSsoSyncBridgeTest, IncrementalDeleteAndAdd) {
  const auto& entries = bridge().CookieSpecificsEntriesForTest();
  size_t initial_size = entries.size();
  ASSERT_TRUE(entries.contains(kUniqueKeysForTests[0]));

  // Delete the first entity.
  syncer::EntityChangeList delete_first;
  delete_first.push_back(
      syncer::EntityChange::CreateDelete(kUniqueKeysForTests[0]));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(delete_first));
  EXPECT_EQ(entries.size(), initial_size - 1);
  EXPECT_FALSE(entries.contains(kUniqueKeysForTests[0]));

  // Add the entity back.
  syncer::EntityChangeList add_first;
  add_first.push_back(syncer::EntityChange::CreateAdd(
      kUniqueKeysForTests[0],
      MakeEntityData(CreatePredefinedCookieSpecificsForTest(0))));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_first));
  EXPECT_EQ(entries.size(), initial_size);
  EXPECT_TRUE(entries.contains(kUniqueKeysForTests[0]));
  EXPECT_THAT(
      entries.at(kUniqueKeysForTests[0]),
      base::test::EqualsProto(CreatePredefinedCookieSpecificsForTest(0)));
}

TEST_F(FloatingSsoSyncBridgeTest, IncrementalUpdate) {
  auto initial_entries_copy = bridge().CookieSpecificsEntriesForTest();
  ASSERT_TRUE(initial_entries_copy.contains(kUniqueKeysForTests[0]));

  // Update the first entity.
  syncer::EntityChangeList update;
  sync_pb::CookieSpecifics updated_specifics =
      CreatePredefinedCookieSpecificsForTest(0);
  updated_specifics.set_value("UpdatedValue");
  // Make sure that `updated_specifics` is not equal to the proto we had
  // initially.
  ASSERT_THAT(initial_entries_copy.at(kUniqueKeysForTests[0]),
              testing::Not(base::test::EqualsProto(updated_specifics)));
  update.push_back(syncer::EntityChange::CreateUpdate(
      kUniqueKeysForTests[0], MakeEntityData(updated_specifics)));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(update));

  // Check that the first entry got updated while others remained the same.
  const auto& current_entries = bridge().CookieSpecificsEntriesForTest();
  EXPECT_EQ(initial_entries_copy.size(), current_entries.size());
  for (const auto& [key, specifics] : current_entries) {
    EXPECT_THAT(specifics,
                base::test::EqualsProto(key == kUniqueKeysForTests[0]
                                            ? updated_specifics
                                            : initial_entries_copy.at(key)));
  }
}

// TODO: b/353222478 - for now we always prefer remote data. Expand this test
// with an example where a local cookie wins against the remote one during
// conflict resolution (this will happen with local SAML cookies).
TEST_F(FloatingSsoSyncBridgeTest, MergeFullSyncData) {
  auto initial_entries_copy = bridge().CookieSpecificsEntriesForTest();

  syncer::EntityChangeList remote_entities;
  // Remote cookie which should update one of the locally stored cookies.
  sync_pb::CookieSpecifics updated_first_cookie =
      CreatePredefinedCookieSpecificsForTest(0);
  updated_first_cookie.set_value("NewRemoteValue");
  remote_entities.push_back(syncer::EntityChange::CreateAdd(
      kUniqueKeysForTests[0], MakeEntityData(updated_first_cookie)));
  // Remote cookie which should be completely new for the client.
  sync_pb::CookieSpecifics new_remote_cookie;
  // Key is the only part relevant for this test, so we don't populate other
  // fields.
  new_remote_cookie.set_unique_key(kKeyForTests);
  // Make sure this key is not present locally.
  ASSERT_FALSE(initial_entries_copy.contains(kKeyForTests));
  remote_entities.push_back(syncer::EntityChange::CreateAdd(
      kKeyForTests, MakeEntityData(new_remote_cookie)));

  // Expect local-only cookies to be sent to Sync server.
  EXPECT_CALL(processor(), Put(kUniqueKeysForTests[1], _, _));
  EXPECT_CALL(processor(), Put(kUniqueKeysForTests[2], _, _));
  EXPECT_CALL(processor(), Put(kUniqueKeysForTests[3], _, _));

  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_entities));

  const auto& current_local_entries = bridge().CookieSpecificsEntriesForTest();
  // Expect one new entry and one updated entry, the rest should be the same as
  // before.
  EXPECT_EQ(current_local_entries.size(), initial_entries_copy.size() + 1);
  for (const auto& [key, specifics] : current_local_entries) {
    if (key == kKeyForTests) {
      EXPECT_THAT(specifics, base::test::EqualsProto(new_remote_cookie));
    } else if (key == kUniqueKeysForTests[0]) {
      EXPECT_THAT(specifics, base::test::EqualsProto(updated_first_cookie));
    } else {
      EXPECT_THAT(specifics,
                  base::test::EqualsProto(initial_entries_copy.at(key)));
    }
  }
}

TEST_F(FloatingSsoSyncBridgeTest, AddOrUpdateCookie) {
  auto initial_entries_copy = bridge().CookieSpecificsEntriesForTest();
  ASSERT_TRUE(initial_entries_copy.contains(kUniqueKeysForTests[0]));

  // Update the first entity.
  sync_pb::CookieSpecifics updated_specifics =
      CreatePredefinedCookieSpecificsForTest(0);
  constexpr char kUpdatedValue[] = "UpdatedValue";
  updated_specifics.set_value(kUpdatedValue);

  // Check that the updated entry will be sent to the Sync server.
  EXPECT_CALL(processor(), Put(kUniqueKeysForTests[0], _, _));

  bridge().AddOrUpdateCookie(updated_specifics);

  // Check that the first entry got updated while others remained the same.
  auto current_entries = bridge().CookieSpecificsEntriesForTest();
  EXPECT_EQ(initial_entries_copy.size(), current_entries.size());

  for (const auto& [key, specifics] : current_entries) {
    EXPECT_EQ(specifics.value(), key == kUniqueKeysForTests[0]
                                     ? kUpdatedValue
                                     : initial_entries_copy.at(key).value());
  }

  // Add new entry.
  constexpr char kNewUniqueKey[] =
      "https://toplevelsite.comtrueNewNamewww.example.com/baz219";
  constexpr char kNewName[] = "NewName";
  sync_pb::CookieSpecifics new_specifics =
      CreateCookieSpecificsForTest(kNewUniqueKey, kNewName);

  // Check that the new entry will be sent to the Sync server.
  EXPECT_CALL(processor(), Put(kNewUniqueKey, _, _));

  bridge().AddOrUpdateCookie(new_specifics);

  // Check that a new entry was added.
  current_entries = bridge().CookieSpecificsEntriesForTest();
  EXPECT_EQ(initial_entries_copy.size() + 1, current_entries.size());
  EXPECT_TRUE(current_entries.contains(kNewUniqueKey));

  // Check current entries.
  for (const auto& [key, specifics] : current_entries) {
    EXPECT_EQ(specifics.name(), key == kNewUniqueKey
                                    ? kNewName
                                    : initial_entries_copy.at(key).name());
  }
}

TEST_F(FloatingSsoSyncBridgeTest, DeleteCookie) {
  auto initial_entries_copy = bridge().CookieSpecificsEntriesForTest();
  ASSERT_TRUE(initial_entries_copy.contains(kUniqueKeysForTests[0]));

  // Check that the entry deletion will be sent to the Sync server.
  EXPECT_CALL(processor(), Delete(kUniqueKeysForTests[0], _, _));

  // Delete the first entity.
  bridge().DeleteCookie(kUniqueKeysForTests[0]);

  // Check that the first entry was deleted.
  const auto& current_entries = bridge().CookieSpecificsEntriesForTest();
  EXPECT_EQ(initial_entries_copy.size() - 1, current_entries.size());

  // Check current entries.
  EXPECT_FALSE(current_entries.contains(kUniqueKeysForTests[0]));
  EXPECT_TRUE(current_entries.contains(kUniqueKeysForTests[1]));
  EXPECT_TRUE(current_entries.contains(kUniqueKeysForTests[2]));
  EXPECT_TRUE(current_entries.contains(kUniqueKeysForTests[3]));
}

TEST(FloatingSsoSyncBridgeInitialization, EventsWhileStoreIsLoading) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor;
  auto store = syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();

  // Add a cookie to the store so that we can delete it later.
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store->CreateWriteBatch();
  sync_pb::CookieSpecifics delete_specifics =
      CreatePredefinedCookieSpecificsForTest(0);
  batch->WriteData(delete_specifics.unique_key(),
                   delete_specifics.SerializeAsString());
  CommitToStoreAndWait(store.get(), std::move(batch));

  base::test::TestFuture<syncer::DataType, syncer::DataTypeStore::InitCallback>
      store_future;

  // Create a bridge.
  auto bridge = std::make_unique<FloatingSsoSyncBridge>(
      processor.CreateForwardingProcessor(), store_future.GetCallback());

  // Delete already existing item from store.
  bridge->DeleteCookie(kUniqueKeysForTests[0]);

  // Add a cookie before the store is initialized to test the queue.
  sync_pb::CookieSpecifics add_specifics =
      CreatePredefinedCookieSpecificsForTest(1);
  // Used for waiting for the two store commits to be finalized.
  base::test::TestFuture<void> commit_future;
  bridge->SetOnCommitCallbackForTest(base::BarrierClosure(
      /*num_callbacks=*/2, commit_future.GetRepeatingCallback()));
  bridge->AddOrUpdateCookie(add_specifics);

  // Add another cookie and remove it from the queue before the store
  // initializes.
  constexpr char kNewUniqueKey[] =
      "https://toplevelsite.comtrueNewNamewww.example.com/baz219";
  sync_pb::CookieSpecifics new_specifics =
      CreateCookieSpecificsForTest(kNewUniqueKey, "NewName");
  bridge->AddOrUpdateCookie(new_specifics);
  bridge->DeleteCookie(kNewUniqueKey);

  auto [type, callback] = store_future.Take();
  // Trigger OnStoreCreated().
  std::move(callback).Run(
      /*error=*/std::nullopt, std::move(store));

  // Wait until the bridge finishes reading initial data from the store.
  ASSERT_TRUE(base::test::RunUntil([&bridge_internal = bridge]() {
    return bridge_internal->IsInitialDataReadFinishedForTest();
  }));

  // Wait for commits.
  commit_future.Get();

  // Check that there is just kUniqueKeysForTests[0] in the store and the other
  // cookie was not added.
  const auto& current_entries = bridge->CookieSpecificsEntriesForTest();
  EXPECT_EQ(1u, current_entries.size());
  EXPECT_TRUE(current_entries.contains(kUniqueKeysForTests[1]));
  EXPECT_FALSE(current_entries.contains(kUniqueKeysForTests[0]));
  EXPECT_FALSE(current_entries.contains(kNewUniqueKey));
}

}  // namespace ash::floating_sso
