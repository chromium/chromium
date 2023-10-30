// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_bridge.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/android/webapk/test/fake_webapk_database_factory.h"
#include "chrome/browser/android/webapk/webapk_database_factory.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace webapk {

bool IsBasicSyncDataEqual(const WebApkProto& expected_app,
                          const syncer::EntityData& entity_data) {
  if (!entity_data.specifics.has_web_apk()) {
    return false;
  }

  const sync_pb::WebApkSpecifics& expected_app_specifics =
      expected_app.sync_data();
  const sync_pb::WebApkSpecifics& entity_specifics =
      entity_data.specifics.web_apk();

  return expected_app_specifics.manifest_id() ==
             entity_specifics.manifest_id() &&
         expected_app_specifics.start_url() == entity_specifics.start_url() &&
         expected_app_specifics.name() == entity_specifics.name();
}

bool RegistryContainsSyncDataBatchChanges(
    const Registry& registry,
    std::unique_ptr<syncer::DataBatch> data_batch) {
  if (!data_batch || !data_batch->HasNext()) {
    return registry.empty();
  }

  while (data_batch->HasNext()) {
    syncer::KeyAndData key_and_data = data_batch->Next();
    auto web_app_iter = registry.find(key_and_data.first);
    if (web_app_iter == registry.end()) {
      LOG(ERROR) << "App not found in registry: " << key_and_data.first;
      return false;
    }

    if (!IsBasicSyncDataEqual(*web_app_iter->second, *key_and_data.second)) {
      return false;
    }
  }
  return true;
}

std::unique_ptr<WebApkProto> CreateWebApkProto(const std::string& url) {
  std::unique_ptr<WebApkProto> web_apk = std::make_unique<WebApkProto>();

  sync_pb::WebApkSpecifics* sync_data = web_apk->mutable_sync_data();
  sync_data->set_manifest_id(url);
  sync_data->set_start_url(url);
  sync_data->set_name("Name");

  return web_apk;
}

void InsertAppIntoRegistry(Registry* registry,
                           std::unique_ptr<WebApkProto> app) {
  webapps::AppId app_id =
      GenerateAppIdFromManifestId(GURL(app->sync_data().manifest_id()));
  ASSERT_FALSE(base::Contains(*registry, app_id));
  registry->emplace(std::move(app_id), std::move(app));
}

class WebApkSyncBridgeTest : public ::testing::Test {
 public:
  void SetUp() override {
    database_factory_ = std::make_unique<FakeWebApkDatabaseFactory>();

    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
  }

  void TearDown() override { DestroyManagers(); }

  void InitSyncBridge() {
    base::RunLoop loop;
    sync_bridge_ = std::make_unique<WebApkSyncBridge>(
        database_factory_.get(), loop.QuitClosure(),
        mock_processor_.CreateForwardingProcessor());
    loop.Run();
  }

 protected:
  void DestroyManagers() {
    if (sync_bridge_) {
      sync_bridge_.reset();
    }
    if (database_factory_) {
      database_factory_.reset();
    }
  }

  syncer::MockModelTypeChangeProcessor& processor() { return mock_processor_; }
  FakeWebApkDatabaseFactory& database_factory() { return *database_factory_; }

  WebApkSyncBridge& sync_bridge() { return *sync_bridge_; }

 private:
  std::unique_ptr<WebApkSyncBridge> sync_bridge_;
  std::unique_ptr<FakeWebApkDatabaseFactory> database_factory_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
};

// Tests that the WebApkSyncBridge correctly reports data from the
// WebApkDatabase.
TEST_F(WebApkSyncBridgeTest, GetData) {
  base::test::SingleThreadTaskEnvironment task_environment;

  Registry registry;

  std::unique_ptr<WebApkProto> synced_app1 =
      CreateWebApkProto("https://example.com/app1/");
  InsertAppIntoRegistry(&registry, std::move(synced_app1));

  std::unique_ptr<WebApkProto> synced_app2 =
      CreateWebApkProto("https://example.com/app2/");
  InsertAppIntoRegistry(&registry, std::move(synced_app2));

  database_factory().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync(_)).Times(1);
  InitSyncBridge();

  {
    WebApkSyncBridge::StorageKeyList storage_keys;
    // Add an unknown key to test this is handled gracefully.
    storage_keys.push_back("unknown");
    for (const Registry::value_type& id_and_web_app : registry) {
      storage_keys.push_back(id_and_web_app.first);
    }

    base::RunLoop run_loop;
    sync_bridge().GetData(
        std::move(storage_keys),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<syncer::DataBatch> data_batch) {
              EXPECT_TRUE(RegistryContainsSyncDataBatchChanges(
                  registry, std::move(data_batch)));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    sync_bridge().GetAllDataForDebugging(base::BindLambdaForTesting(
        [&](std::unique_ptr<syncer::DataBatch> data_batch) {
          EXPECT_TRUE(RegistryContainsSyncDataBatchChanges(
              registry, std::move(data_batch)));
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

// Tests that the client & storage tags are correct for entity data.
TEST_F(WebApkSyncBridgeTest, Identities) {
  // Should be kept up to date with
  // chrome/browser/web_applications/web_app_sync_bridge_unittest.cc's
  // WebAppSyncBridgeTest.Identities test.
  base::test::SingleThreadTaskEnvironment task_environment;
  InitSyncBridge();

  std::unique_ptr<WebApkProto> app = CreateWebApkProto("https://example.com/");
  std::unique_ptr<syncer::EntityData> entity_data = CreateSyncEntityData(*app);

  EXPECT_EQ("ocjeedicdelkkoefdcgeopgiagdjbcng",
            sync_bridge().GetClientTag(*entity_data));
  EXPECT_EQ("ocjeedicdelkkoefdcgeopgiagdjbcng",
            sync_bridge().GetStorageKey(*entity_data));
}

}  // namespace webapk
