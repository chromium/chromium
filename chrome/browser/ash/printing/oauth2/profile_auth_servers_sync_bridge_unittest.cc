// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/profile_auth_servers_sync_bridge.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/test/model/mock_model_type_change_processor.h"
#include "components/sync/test/model/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::printing::oauth2 {
namespace {

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
  base::RunLoop loop;
  CreateBridge();
  EXPECT_CALL(mock_observer_, OnProfileAuthorizationServersInitialized())
      .Times(1)
      .WillOnce([&loop] { loop.Quit(); });
  loop.Run();
}

}  // namespace
}  // namespace ash::printing::oauth2
