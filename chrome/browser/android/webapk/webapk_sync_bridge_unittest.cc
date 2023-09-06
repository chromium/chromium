// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_bridge.h"

#include "components/sync/test/mock_model_type_change_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webapk {

std::unique_ptr<WebApkProto> CreateWebApkProto(const std::string& url) {
  std::unique_ptr<WebApkProto> web_apk = std::make_unique<WebApkProto>();

  sync_pb::WebApkSpecifics* sync_data = web_apk->mutable_sync_data();
  sync_data->set_manifest_id(url);
  sync_data->set_start_url(url);
  sync_data->set_name("Name");

  return web_apk;
}

class WebApkSyncBridgeTest : public ::testing::Test {
 public:
  void SetUp() override {
    sync_bridge_ = std::make_unique<WebApkSyncBridge>(
        mock_processor_.CreateForwardingProcessor());

    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
  }

  void TearDown() override { DestroyManagers(); }

 protected:
  void DestroyManagers() {
    if (sync_bridge_) {
      sync_bridge_.reset();
    }
  }

  syncer::MockModelTypeChangeProcessor& processor() { return mock_processor_; }

  WebApkSyncBridge& sync_bridge() { return *sync_bridge_; }

 private:
  std::unique_ptr<WebApkSyncBridge> sync_bridge_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
};

// Tests that the client & storage tags are correct for entity data.
TEST_F(WebApkSyncBridgeTest, Identities) {
  // Should be kept up to date with
  // chrome/browser/web_applications/web_app_sync_bridge_unittest.cc's
  // WebAppSyncBridgeTest.Identities test.
  std::unique_ptr<WebApkProto> app = CreateWebApkProto("https://example.com/");
  std::unique_ptr<syncer::EntityData> entity_data = CreateSyncEntityData(*app);

  EXPECT_EQ("ocjeedicdelkkoefdcgeopgiagdjbcng",
            sync_bridge().GetClientTag(*entity_data));
  EXPECT_EQ("ocjeedicdelkkoefdcgeopgiagdjbcng",
            sync_bridge().GetStorageKey(*entity_data));
}

}  // namespace webapk
