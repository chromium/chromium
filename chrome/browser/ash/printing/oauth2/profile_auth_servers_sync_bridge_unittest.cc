// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/profile_auth_servers_sync_bridge.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/printing/uri.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/model/mock_model_type_change_processor.h"
#include "components/sync/test/model/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
};

class PrintingOAuth2ProfileAuthServersSyncBridgeTest : public testing::Test {
 protected:
  PrintingOAuth2ProfileAuthServersSyncBridgeTest() = default;

  void CreateBridge() {
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(false));
    bridge_ = ProfileAuthServersSyncBridge::CreateForTesting(
        &mock_observer_, mock_processor_.CreateForwardingProcessor(),
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
            store_.get()));
    base::RunLoop loop;
    EXPECT_CALL(mock_observer_, OnProfileAuthorizationServersInitialized())
        .Times(1)
        .WillOnce([&loop] { loop.Quit(); });
    loop.Run();
  }

  void DoInitialMerge(const std::vector<std::string>& records) {
    syncer::ModelTypeSyncBridge* bridge = bridge_.get();
    syncer::EntityChangeList data_change_list;
    for (const std::string& uri : records) {
      data_change_list.push_back(
          syncer::EntityChange::CreateAdd(uri, ToEntityData(uri)));
    }
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge->MergeSyncData(bridge->CreateMetadataChangeList(),
                          std::move(data_change_list));
  }

  std::vector<std::string> GetAllData() {
    std::unique_ptr<syncer::DataBatch> output;
    syncer::ModelTypeSyncBridge* bridge = bridge_.get();
    base::RunLoop loop;
    auto callback = [&output, &loop](std::unique_ptr<syncer::DataBatch> data) {
      output = std::move(data);
      loop.Quit();
    };
    bridge->GetAllDataForDebugging(base::BindLambdaForTesting(callback));
    loop.Run();

    std::vector<std::string> uris;
    while (output->HasNext()) {
      auto [key, data] = output->Next();
      uris.emplace_back(key);
    }
    return uris;
  }

  // In memory model type store needs to be able to post tasks.
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::ModelTypeStore> store_ =
      syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
  testing::StrictMock<MockProfileAuthServersSyncBridgeObserver> mock_observer_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<ProfileAuthServersSyncBridge> bridge_;
};

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest, Initialization) {
  CreateBridge();
}

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest, MergeSyncData) {
  const std::string uri_1 = "https://a.b.c/123";
  const std::string uri_2 = "https://d.e.f:123/g/h/i";
  CreateBridge();
  DoInitialMerge({uri_1, uri_2});

  const std::vector<std::string> uris = GetAllData();
  EXPECT_EQ(uris, (std::vector{uri_1, uri_2}));
}

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest,
       ServersAddedWhenSyncDisabled) {
  const std::string uri_1 = "https://a.b.c/123";
  CreateBridge();
  bridge_->AddAuthorizationServer(chromeos::Uri(uri_1));

  const std::vector<std::string> uris = GetAllData();
  EXPECT_EQ(uris, std::vector{uri_1});
}

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest,
       ServersAddedAfterInitialMerge) {
  const std::string uri_1 = "https://a.b.c/123";
  const std::string uri_2 = "https://d.e.f:123/g/h/i";
  CreateBridge();
  DoInitialMerge({uri_2});
  bridge_->AddAuthorizationServer(chromeos::Uri(uri_1));

  const std::vector<std::string> uris = GetAllData();
  EXPECT_EQ(uris, (std::vector{uri_1, uri_2}));
}

TEST_F(PrintingOAuth2ProfileAuthServersSyncBridgeTest,
       ServersAddedBeforeInitialMerge) {
  const std::string uri_1 = "https://a.b.c/123";
  const std::string uri_2 = "https://d.e.f:123/g/h/i";
  CreateBridge();
  bridge_->AddAuthorizationServer(chromeos::Uri(uri_1));
  DoInitialMerge({uri_2});

  const std::vector<std::string> uris = GetAllData();
  EXPECT_EQ(uris, (std::vector{uri_1, uri_2}));
}

}  // namespace
}  // namespace ash::printing::oauth2
