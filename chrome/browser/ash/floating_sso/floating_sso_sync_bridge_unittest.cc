// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"

#include <map>
#include <memory>
#include <optional>

#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/floating_sso/cookie_sync_test_util.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
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

}  // namespace

class FloatingSsoSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    // Create a store for test and add some initial data to it.
    store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
    std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    for (size_t i = 0; i < kNamesForTests.size(); ++i) {
      sync_pb::CookieSpecifics specifics = CookieSpecificsForTest(i);
      batch->WriteData(specifics.unique_key(), specifics.SerializeAsString());
    }
    CommitToStoreAndWait(std::move(batch));

    // Create a bridge and then wait until it finishes reading initial data from
    // the store.
    bridge_ = std::make_unique<FloatingSsoSyncBridge>(
        processor_.CreateForwardingProcessor(),
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
            store_.get()));
    ASSERT_TRUE(base::test::RunUntil([&bridge = bridge_]() {
      return bridge->IsInitialDataReadFinishedForTest();
    }));
  }

  FloatingSsoSyncBridge& bridge() { return *bridge_; }
  syncer::MockModelTypeChangeProcessor& processor() { return processor_; }

 private:
  void CommitToStoreAndWait(
      std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch) {
    base::test::TestFuture<const std::optional<syncer::ModelError>&> future;

    store_->CommitWriteBatch(std::move(batch), future.GetCallback());
    const std::optional<syncer::ModelError>& error = future.Get();
    EXPECT_FALSE(error.has_value()) << error->ToString();
  }

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
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
    EXPECT_THAT(entries.at(kUniqueKeysForTests[i]),
                base::test::EqualsProto(CookieSpecificsForTest(i)));
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
    EXPECT_THAT(key_and_data.second->specifics.cookie(),
                base::test::EqualsProto(CookieSpecificsForTest(i)));
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
      kUniqueKeysForTests[0], MakeEntityData(CookieSpecificsForTest(0))));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_first));
  EXPECT_EQ(entries.size(), initial_size);
  EXPECT_TRUE(entries.contains(kUniqueKeysForTests[0]));
  EXPECT_THAT(entries.at(kUniqueKeysForTests[0]),
              base::test::EqualsProto(CookieSpecificsForTest(0)));
}

TEST_F(FloatingSsoSyncBridgeTest, IncrementalUpdate) {
  auto initial_entries_copy = bridge().CookieSpecificsEntriesForTest();
  ASSERT_TRUE(initial_entries_copy.contains(kUniqueKeysForTests[0]));

  // Update the first entity.
  syncer::EntityChangeList update;
  sync_pb::CookieSpecifics updated_specifics = CookieSpecificsForTest(0);
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
  sync_pb::CookieSpecifics updated_first_cookie = CookieSpecificsForTest(0);
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
  EXPECT_CALL(processor(), Put(kUniqueKeysForTests[1], _, _)).Times(1);
  EXPECT_CALL(processor(), Put(kUniqueKeysForTests[2], _, _)).Times(1);
  EXPECT_CALL(processor(), Put(kUniqueKeysForTests[3], _, _)).Times(1);

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

}  // namespace ash::floating_sso
