// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/profile_auth_servers_sync_bridge.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {
namespace {

syncer::EntityData ToEntityData(const std::string& uri) {
  syncer::EntityData entity_data;
  entity_data.specifics.mutable_printers_authorization_server()->set_uri(uri);
  entity_data.name = uri;
  return entity_data;
}

class MockProfileAuthServersSyncBridgeObserver
    : public ProfileAuthServersSyncBridge::Observer {
 public:
  MOCK_METHOD(void, OnProfileAuthorizationServersInitialized, (), (override));
  MOCK_METHOD(void,
              OnProfileAuthorizationServersUpdate,
              (std::set<GURL>, std::set<GURL>),
              (override));
};

class PrintingOAuth2ProfileAuthServersSyncBridgeTest : public testing::Test {
 protected:
  PrintingOAuth2ProfileAuthServersSyncBridgeTest() {
    DCHECK(uri_1u_.is_valid());
    DCHECK(uri_2u_.is_valid());
    DCHECK(uri_3u_.is_valid());
    DCHECK(uri_4u_.is_valid());
  }

  void CreateBridge(const std::vector<std::string>& uris = {}) {
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(false));
    SaveToLocalStore(uris);
    bridge_ = ProfileAuthServersSyncBridge::CreateForTesting(
        &mock_observer_, mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));
    base::RunLoop loop;
    EXPECT_CALL(mock_observer_, OnProfileAuthorizationServersInitialized())
        .WillOnce([&loop]() { loop.Quit(); });
    loop.Run();
  }

  void DoInitialMerge(const std::vector<std::string>& records) {
    syncer::EntityChangeList data_change_list;
    for (const std::string& uri : records) {
      data_change_list.push_back(
          syncer::EntityChange::CreateAdd(uri, ToEntityData(uri)));
    }
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    std::optional<syncer::ModelError> error = bridge_->MergeFullSyncData(
        bridge_->CreateMetadataChangeList(), std::move(data_change_list));
    ASSERT_FALSE(error);
  }

  void DoApplyIncrementalSyncChanges(const std::vector<std::string>& added,
                                     const std::vector<std::string>& deleted) {
    syncer::EntityChangeList data_change_list;
    for (const std::string& uri : added) {
      data_change_list.push_back(
          syncer::EntityChange::CreateAdd(uri, ToEntityData(uri)));
    }
    for (const std::string& uri : deleted) {
      data_change_list.push_back(syncer::EntityChange::CreateDelete(uri));
    }
    std::optional<syncer::ModelError> error =
        bridge_->ApplyIncrementalSyncChanges(
            bridge_->CreateMetadataChangeList(), std::move(data_change_list));
    ASSERT_FALSE(error);
  }

  std::vector<std::string> GetAllData() {
    std::unique_ptr<syncer::DataBatch> output =
        bridge_->GetAllDataForDebugging();

    std::vector<std::string> uris;
    while (output->HasNext()) {
      auto [key, data] = output->Next();
      uris.emplace_back(key);
    }
    return uris;
  }

  // Example URIs for testing.
  const std::string uri_1_ = "https://a.b.c/123";
  const std::string uri_2_ = "https://def:123/gh";
  const std::string uri_3_ = "https://xyz/ab";
  const std::string uri_4_ = "https://ala.ma.kota/psa?moze";
  const GURL uri_1u_ = GURL(uri_1_);
  const GURL uri_2u_ = GURL(uri_2_);
  const GURL uri_3u_ = GURL(uri_3_);
  const GURL uri_4u_ = GURL(uri_4_);

  testing::StrictMock<MockProfileAuthServersSyncBridgeObserver> mock_observer_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<ProfileAuthServersSyncBridge> bridge_;

 private:
  void SaveToLocalStore(const std::vector<std::string>& uris) {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    for (const std::string& uri : uris) {
      sync_pb::PrintersAuthorizationServerSpecifics specifics;
      specifics.set_uri(uri);
      batch->WriteData(uri, specifics.SerializeAsString());
    }

    base::RunLoop loop;
    store_->CommitWriteBatch(
        std::move(batch),
        base::BindLambdaForTesting(
            [&loop](const std::optional<syncer::ModelError>& error) {
              DCHECK(!error);
              loop.Quit();
            }));
    loop.Run();
  }

  // In memory data type store needs to be able to post tasks.
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> store_ =
      syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
};

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest, Initialization) {
  EXPECT_CALL(mock_processor_, ModelReadyToSync(testing::_));
  CreateBridge();
}

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest, MergeFullSyncData) {
  EXPECT_CALL(mock_observer_,
              OnProfileAuthorizationServersUpdate(std::set{uri_1u_, uri_3u_},
                                                  std::set<GURL>{}));
  CreateBridge({uri_1_, uri_3_});

  EXPECT_CALL(mock_processor_, Put(uri_3_, testing::_, testing::_));
  EXPECT_CALL(mock_observer_, OnProfileAuthorizationServersUpdate(
                                  std::set{uri_2u_}, std::set<GURL>{}));
  DoInitialMerge({uri_1_, uri_2_});

  const std::vector<std::string> uris = GetAllData();
  EXPECT_EQ(uris, (std::vector{uri_1_, uri_2_, uri_3_}));
}

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest,
       ServersAddedWhenSyncDisabled) {
  CreateBridge();
  bridge_->AddAuthorizationServer(uri_1u_);

  const std::vector<std::string> uris = GetAllData();
  EXPECT_EQ(uris, std::vector{uri_1_});
}

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest,
       ServersAddedAfterInitialMerge) {
  CreateBridge();
  EXPECT_CALL(mock_observer_, OnProfileAuthorizationServersUpdate(
                                  std::set{uri_2u_}, std::set<GURL>{}));
  DoInitialMerge({uri_2_});
  EXPECT_CALL(mock_processor_, Put(uri_1_, testing::_, testing::_));
  bridge_->AddAuthorizationServer(uri_1u_);

  const std::vector<std::string> uris = GetAllData();
  EXPECT_EQ(uris, (std::vector{uri_1_, uri_2_}));
}

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest,
       ServersAddedBeforeInitialMerge) {
  CreateBridge();
  bridge_->AddAuthorizationServer(uri_1u_);
  EXPECT_CALL(mock_processor_, Put(uri_1_, testing::_, testing::_));
  EXPECT_CALL(mock_observer_, OnProfileAuthorizationServersUpdate(
                                  std::set{uri_2u_}, std::set<GURL>{}));
  DoInitialMerge({uri_2_});

  const std::vector<std::string> uris = GetAllData();
  EXPECT_EQ(uris, (std::vector{uri_1_, uri_2_}));
}

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest,
       ApplyIncrementalSyncChanges) {
  CreateBridge();
  DoInitialMerge({});

  EXPECT_CALL(mock_observer_,
              OnProfileAuthorizationServersUpdate(std::set{uri_1u_, uri_2u_},
                                                  std::set<GURL>{}));
  DoApplyIncrementalSyncChanges(/*added=*/{uri_1_, uri_2_}, /*deleted=*/{});
  std::vector<std::string> uris = GetAllData();
  EXPECT_EQ(uris, (std::vector{uri_1_, uri_2_}));

  EXPECT_CALL(mock_observer_, OnProfileAuthorizationServersUpdate(
                                  std::set{uri_3u_}, std::set{uri_1u_}));
  DoApplyIncrementalSyncChanges(/*added=*/{uri_3_}, /*deleted=*/{uri_1_});
  uris = GetAllData();
  EXPECT_EQ(uris, (std::vector{uri_2_, uri_3_}));
}

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest, GetDataForCommit) {
  CreateBridge();
  EXPECT_CALL(mock_observer_,
              OnProfileAuthorizationServersUpdate(std::set{uri_1u_, uri_2u_},
                                                  std::set<GURL>{}));
  DoInitialMerge({uri_1_, uri_2_});

  std::unique_ptr<syncer::DataBatch> output =
      bridge_->GetDataForCommit({uri_1_, uri_3_});

  ASSERT_TRUE(output);
  std::vector<syncer::KeyAndData> data;
  while (output->HasNext()) {
    data.push_back(output->Next());
  }
  ASSERT_EQ(data.size(), 1u);
  EXPECT_EQ(data[0].first, uri_1_);
  ASSERT_TRUE(data[0].second);
  EXPECT_EQ(
      data[0].second->specifics.mutable_printers_authorization_server()->uri(),
      uri_1_);
}

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest,
       OnProfileAuthorizationServersUpdate) {
  CreateBridge();
  EXPECT_CALL(mock_observer_,
              OnProfileAuthorizationServersUpdate(std::set{uri_1u_, uri_2u_},
                                                  std::set<GURL>{}));
  DoInitialMerge({uri_1_, uri_2_});

  EXPECT_CALL(mock_observer_, OnProfileAuthorizationServersUpdate(
                                  std::set{uri_3u_}, std::set{uri_2u_}));
  DoApplyIncrementalSyncChanges(/*added=*/{uri_1_, uri_3_},
                                /*deleted=*/{uri_2_, uri_4_});
}

}  // namespace
}  // namespace ash::printing::oauth2
