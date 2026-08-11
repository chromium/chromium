// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_android_theme_sync_bridge.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_customization {
namespace {

constexpr char kTestEntityName[] = "Android Theme";
constexpr char kTestStorageKey[] = "current_android_theme";
constexpr char kTestImageUrl[] = "https://example.com/image.png";
constexpr char kTestNewImageUrl[] = "https://example.com/new.png";
constexpr char kTestInvalidData[] = "\xFF\xFF\xFFinvalid_proto";

using testing::_;

class NtpAndroidThemeSyncBridgeTest : public testing::Test {
 public:
  void OnThemeChanged(const sync_pb::ThemeAndroidSpecifics& specifics) {
    last_received_specifics_ = specifics;
    callback_count_++;
  }

 protected:
  NtpAndroidThemeSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    InitializeBridge();
  }

  void InitializeBridge() {
    bridge_ = std::make_unique<NtpAndroidThemeSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        base::BindRepeating(&NtpAndroidThemeSyncBridgeTest::OnThemeChanged,
                            base::Unretained(this)));
  }

  syncer::EntityChangeList CreateEntityChangeList(
      const std::vector<sync_pb::ThemeAndroidSpecifics>& specifics_list) {
    syncer::EntityChangeList list;
    for (const auto& specifics : specifics_list) {
      syncer::EntityData data;
      *data.specifics.mutable_theme_android() = specifics;
      data.name = kTestEntityName;
      list.push_back(
          syncer::EntityChange::CreateAdd(kTestStorageKey, std::move(data)));
    }
    return list;
  }

  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  std::unique_ptr<NtpAndroidThemeSyncBridge> bridge_;
  std::optional<sync_pb::ThemeAndroidSpecifics> last_received_specifics_;
  int callback_count_ = 0;
};

TEST_F(NtpAndroidThemeSyncBridgeTest, UpdateTheme_Success) {
  ON_CALL(mock_processor_, IsTrackingMetadata())
      .WillByDefault(testing::Return(true));

  sync_pb::ThemeAndroidSpecifics specifics;
  specifics.mutable_ntp_background()->set_url(kTestImageUrl);

  EXPECT_CALL(mock_processor_, Put(kTestStorageKey, _, _));
  bridge_->UpdateTheme(specifics);

  auto records =
      syncer::DataTypeStoreTestUtil::ReadAllDataAsProtoAndWait<
          sync_pb::ThemeAndroidSpecifics>(*store_);
  ASSERT_EQ(1u, records.count(kTestStorageKey));
  EXPECT_EQ(kTestImageUrl, records[kTestStorageKey].ntp_background().url());

  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge_->GetDataForCommit({kTestStorageKey});
  ASSERT_TRUE(data_batch);
  EXPECT_TRUE(data_batch->HasNext());
}

TEST_F(NtpAndroidThemeSyncBridgeTest, UpdateTheme_WhenNotTrackingMetadata) {
  ON_CALL(mock_processor_, IsTrackingMetadata())
      .WillByDefault(testing::Return(false));

  sync_pb::ThemeAndroidSpecifics specifics;
  specifics.mutable_ntp_background()->set_url(kTestImageUrl);

  EXPECT_CALL(mock_processor_, Put(kTestStorageKey, _, _)).Times(0);
  bridge_->UpdateTheme(specifics);

  auto records = syncer::DataTypeStoreTestUtil::ReadAllDataAsProtoAndWait<
      sync_pb::ThemeAndroidSpecifics>(*store_);
  ASSERT_EQ(1u, records.count(kTestStorageKey));
  EXPECT_EQ(kTestImageUrl, records[kTestStorageKey].ntp_background().url());
}

TEST_F(NtpAndroidThemeSyncBridgeTest, UpdateTheme_BeforeStoreCreated) {
  std::unique_ptr<NtpAndroidThemeSyncBridge> uninitialized_bridge =
      std::make_unique<NtpAndroidThemeSyncBridge>(
          mock_processor_.CreateForwardingProcessor(),
          base::BindOnce([](syncer::DataType,
                            syncer::DataTypeStore::InitCallback) {}),
          base::BindRepeating(&NtpAndroidThemeSyncBridgeTest::OnThemeChanged,
                              base::Unretained(this)));

  sync_pb::ThemeAndroidSpecifics specifics;
  specifics.mutable_ntp_background()->set_url(kTestImageUrl);
  // Should return early and not crash.
  uninitialized_bridge->UpdateTheme(specifics);
}

TEST_F(NtpAndroidThemeSyncBridgeTest, MergeFullSyncData_Success) {
  sync_pb::ThemeAndroidSpecifics specifics;
  specifics.mutable_ntp_background()->set_url(kTestImageUrl);

  std::optional<syncer::ModelError> error = bridge_->MergeFullSyncData(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeList({specifics}));

  EXPECT_FALSE(error.has_value());
  EXPECT_EQ(1, callback_count_);
  ASSERT_TRUE(last_received_specifics_.has_value());
  EXPECT_EQ(kTestImageUrl,
            last_received_specifics_->ntp_background().url());
}

TEST_F(NtpAndroidThemeSyncBridgeTest, MergeFullSyncData_TooManySpecifics) {
  sync_pb::ThemeAndroidSpecifics s1;
  sync_pb::ThemeAndroidSpecifics s2;

  std::optional<syncer::ModelError> error = bridge_->MergeFullSyncData(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeList({s1, s2}));

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(syncer::ModelError::Type::kThemeTooManySpecifics,
            error->type());
  EXPECT_EQ(0, callback_count_);
}

TEST_F(NtpAndroidThemeSyncBridgeTest, ApplyIncrementalSyncChanges_Success) {
  sync_pb::ThemeAndroidSpecifics specifics;
  specifics.mutable_ntp_background()->set_url(kTestNewImageUrl);

  std::optional<syncer::ModelError> error =
      bridge_->ApplyIncrementalSyncChanges(
          bridge_->CreateMetadataChangeList(),
          CreateEntityChangeList({specifics}));

  EXPECT_FALSE(error.has_value());
  EXPECT_EQ(1, callback_count_);
  ASSERT_TRUE(last_received_specifics_.has_value());
  EXPECT_EQ(kTestNewImageUrl,
            last_received_specifics_->ntp_background().url());
}

TEST_F(NtpAndroidThemeSyncBridgeTest, ApplyIncrementalSyncChanges_TooManyChanges) {
  sync_pb::ThemeAndroidSpecifics s1;
  sync_pb::ThemeAndroidSpecifics s2;

  std::optional<syncer::ModelError> error =
      bridge_->ApplyIncrementalSyncChanges(
          bridge_->CreateMetadataChangeList(),
          CreateEntityChangeList({s1, s2}));

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(syncer::ModelError::Type::kThemeTooManyChanges,
            error->type());
  EXPECT_EQ(0, callback_count_);
}

TEST_F(NtpAndroidThemeSyncBridgeTest, ApplyIncrementalSyncChanges_Delete) {
  sync_pb::ThemeAndroidSpecifics specifics;
  specifics.mutable_ntp_background()->set_url(kTestImageUrl);
  bridge_->UpdateTheme(specifics);
  callback_count_ = 0;

  syncer::EntityChangeList changes;
  changes.push_back(syncer::EntityChange::CreateDelete(
      kTestStorageKey, syncer::EntityData()));

  std::optional<syncer::ModelError> error =
      bridge_->ApplyIncrementalSyncChanges(
          bridge_->CreateMetadataChangeList(), std::move(changes));

  EXPECT_FALSE(error.has_value());
  EXPECT_EQ(1, callback_count_);
  ASSERT_TRUE(last_received_specifics_.has_value());
  EXPECT_FALSE(last_received_specifics_->has_ntp_background());

  syncer::DataTypeStore::RecordList records =
      syncer::DataTypeStoreTestUtil::ReadAllDataAndWait(*store_);
  EXPECT_TRUE(records.empty());
}

TEST_F(NtpAndroidThemeSyncBridgeTest, ApplyDisableSyncChanges) {
  sync_pb::ThemeAndroidSpecifics specifics;
  specifics.mutable_ntp_background()->set_url(kTestImageUrl);
  bridge_->UpdateTheme(specifics);
  callback_count_ = 0;

  syncer::DataTypeStore::RecordList records_before =
      syncer::DataTypeStoreTestUtil::ReadAllDataAndWait(*store_);
  ASSERT_FALSE(records_before.empty());

  bridge_->ApplyDisableSyncChanges(bridge_->CreateMetadataChangeList());

  EXPECT_EQ(0, callback_count_);
  syncer::DataTypeStore::RecordList records_after =
      syncer::DataTypeStoreTestUtil::ReadAllDataAndWait(*store_);
  EXPECT_TRUE(records_after.empty());

  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge_->GetDataForCommit({kTestStorageKey});
  ASSERT_TRUE(data_batch);
  EXPECT_FALSE(data_batch->HasNext());
}

TEST_F(NtpAndroidThemeSyncBridgeTest, GetDataForCommit) {
  sync_pb::ThemeAndroidSpecifics specifics;
  specifics.mutable_ntp_background()->set_url(kTestImageUrl);
  bridge_->UpdateTheme(specifics);

  std::unique_ptr<syncer::DataBatch> commit_batch =
      bridge_->GetDataForCommit({kTestStorageKey});
  ASSERT_TRUE(commit_batch);
  ASSERT_TRUE(commit_batch->HasNext());
  auto [key, data] = commit_batch->Next();
  EXPECT_EQ(kTestStorageKey, key);
  EXPECT_EQ(kTestImageUrl,
            data->specifics.theme_android().ntp_background().url());
}

TEST_F(NtpAndroidThemeSyncBridgeTest, GetAllDataForDebugging) {
  sync_pb::ThemeAndroidSpecifics specifics;
  specifics.mutable_ntp_background()->set_url(kTestImageUrl);
  bridge_->UpdateTheme(specifics);

  std::unique_ptr<syncer::DataBatch> debug_batch =
      bridge_->GetAllDataForDebugging();
  ASSERT_TRUE(debug_batch);
  ASSERT_TRUE(debug_batch->HasNext());
  auto [key, data] = debug_batch->Next();
  EXPECT_EQ(kTestStorageKey, key);
  EXPECT_EQ(kTestImageUrl,
            data->specifics.theme_android().ntp_background().url());
}

TEST_F(NtpAndroidThemeSyncBridgeTest, IsEntityDataValid) {
  syncer::EntityData invalid_data;
  EXPECT_FALSE(bridge_->IsEntityDataValid(invalid_data));

  syncer::EntityData valid_data;
  valid_data.specifics.mutable_theme_android();
  EXPECT_TRUE(bridge_->IsEntityDataValid(valid_data));
}

TEST_F(NtpAndroidThemeSyncBridgeTest, GetClientTag) {
  syncer::EntityData data;
  EXPECT_EQ("android_theme_tag", bridge_->GetClientTag(data));
}

TEST_F(NtpAndroidThemeSyncBridgeTest, GetStorageKey) {
  syncer::EntityData data;
  EXPECT_EQ(kTestStorageKey, bridge_->GetStorageKey(data));
}

TEST_F(NtpAndroidThemeSyncBridgeTest, TrimAllSupportedFieldsFromRemoteSpecifics) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_theme_android()->mutable_ntp_background()->set_url(
      kTestImageUrl);
  sync_pb::EntitySpecifics trimmed =
      bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(entity_specifics);
  EXPECT_FALSE(trimmed.has_theme_android());
}

TEST_F(NtpAndroidThemeSyncBridgeTest, OnStoreCreated_StoreCreationError) {
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor;
  EXPECT_CALL(mock_processor, ReportError(_));

  std::unique_ptr<NtpAndroidThemeSyncBridge> bridge =
      std::make_unique<NtpAndroidThemeSyncBridge>(
          mock_processor.CreateForwardingProcessor(),
          base::BindOnce([](syncer::DataType,
                            syncer::DataTypeStore::InitCallback callback) {
            std::move(callback).Run(
                syncer::ModelError(
                    FROM_HERE, syncer::ModelError::Type::kGenericTestError),
                nullptr);
          }),
          base::BindRepeating(&NtpAndroidThemeSyncBridgeTest::OnThemeChanged,
                              base::Unretained(this)));
}

TEST_F(NtpAndroidThemeSyncBridgeTest, OnDataLoaded_Success) {
  sync_pb::ThemeAndroidSpecifics specifics;
  specifics.mutable_ntp_background()->set_url(kTestImageUrl);

  std::unique_ptr<syncer::DataTypeStore> store =
      syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store->CreateWriteBatch();
  batch->WriteData(kTestStorageKey, specifics.SerializeAsString());
  store->CommitWriteBatch(
      std::move(batch),
      base::BindOnce([](const std::optional<syncer::ModelError>&) {}));

  base::RunLoop run_loop;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor;
  EXPECT_CALL(mock_processor, ModelReadyToSync(_))
      .WillOnce([&run_loop](std::unique_ptr<syncer::MetadataBatch>) {
        run_loop.Quit();
      });

  std::unique_ptr<NtpAndroidThemeSyncBridge> bridge =
      std::make_unique<NtpAndroidThemeSyncBridge>(
          mock_processor.CreateForwardingProcessor(),
          syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store.get()),
          base::BindRepeating(&NtpAndroidThemeSyncBridgeTest::OnThemeChanged,
                              base::Unretained(this)));

  run_loop.Run();

  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge->GetDataForCommit({kTestStorageKey});
  ASSERT_TRUE(data_batch);
  ASSERT_TRUE(data_batch->HasNext());
}

TEST_F(NtpAndroidThemeSyncBridgeTest, OnDataLoaded_CorruptedData) {
  std::unique_ptr<syncer::DataTypeStore> store =
      syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store->CreateWriteBatch();
  batch->WriteData(kTestStorageKey, kTestInvalidData);
  store->CommitWriteBatch(
      std::move(batch),
      base::BindOnce([](const std::optional<syncer::ModelError>&) {}));

  base::RunLoop run_loop;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor;
  EXPECT_CALL(mock_processor, ModelReadyToSync(_))
      .WillOnce([&run_loop](std::unique_ptr<syncer::MetadataBatch>) {
        run_loop.Quit();
      });
  EXPECT_CALL(mock_processor, ReportError(_)).Times(0);

  std::unique_ptr<NtpAndroidThemeSyncBridge> bridge =
      std::make_unique<NtpAndroidThemeSyncBridge>(
          mock_processor.CreateForwardingProcessor(),
          syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store.get()),
          base::BindRepeating(&NtpAndroidThemeSyncBridgeTest::OnThemeChanged,
                              base::Unretained(this)));

  run_loop.Run();
  EXPECT_EQ(0, callback_count_);

  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge->GetDataForCommit({kTestStorageKey});
  ASSERT_TRUE(data_batch);
  EXPECT_FALSE(data_batch->HasNext());
}

}  // namespace
}  // namespace ntp_customization
