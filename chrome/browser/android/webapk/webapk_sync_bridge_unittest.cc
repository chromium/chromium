// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_bridge.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/android/webapk/test/fake_webapk_database_factory.h"
#include "chrome/browser/android/webapk/webapk_database_factory.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "chrome/browser/android/webapk/webapk_registry_update.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/entity_data.h"
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

int64_t UnixTsSecToWindowsTsMsec(double unix_ts_sec) {
  return base::Time::FromSecondsSinceUnixEpoch(unix_ts_sec)
      .ToDeltaSinceWindowsEpoch()
      .InMicroseconds();
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

    std::unique_ptr<base::SimpleTestClock> clock =
        std::make_unique<base::SimpleTestClock>();
    clock->SetNow(base::Time::FromSecondsSinceUnixEpoch(
        1136232245.0));  // Mon Jan 02 2006 15:04:05 GMT-0500

    sync_bridge_ = std::make_unique<WebApkSyncBridge>(
        database_factory_.get(), loop.QuitClosure(),
        mock_processor_.CreateForwardingProcessor(), std::move(clock));
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

TEST_F(WebApkSyncBridgeTest, AppWasUsedRecently) {
  base::test::SingleThreadTaskEnvironment task_environment;
  InitSyncBridge();

  std::unique_ptr<sync_pb::WebApkSpecifics> app1 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app1->set_last_used_time_windows_epoch_micros(UnixTsSecToWindowsTsMsec(
      1136145845.0));  // Sun Jan 01 2006 15:04:05 GMT-0500 - slightly before
                       // clock_.Now() (recent enough)

  std::unique_ptr<sync_pb::WebApkSpecifics> app2 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app2->set_last_used_time_windows_epoch_micros(UnixTsSecToWindowsTsMsec(
      1136318645.0));  // Tue Jan 03 2006 15:04:05 GMT-0500 - slightly after
                       // clock_.Now() (recent enough)

  std::unique_ptr<sync_pb::WebApkSpecifics> app3 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app3->set_last_used_time_windows_epoch_micros(UnixTsSecToWindowsTsMsec(
      1133726645.0));  // Sun Dec 04 2005 15:04:05 GMT-0500 - 29 days before
                       // clock_.Now() (recent enough)

  std::unique_ptr<sync_pb::WebApkSpecifics> app4 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app4->set_last_used_time_windows_epoch_micros(UnixTsSecToWindowsTsMsec(
      1133640244.0));  // Sat Dec 03 2005 15:04:04 GMT-0500 - 30 days and 1
                       // second before clock_.Now() (not recent enough)

  EXPECT_TRUE(sync_bridge().AppWasUsedRecently(app1.get()));
  EXPECT_TRUE(sync_bridge().AppWasUsedRecently(app2.get()));
  EXPECT_TRUE(sync_bridge().AppWasUsedRecently(app3.get()));
  EXPECT_FALSE(sync_bridge().AppWasUsedRecently(app4.get()));
}

TEST_F(WebApkSyncBridgeTest, PrepareSyncUpdateFromInstalledApps) {
  base::test::SingleThreadTaskEnvironment task_environment;
  InitSyncBridge();

  const std::string manifest_id_1 = "https://example.com/app1";
  const std::string manifest_id_2 = "https://example.com/app2";
  const std::string manifest_id_3 = "https://example.com/app3";
  const std::string manifest_id_4 = "https://example.com/app4";
  const std::string manifest_id_5 = "https://example.com/app5";
  const std::string manifest_id_6 = "https://example.com/app6";

  // app1 makes it into the output (it's recent enough and the only matching
  // sync change is a deletion)
  std::unique_ptr<sync_pb::WebApkSpecifics> app1 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app1->set_manifest_id(manifest_id_1);
  app1->set_name("app1_installed");
  app1->set_last_used_time_windows_epoch_micros(UnixTsSecToWindowsTsMsec(
      1136145845.0));  // Sun Jan 01 2006 15:04:05 GMT-0500 - slightly before
                       // clock_.Now() (recent enough)

  // app2 doesn't make it into the output (it's recent enough but there's a
  // newer matching sync change)
  std::unique_ptr<sync_pb::WebApkSpecifics> app2 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app2->set_manifest_id(manifest_id_2);
  app2->set_name("app2_installed");
  app2->set_last_used_time_windows_epoch_micros(UnixTsSecToWindowsTsMsec(
      1133726645.0));  // Sun Dec 04 2005 15:04:05 GMT-0500 - 29 days before
                       // clock_.Now() (recent enough)

  // app3 makes it into the output (it's recent enough and there's a matching
  // sync change, but it's older)
  std::unique_ptr<sync_pb::WebApkSpecifics> app3 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app3->set_manifest_id(manifest_id_3);
  app3->set_name("app3_installed");
  app3->set_last_used_time_windows_epoch_micros(UnixTsSecToWindowsTsMsec(
      1133726645.0));  // Sun Dec 04 2005 15:04:05 GMT-0500 - 29 days before
                       // clock_.Now() (recent enough)

  // app4 doesn't make it into the output (it's not recent enough)
  std::unique_ptr<sync_pb::WebApkSpecifics> app4 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app4->set_manifest_id(manifest_id_4);
  app4->set_name("app4_installed");
  app4->set_last_used_time_windows_epoch_micros(UnixTsSecToWindowsTsMsec(
      1133640244.0));  // Sat Dec 03 2005 15:04:04 GMT-0500 - 30 days and 1
                       // second before clock_.Now() (not recent enough)

  // app5 makes it into the output (it's recent enough and there's no matching
  // sync change)
  std::unique_ptr<sync_pb::WebApkSpecifics> app5 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app5->set_manifest_id(manifest_id_5);
  app5->set_name("app5_installed");
  app5->set_last_used_time_windows_epoch_micros(UnixTsSecToWindowsTsMsec(
      1133726645.0));  // Sun Dec 04 2005 15:04:05 GMT-0500 - 29 days before
                       // clock_.Now() (recent enough)

  std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>> installed_apps;
  installed_apps.push_back(std::move(app1));
  installed_apps.push_back(std::move(app2));
  installed_apps.push_back(std::move(app3));
  installed_apps.push_back(std::move(app4));
  installed_apps.push_back(std::move(app5));

  // app that isn't installed - no effect
  syncer::EntityData sync_data_1;
  sync_pb::WebApkSpecifics* sync_specifics_1 =
      sync_data_1.specifics.mutable_web_apk();
  sync_specifics_1->set_manifest_id(manifest_id_6);
  sync_specifics_1->set_name("app6_sync");
  sync_specifics_1->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1136232245.0));  // Mon Jan 02 2006 15:04:05 GMT-0500
  std::unique_ptr<syncer::EntityChange> sync_change_1 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_6),
                                      std::move(sync_data_1));

  // deletion - no effect
  std::unique_ptr<syncer::EntityChange> sync_change_2 =
      syncer::EntityChange::CreateDelete(ManifestIdStrToAppId(manifest_id_1));

  // app that's installed, but newer - prevents app2 from being in the result
  syncer::EntityData sync_data_3;
  sync_pb::WebApkSpecifics* sync_specifics_3 =
      sync_data_3.specifics.mutable_web_apk();
  sync_specifics_3->set_manifest_id(manifest_id_2);
  sync_specifics_3->set_name("app2_sync");
  sync_specifics_3->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(1136145845.0));  // Sun Jan 01 2006 15:04:05
                                                // GMT-0500 - more recent than
                                                // the installed version
  std::unique_ptr<syncer::EntityChange> sync_change_3 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_2),
                                      std::move(sync_data_3));

  // app that's installed, but older - no effect
  syncer::EntityData sync_data_4;
  sync_pb::WebApkSpecifics* sync_specifics_4 =
      sync_data_4.specifics.mutable_web_apk();
  sync_specifics_4->set_manifest_id(manifest_id_3);
  sync_specifics_4->set_name("app3_sync");
  sync_specifics_4->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1133640244.0));  // Sat Dec 03 2005 15:04:04 GMT-0500 - older than
                           // the installed version
  std::unique_ptr<syncer::EntityChange> sync_change_4 =
      syncer::EntityChange::CreateUpdate(ManifestIdStrToAppId(manifest_id_3),
                                         std::move(sync_data_4));

  syncer::EntityChangeList sync_changes;
  sync_changes.push_back(std::move(sync_change_1));
  sync_changes.push_back(std::move(sync_change_2));
  sync_changes.push_back(std::move(sync_change_3));
  sync_changes.push_back(std::move(sync_change_4));

  std::vector<const sync_pb::WebApkSpecifics*> sync_update_from_installed;
  sync_bridge().PrepareSyncUpdateFromInstalledApps(installed_apps, sync_changes,
                                                   &sync_update_from_installed);

  EXPECT_EQ(3u, sync_update_from_installed.size());

  EXPECT_EQ(manifest_id_1, sync_update_from_installed.at(0)->manifest_id());
  EXPECT_EQ("app1_installed", sync_update_from_installed.at(0)->name());

  EXPECT_EQ(manifest_id_3, sync_update_from_installed.at(1)->manifest_id());
  EXPECT_EQ("app3_installed", sync_update_from_installed.at(1)->name());

  EXPECT_EQ(manifest_id_5, sync_update_from_installed.at(2)->manifest_id());
  EXPECT_EQ("app5_installed", sync_update_from_installed.at(2)->name());
}

TEST_F(WebApkSyncBridgeTest, PrepareRegistryUpdateFromInstalledAndSyncApps) {
  base::test::SingleThreadTaskEnvironment task_environment;

  const std::string manifest_id_1 = "https://example.com/app1";
  const std::string manifest_id_2 = "https://example.com/app2";
  const std::string manifest_id_3 = "https://example.com/app3";
  const std::string manifest_id_4 = "https://example.com/app4";
  const std::string manifest_id_5 = "https://example.com/app5";

  Registry registry;

  std::unique_ptr<WebApkProto> synced_app1 = CreateWebApkProto(manifest_id_5);
  InsertAppIntoRegistry(&registry, std::move(synced_app1));

  database_factory().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync(_)).Times(1);
  InitSyncBridge();

  std::unique_ptr<sync_pb::WebApkSpecifics> app1 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app1->set_manifest_id(manifest_id_1);
  app1->set_name("app1_installed");

  std::unique_ptr<sync_pb::WebApkSpecifics> app2 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app2->set_manifest_id(manifest_id_2);
  app2->set_name("app2_installed");

  std::vector<const sync_pb::WebApkSpecifics*> sync_update_from_installed;
  sync_update_from_installed.push_back(app1.get());
  sync_update_from_installed.push_back(app2.get());

  // same as app1 -> doesn't get included in output
  syncer::EntityData sync_data_1;
  sync_pb::WebApkSpecifics* sync_specifics_1 =
      sync_data_1.specifics.mutable_web_apk();
  sync_specifics_1->set_manifest_id(manifest_id_1);
  sync_specifics_1->set_name("app1_sync");
  std::unique_ptr<syncer::EntityChange> sync_change_1 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_1),
                                      std::move(sync_data_1));

  // same as app2 -> doesn't get included in output
  std::unique_ptr<syncer::EntityChange> sync_change_2 =
      syncer::EntityChange::CreateDelete(ManifestIdStrToAppId(manifest_id_2));

  // not found in sync_update_from_installed -> included in output
  syncer::EntityData sync_data_3;
  sync_pb::WebApkSpecifics* sync_specifics_3 =
      sync_data_3.specifics.mutable_web_apk();
  sync_specifics_3->set_manifest_id(manifest_id_3);
  sync_specifics_3->set_name("app3_sync");
  std::unique_ptr<syncer::EntityChange> sync_change_3 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_3),
                                      std::move(sync_data_3));

  // not found in sync_update_from_installed, but there's no entry in the
  // registry to delete, so we ignore it anyway
  std::unique_ptr<syncer::EntityChange> sync_change_4 =
      syncer::EntityChange::CreateDelete(ManifestIdStrToAppId(manifest_id_4));

  // not found in sync_update_from_installed, but there IS an entry in the
  // registry to delete -> included in output
  std::unique_ptr<syncer::EntityChange> sync_change_5 =
      syncer::EntityChange::CreateDelete(ManifestIdStrToAppId(manifest_id_5));

  syncer::EntityChangeList sync_changes;
  sync_changes.push_back(std::move(sync_change_1));
  sync_changes.push_back(std::move(sync_change_2));
  sync_changes.push_back(std::move(sync_change_3));
  sync_changes.push_back(std::move(sync_change_4));
  sync_changes.push_back(std::move(sync_change_5));

  RegistryUpdateData registry_update_from_installed_and_sync;
  sync_bridge().PrepareRegistryUpdateFromInstalledAndSyncApps(
      sync_update_from_installed, sync_changes,
      &registry_update_from_installed_and_sync);

  EXPECT_EQ(3u, registry_update_from_installed_and_sync.apps_to_create.size());

  EXPECT_TRUE(registry_update_from_installed_and_sync.apps_to_create.at(0)
                  ->is_locally_installed());
  EXPECT_EQ(manifest_id_1,
            registry_update_from_installed_and_sync.apps_to_create.at(0)
                ->sync_data()
                .manifest_id());
  EXPECT_EQ("app1_installed",
            registry_update_from_installed_and_sync.apps_to_create.at(0)
                ->sync_data()
                .name());

  EXPECT_TRUE(registry_update_from_installed_and_sync.apps_to_create.at(1)
                  ->is_locally_installed());
  EXPECT_EQ(manifest_id_2,
            registry_update_from_installed_and_sync.apps_to_create.at(1)
                ->sync_data()
                .manifest_id());
  EXPECT_EQ("app2_installed",
            registry_update_from_installed_and_sync.apps_to_create.at(1)
                ->sync_data()
                .name());

  EXPECT_FALSE(registry_update_from_installed_and_sync.apps_to_create.at(2)
                   ->is_locally_installed());
  EXPECT_EQ(manifest_id_3,
            registry_update_from_installed_and_sync.apps_to_create.at(2)
                ->sync_data()
                .manifest_id());
  EXPECT_EQ("app3_sync",
            registry_update_from_installed_and_sync.apps_to_create.at(2)
                ->sync_data()
                .name());

  EXPECT_EQ(1u, registry_update_from_installed_and_sync.apps_to_delete.size());
  EXPECT_EQ(ManifestIdStrToAppId(manifest_id_5),
            registry_update_from_installed_and_sync.apps_to_delete.at(0));
}

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
