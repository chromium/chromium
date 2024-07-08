// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"

#include <memory>
#include <optional>

#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/floating_sso/cookie_sync_test_util.h"
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

constexpr char kKeyForTests[] = "test_key_value";

}  // namespace

class FloatingSsoSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    // Create a store for test and add some initial data to it.
    store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
    std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    sync_pb::CookieSpecifics sync_specifics = DefaultCookieSpecificsForTest();
    batch->WriteData(sync_specifics.unique_key(),
                     sync_specifics.SerializeAsString());
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
  EXPECT_EQ(entries.size(), 1u);
  const auto& [key, specifics] = *entries.begin();
  EXPECT_EQ(key, specifics.unique_key());
  EXPECT_THAT(specifics,
              base::test::EqualsProto(DefaultCookieSpecificsForTest()));
}

}  // namespace ash::floating_sso
