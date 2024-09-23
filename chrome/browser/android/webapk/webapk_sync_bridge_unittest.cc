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
#include "chrome/browser/android/webapk/test/fake_data_type_store_service.h"
#include "chrome/browser/android/webapk/test/fake_webapk_specifics_fetcher.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "chrome/browser/android/webapk/webapk_registry_update.h"
#include "chrome/browser/android/webapk/webapk_restore_task.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::IsEmpty;
using testing::SizeIs;

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

std::unique_ptr<WebApkProto> CreateWebApkProto(const std::string& url,
                                               const std::string& name) {
  std::unique_ptr<WebApkProto> web_apk = std::make_unique<WebApkProto>();

  sync_pb::WebApkSpecifics* sync_data = web_apk->mutable_sync_data();
  sync_data->set_manifest_id(url);
  sync_data->set_start_url(url);
  sync_data->set_name(name);

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
    data_type_store_service_ = std::make_unique<FakeDataTypeStoreService>();

    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
  }

  void InitSyncBridge() {
    base::RunLoop loop;

    std::unique_ptr<base::SimpleTestClock> clock =
        std::make_unique<base::SimpleTestClock>();
    clock->SetNow(base::Time::FromSecondsSinceUnixEpoch(
        1136232245.0));  // Mon Jan 02 2006 15:04:05 GMT-0500

    std::unique_ptr<FakeWebApkSpecificsFetcher> specifics_fetcher =
        std::make_unique<FakeWebApkSpecificsFetcher>();
    specifics_fetcher_ = specifics_fetcher.get();

    sync_bridge_ = std::make_unique<WebApkSyncBridge>(
        data_type_store_service_.get(), loop.QuitClosure(),
        mock_processor_.CreateForwardingProcessor(), std::move(clock),
        std::move(specifics_fetcher));

    loop.Run();
  }

 protected:
  syncer::MockDataTypeLocalChangeProcessor& processor() {
    return mock_processor_;
  }
  FakeDataTypeStoreService& data_type_store_service() {
    return *data_type_store_service_;
  }

  WebApkSyncBridge& sync_bridge() { return *sync_bridge_; }
  FakeWebApkSpecificsFetcher& specifics_fetcher() {
    return *specifics_fetcher_;
  }

 private:
  std::unique_ptr<FakeDataTypeStoreService> data_type_store_service_;
  std::unique_ptr<WebApkSyncBridge> sync_bridge_;
  raw_ptr<FakeWebApkSpecificsFetcher>
      specifics_fetcher_;  // owned by sync_bridge_; should not be accessed
                           // before InitSyncBridge() or after sync_bridge_ is
                           // destroyed

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(WebApkSyncBridgeTest,
       ManifestIdStrToAppId_DoesNotCrashOnEmptyStringOrInvalidManifestId) {
  EXPECT_EQ(ManifestIdStrToAppId(""), "");
  EXPECT_EQ(ManifestIdStrToAppId("%%"), "");
}

TEST_F(WebApkSyncBridgeTest, AppWasUsedRecently) {
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

TEST_F(WebApkSyncBridgeTest, PrepareRegistryUpdateFromSyncApps) {
  const std::string manifest_id_1 = "https://example.com/app1";
  const std::string manifest_id_2 = "https://example.com/app2";
  const std::string manifest_id_3 = "https://example.com/app3";
  const std::string manifest_id_4 = "https://example.com/app4";
  const std::string manifest_id_5 = "https://example.com/app5";

  Registry registry;

  std::unique_ptr<WebApkProto> synced_app1 =
      CreateWebApkProto(manifest_id_5, "name");
  InsertAppIntoRegistry(&registry, std::move(synced_app1));

  data_type_store_service().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  InitSyncBridge();

  syncer::EntityData sync_data_1;
  sync_pb::WebApkSpecifics* sync_specifics_1 =
      sync_data_1.specifics.mutable_web_apk();
  sync_specifics_1->set_manifest_id(manifest_id_1);
  sync_specifics_1->set_name("app1_sync");
  std::unique_ptr<syncer::EntityChange> sync_change_1 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_1),
                                      std::move(sync_data_1));

  // There's no entry in the registry to delete, so we ignore it
  std::unique_ptr<syncer::EntityChange> sync_change_2 =
      syncer::EntityChange::CreateDelete(ManifestIdStrToAppId(manifest_id_2));

  syncer::EntityData sync_data_3;
  sync_pb::WebApkSpecifics* sync_specifics_3 =
      sync_data_3.specifics.mutable_web_apk();
  sync_specifics_3->set_manifest_id(manifest_id_3);
  sync_specifics_3->set_name("app3_sync");
  std::unique_ptr<syncer::EntityChange> sync_change_3 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_3),
                                      std::move(sync_data_3));

  // There's no entry in the registry to delete, so we ignore it
  std::unique_ptr<syncer::EntityChange> sync_change_4 =
      syncer::EntityChange::CreateDelete(ManifestIdStrToAppId(manifest_id_4));

  // There IS an entry in the registry to delete -> included in output
  std::unique_ptr<syncer::EntityChange> sync_change_5 =
      syncer::EntityChange::CreateDelete(ManifestIdStrToAppId(manifest_id_5));

  syncer::EntityChangeList sync_changes;
  sync_changes.push_back(std::move(sync_change_1));
  sync_changes.push_back(std::move(sync_change_2));
  sync_changes.push_back(std::move(sync_change_3));
  sync_changes.push_back(std::move(sync_change_4));
  sync_changes.push_back(std::move(sync_change_5));

  RegistryUpdateData registry_update_from_sync;
  sync_bridge().PrepareRegistryUpdateFromSyncApps(sync_changes,
                                                  &registry_update_from_sync);

  EXPECT_EQ(2u, registry_update_from_sync.apps_to_create.size());

  EXPECT_FALSE(
      registry_update_from_sync.apps_to_create.at(0)->is_locally_installed());
  EXPECT_EQ(manifest_id_1, registry_update_from_sync.apps_to_create.at(0)
                               ->sync_data()
                               .manifest_id());
  EXPECT_EQ("app1_sync",
            registry_update_from_sync.apps_to_create.at(0)->sync_data().name());

  EXPECT_FALSE(
      registry_update_from_sync.apps_to_create.at(1)->is_locally_installed());
  EXPECT_EQ(manifest_id_3, registry_update_from_sync.apps_to_create.at(1)
                               ->sync_data()
                               .manifest_id());
  EXPECT_EQ("app3_sync",
            registry_update_from_sync.apps_to_create.at(1)->sync_data().name());

  EXPECT_EQ(1u, registry_update_from_sync.apps_to_delete.size());
  EXPECT_EQ(ManifestIdStrToAppId(manifest_id_5),
            registry_update_from_sync.apps_to_delete.at(0));
}

TEST_F(WebApkSyncBridgeTest, SyncDataContainsNewApps) {
  InitSyncBridge();

  const std::string manifest_id_1 = "https://example.com/app1";
  const std::string manifest_id_2 = "https://example.com/app2";
  const std::string manifest_id_3 = "https://example.com/app3";
  const std::string manifest_id_4 = "https://example.com/app4";

  std::unique_ptr<sync_pb::WebApkSpecifics> app1 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app1->set_manifest_id(manifest_id_1);
  app1->set_name("app1_installed");

  std::unique_ptr<sync_pb::WebApkSpecifics> app2 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app2->set_manifest_id(manifest_id_2);
  app2->set_name("app2_installed");

  std::unique_ptr<sync_pb::WebApkSpecifics> app3 =
      std::make_unique<sync_pb::WebApkSpecifics>();
  app3->set_manifest_id(manifest_id_3);
  app3->set_name("app3_installed");

  std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>> installed_apps;
  installed_apps.push_back(std::move(app1));
  installed_apps.push_back(std::move(app2));
  installed_apps.push_back(std::move(app3));

  syncer::EntityData sync_data_1;
  sync_pb::WebApkSpecifics* sync_specifics_1 =
      sync_data_1.specifics.mutable_web_apk();
  sync_specifics_1->set_manifest_id(manifest_id_1);
  sync_specifics_1->set_name("app1_sync");
  std::unique_ptr<syncer::EntityChange> sync_change_1 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_1),
                                      std::move(sync_data_1));

  syncer::EntityData sync_data_2;
  sync_pb::WebApkSpecifics* sync_specifics_2 =
      sync_data_2.specifics.mutable_web_apk();
  sync_specifics_2->set_manifest_id(manifest_id_2);
  sync_specifics_2->set_name("app2_sync");
  std::unique_ptr<syncer::EntityChange> sync_change_2 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_2),
                                      std::move(sync_data_2));

  syncer::EntityData sync_data_3;
  sync_pb::WebApkSpecifics* sync_specifics_3 =
      sync_data_3.specifics.mutable_web_apk();
  sync_specifics_3->set_manifest_id(manifest_id_3);
  sync_specifics_3->set_name("app3_sync");
  std::unique_ptr<syncer::EntityChange> sync_change_3 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_3),
                                      std::move(sync_data_3));

  syncer::EntityData sync_data_4;
  sync_pb::WebApkSpecifics* sync_specifics_4 =
      sync_data_4.specifics.mutable_web_apk();
  sync_specifics_4->set_manifest_id(manifest_id_4);
  sync_specifics_4->set_name("app4_sync");
  std::unique_ptr<syncer::EntityChange> sync_change_4 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_4),
                                      std::move(sync_data_4));

  syncer::EntityChangeList sync_changes;
  sync_changes.push_back(std::move(sync_change_1));
  sync_changes.push_back(std::move(sync_change_2));

  EXPECT_FALSE(
      sync_bridge().SyncDataContainsNewApps(installed_apps, sync_changes));

  syncer::EntityChangeList sync_changes_2;
  sync_changes_2.push_back(std::move(sync_change_4));  // 4 exists in sync, but
                                                       // not on the device
  sync_changes_2.push_back(std::move(sync_change_3));

  EXPECT_TRUE(
      sync_bridge().SyncDataContainsNewApps(installed_apps, sync_changes_2));
}

TEST_F(WebApkSyncBridgeTest, MergeFullSyncData) {
  // inputs:
  //   sync:
  //     * App1 add oldest
  //     * App2 add new
  //     * App4 add
  //     * App5 delete
  //
  //   registry:
  //     * App1 newest
  //     * App5
  //     * App6
  //
  // outputs:
  //   send to sync:
  //     * App1 middle (installed)
  //     * App6 (registry)
  //
  //   final state (sync, registry, and db):
  //     * App1 middle (installed)
  //     * App2 new (sync)
  //     * App4 (sync)
  //     * App6 (registry)

  const std::string manifest_id_1 = "https://example.com/app1";
  const std::string manifest_id_2 = "https://example.com/app2";
  const std::string manifest_id_3 = "https://example.com/app3";
  const std::string manifest_id_4 = "https://example.com/app4";
  const std::string manifest_id_5 = "https://example.com/app5";
  const std::string manifest_id_6 = "https://example.com/app6";

  Registry registry;

  // not sent to sync or included in final state (installed version always
  // overrides registry version)
  std::unique_ptr<WebApkProto> registry_app1 =
      CreateWebApkProto(manifest_id_1, "app1_registry");
  registry_app1->mutable_sync_data()->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1136145845.0));  // Sun Jan 01 2006 15:04:05 GMT-0500 - newest app1
                           // timestamp
  InsertAppIntoRegistry(&registry, std::move(registry_app1));

  // not sent to sync or included in final state (deleted by sync)
  std::unique_ptr<WebApkProto> registry_app5 =
      CreateWebApkProto(manifest_id_5, "app5_registry");
  InsertAppIntoRegistry(&registry, std::move(registry_app5));

  // sent to sync and included in final state (no other version exists)
  std::unique_ptr<WebApkProto> registry_app6 =
      CreateWebApkProto(manifest_id_6, "app6_registry");
  InsertAppIntoRegistry(&registry, std::move(registry_app6));

  data_type_store_service().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  InitSyncBridge();

  // not included in final state (older than installed version)
  syncer::EntityData sync_data_1;
  sync_pb::WebApkSpecifics* sync_specifics_1 =
      sync_data_1.specifics.mutable_web_apk();
  sync_specifics_1->set_manifest_id(manifest_id_1);
  sync_specifics_1->set_name("app1_sync");
  sync_specifics_1->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1134072245.0));  // Thu Dec 08 2005 15:04:05 GMT-0500 - oldest app1
                           // timestamp
  std::unique_ptr<syncer::EntityChange> sync_change_1 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_1),
                                      std::move(sync_data_1));

  // included in final state (newer than installed version)
  syncer::EntityData sync_data_2;
  sync_pb::WebApkSpecifics* sync_specifics_2 =
      sync_data_2.specifics.mutable_web_apk();
  sync_specifics_2->set_manifest_id(manifest_id_2);
  sync_specifics_2->set_name("app2_sync");
  sync_specifics_2->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1136145845.0));  // Sun Jan 01 2006 15:04:05 GMT-0500 - newest app2
                           // timestamp
  std::unique_ptr<syncer::EntityChange> sync_change_2 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_2),
                                      std::move(sync_data_2));

  // included in final state (no other version exists)
  syncer::EntityData sync_data_4;
  sync_pb::WebApkSpecifics* sync_specifics_4 =
      sync_data_4.specifics.mutable_web_apk();
  sync_specifics_4->set_manifest_id(manifest_id_4);
  sync_specifics_4->set_name("app4_sync");
  std::unique_ptr<syncer::EntityChange> sync_change_4 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_4),
                                      std::move(sync_data_4));

  // causes app5 to be not included in final state (no installed version, only
  // registry)
  std::unique_ptr<syncer::EntityChange> sync_change_5 =
      syncer::EntityChange::CreateDelete(ManifestIdStrToAppId(manifest_id_5));

  syncer::EntityChangeList sync_changes;
  sync_changes.push_back(std::move(sync_change_1));
  sync_changes.push_back(std::move(sync_change_2));
  sync_changes.push_back(std::move(sync_change_4));
  sync_changes.push_back(std::move(sync_change_5));

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();

  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  std::optional<syncer::ModelError> result = sync_bridge().MergeFullSyncData(
      std::move(metadata_change_list), std::move(sync_changes));

  EXPECT_EQ(std::nullopt, result);

  const Registry& final_registry = sync_bridge().GetRegistryForTesting();
  EXPECT_EQ(4u, final_registry.size());

  EXPECT_EQ(manifest_id_1,
            final_registry.at(ManifestIdStrToAppId(manifest_id_1))
                ->sync_data()
                .manifest_id());
  EXPECT_EQ("app1_sync", final_registry.at(ManifestIdStrToAppId(manifest_id_1))
                             ->sync_data()
                             .name());
  EXPECT_FALSE(final_registry.at(ManifestIdStrToAppId(manifest_id_1))
                   ->is_locally_installed());

  EXPECT_EQ(manifest_id_2,
            final_registry.at(ManifestIdStrToAppId(manifest_id_2))
                ->sync_data()
                .manifest_id());
  EXPECT_EQ("app2_sync", final_registry.at(ManifestIdStrToAppId(manifest_id_2))
                             ->sync_data()
                             .name());
  EXPECT_FALSE(final_registry.at(ManifestIdStrToAppId(manifest_id_2))
                   ->is_locally_installed());

  EXPECT_EQ(manifest_id_4,
            final_registry.at(ManifestIdStrToAppId(manifest_id_4))
                ->sync_data()
                .manifest_id());
  EXPECT_EQ("app4_sync", final_registry.at(ManifestIdStrToAppId(manifest_id_4))
                             ->sync_data()
                             .name());
  EXPECT_FALSE(final_registry.at(ManifestIdStrToAppId(manifest_id_4))
                   ->is_locally_installed());

  EXPECT_EQ(manifest_id_6,
            final_registry.at(ManifestIdStrToAppId(manifest_id_6))
                ->sync_data()
                .manifest_id());
  EXPECT_EQ("app6_registry",
            final_registry.at(ManifestIdStrToAppId(manifest_id_6))
                ->sync_data()
                .name());
  EXPECT_FALSE(final_registry.at(ManifestIdStrToAppId(manifest_id_6))
                   ->is_locally_installed());

  const Registry db_registry = data_type_store_service().ReadRegistry();
  EXPECT_EQ(4u, db_registry.size());

  EXPECT_EQ(manifest_id_1, db_registry.at(ManifestIdStrToAppId(manifest_id_1))
                               ->sync_data()
                               .manifest_id());
  EXPECT_EQ(
      "app1_sync",
      db_registry.at(ManifestIdStrToAppId(manifest_id_1))->sync_data().name());
  EXPECT_FALSE(db_registry.at(ManifestIdStrToAppId(manifest_id_1))
                   ->is_locally_installed());

  EXPECT_EQ(manifest_id_2, db_registry.at(ManifestIdStrToAppId(manifest_id_2))
                               ->sync_data()
                               .manifest_id());
  EXPECT_EQ(
      "app2_sync",
      db_registry.at(ManifestIdStrToAppId(manifest_id_2))->sync_data().name());
  EXPECT_FALSE(db_registry.at(ManifestIdStrToAppId(manifest_id_2))
                   ->is_locally_installed());

  EXPECT_EQ(manifest_id_4, db_registry.at(ManifestIdStrToAppId(manifest_id_4))
                               ->sync_data()
                               .manifest_id());
  EXPECT_EQ(
      "app4_sync",
      db_registry.at(ManifestIdStrToAppId(manifest_id_4))->sync_data().name());
  EXPECT_FALSE(db_registry.at(ManifestIdStrToAppId(manifest_id_4))
                   ->is_locally_installed());

  EXPECT_EQ(manifest_id_6, db_registry.at(ManifestIdStrToAppId(manifest_id_6))
                               ->sync_data()
                               .manifest_id());
  EXPECT_EQ(
      "app6_registry",
      db_registry.at(ManifestIdStrToAppId(manifest_id_6))->sync_data().name());
  EXPECT_FALSE(db_registry.at(ManifestIdStrToAppId(manifest_id_6))
                   ->is_locally_installed());
}

TEST_F(WebApkSyncBridgeTest, MergeFullSyncData_NoChanges) {
  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  InitSyncBridge();

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
  syncer::EntityChangeList sync_changes;
  std::optional<syncer::ModelError> result = sync_bridge().MergeFullSyncData(
      std::move(metadata_change_list), std::move(sync_changes));

  EXPECT_EQ(std::nullopt, result);
  EXPECT_EQ(0u, sync_bridge().GetRegistryForTesting().size());
  EXPECT_EQ(0u, data_type_store_service().ReadRegistry().size());
}

TEST_F(WebApkSyncBridgeTest, ApplyIncrementalSyncChanges) {
  const std::string manifest_id_1 = "https://example.com/app1";
  const std::string manifest_id_2 = "https://example.com/app2";
  const std::string manifest_id_3 = "https://example.com/app3";
  const std::string manifest_id_4 = "https://example.com/app4";

  Registry registry;

  // left as-is
  std::unique_ptr<WebApkProto> registry_app1 =
      CreateWebApkProto(manifest_id_1, "app1_registry");
  InsertAppIntoRegistry(&registry, std::move(registry_app1));

  // deleted
  std::unique_ptr<WebApkProto> registry_app2 =
      CreateWebApkProto(manifest_id_2, "app2_registry");
  InsertAppIntoRegistry(&registry, std::move(registry_app2));

  // replaced
  std::unique_ptr<WebApkProto> registry_app3 =
      CreateWebApkProto(manifest_id_3, "app3_registry");
  InsertAppIntoRegistry(&registry, std::move(registry_app3));

  data_type_store_service().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  InitSyncBridge();

  std::unique_ptr<syncer::EntityChange> sync_change_2 =
      syncer::EntityChange::CreateDelete(ManifestIdStrToAppId(manifest_id_2));

  syncer::EntityData sync_data_3;
  sync_pb::WebApkSpecifics* sync_specifics_3 =
      sync_data_3.specifics.mutable_web_apk();
  sync_specifics_3->set_manifest_id(manifest_id_3);
  sync_specifics_3->set_name("app3_sync");
  std::unique_ptr<syncer::EntityChange> sync_change_3 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_3),
                                      std::move(sync_data_3));

  syncer::EntityData sync_data_4;
  sync_pb::WebApkSpecifics* sync_specifics_4 =
      sync_data_4.specifics.mutable_web_apk();
  sync_specifics_4->set_manifest_id(manifest_id_4);
  sync_specifics_4->set_name("app4_sync");
  std::unique_ptr<syncer::EntityChange> sync_change_4 =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id_4),
                                      std::move(sync_data_4));

  syncer::EntityChangeList sync_changes;
  sync_changes.push_back(std::move(sync_change_2));
  sync_changes.push_back(std::move(sync_change_3));
  sync_changes.push_back(std::move(sync_change_4));

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
  std::optional<syncer::ModelError> result =
      sync_bridge().ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                                std::move(sync_changes));

  EXPECT_EQ(std::nullopt, result);

  const Registry& final_registry = sync_bridge().GetRegistryForTesting();
  EXPECT_EQ(3u, final_registry.size());

  EXPECT_EQ(manifest_id_1,
            final_registry.at(ManifestIdStrToAppId(manifest_id_1))
                ->sync_data()
                .manifest_id());
  EXPECT_EQ("app1_registry",
            final_registry.at(ManifestIdStrToAppId(manifest_id_1))
                ->sync_data()
                .name());

  EXPECT_EQ(manifest_id_3,
            final_registry.at(ManifestIdStrToAppId(manifest_id_3))
                ->sync_data()
                .manifest_id());
  EXPECT_EQ("app3_sync", final_registry.at(ManifestIdStrToAppId(manifest_id_3))
                             ->sync_data()
                             .name());

  EXPECT_EQ(manifest_id_4,
            final_registry.at(ManifestIdStrToAppId(manifest_id_4))
                ->sync_data()
                .manifest_id());
  EXPECT_EQ("app4_sync", final_registry.at(ManifestIdStrToAppId(manifest_id_4))
                             ->sync_data()
                             .name());

  const Registry db_registry = data_type_store_service().ReadRegistry();
  EXPECT_EQ(3u, db_registry.size());

  EXPECT_EQ(manifest_id_1, db_registry.at(ManifestIdStrToAppId(manifest_id_1))
                               ->sync_data()
                               .manifest_id());
  EXPECT_EQ(
      "app1_registry",
      db_registry.at(ManifestIdStrToAppId(manifest_id_1))->sync_data().name());

  EXPECT_EQ(manifest_id_3, db_registry.at(ManifestIdStrToAppId(manifest_id_3))
                               ->sync_data()
                               .manifest_id());
  EXPECT_EQ(
      "app3_sync",
      db_registry.at(ManifestIdStrToAppId(manifest_id_3))->sync_data().name());

  EXPECT_EQ(manifest_id_4, db_registry.at(ManifestIdStrToAppId(manifest_id_4))
                               ->sync_data()
                               .manifest_id());
  EXPECT_EQ(
      "app4_sync",
      db_registry.at(ManifestIdStrToAppId(manifest_id_4))->sync_data().name());
}

TEST_F(WebApkSyncBridgeTest, ApplyIncrementalSyncChanges_NoChanges) {
  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  InitSyncBridge();

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
  syncer::EntityChangeList sync_changes;
  std::optional<syncer::ModelError> result =
      sync_bridge().ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                                std::move(sync_changes));

  EXPECT_EQ(std::nullopt, result);
  EXPECT_EQ(0u, sync_bridge().GetRegistryForTesting().size());
  EXPECT_EQ(0u, data_type_store_service().ReadRegistry().size());
}

TEST_F(WebApkSyncBridgeTest, OnWebApkUsed_ReplaceExistingSyncEntry) {
  const std::string manifest_id = "https://example.com/app1";

  Registry registry;

  std::unique_ptr<WebApkProto> registry_app =
      CreateWebApkProto(manifest_id, "registry_app");
  registry_app->mutable_sync_data()->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(1136145845.0));  // Sun Jan 01 2006 15:04:05
                                                // GMT-0500 timestamp
  InsertAppIntoRegistry(&registry, std::move(registry_app));

  data_type_store_service().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Delete).Times(0);

  InitSyncBridge();

  syncer::EntityData sync_data;
  sync_pb::WebApkSpecifics* sync_specifics =
      sync_data.specifics.mutable_web_apk();
  sync_specifics->set_manifest_id(manifest_id);
  sync_specifics->set_name("sync_app1");
  sync_specifics->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1133726645.0));  // Sun Dec 04 2005 15:04:05 GMT-0500 - 29 days before
                           // clock_.Now()
  std::unique_ptr<syncer::EntityChange> sync_change =
      syncer::EntityChange::CreateAdd(ManifestIdStrToAppId(manifest_id),
                                      std::move(sync_data));

  syncer::EntityChangeList sync_changes;
  sync_changes.push_back(std::move(sync_change));

  std::unique_ptr<sync_pb::WebApkSpecifics> used_specifics =
      std::make_unique<sync_pb::WebApkSpecifics>();
  used_specifics->set_manifest_id(manifest_id);
  used_specifics->set_name("used_app1");
  used_specifics->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1136232245.0));  // Mon Jan 02 2006 15:04:05 GMT-0500 (now)

  base::RunLoop run_loop;

  ON_CALL(processor(), Put)
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        EXPECT_EQ(ManifestIdStrToAppId(manifest_id), storage_key);
        EXPECT_EQ(manifest_id, entity_data->specifics.web_apk().manifest_id());
        EXPECT_EQ("used_app1", entity_data->specifics.web_apk().name());
        EXPECT_EQ(UnixTsSecToWindowsTsMsec(1136232245.0),
                  entity_data->specifics.web_apk()
                      .last_used_time_windows_epoch_micros());

        run_loop.Quit();
      });

  sync_bridge().OnWebApkUsed(std::move(used_specifics), false /* is_install */);

  run_loop.Run();

  const Registry& final_registry = sync_bridge().GetRegistryForTesting();
  EXPECT_EQ(1u, final_registry.size());

  EXPECT_EQ(manifest_id, final_registry.at(ManifestIdStrToAppId(manifest_id))
                             ->sync_data()
                             .manifest_id());
  EXPECT_EQ(
      "used_app1",
      final_registry.at(ManifestIdStrToAppId(manifest_id))->sync_data().name());
  EXPECT_EQ(UnixTsSecToWindowsTsMsec(1136232245.0),
            final_registry.at(ManifestIdStrToAppId(manifest_id))
                ->sync_data()
                .last_used_time_windows_epoch_micros());
  EXPECT_TRUE(final_registry.at(ManifestIdStrToAppId(manifest_id))
                  ->is_locally_installed());

  const Registry db_registry = data_type_store_service().ReadRegistry();
  EXPECT_EQ(1u, db_registry.size());

  EXPECT_EQ(manifest_id, db_registry.at(ManifestIdStrToAppId(manifest_id))
                             ->sync_data()
                             .manifest_id());
  EXPECT_EQ(
      "used_app1",
      db_registry.at(ManifestIdStrToAppId(manifest_id))->sync_data().name());
  EXPECT_EQ(UnixTsSecToWindowsTsMsec(1136232245.0),
            db_registry.at(ManifestIdStrToAppId(manifest_id))
                ->sync_data()
                .last_used_time_windows_epoch_micros());
  EXPECT_TRUE(db_registry.at(ManifestIdStrToAppId(manifest_id))
                  ->is_locally_installed());
}

TEST_F(WebApkSyncBridgeTest, OnWebApkUsed_CreateNewSyncEntry) {
  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Delete).Times(0);

  InitSyncBridge();

  const std::string manifest_id = "https://example.com/app2";

  std::unique_ptr<sync_pb::WebApkSpecifics> used_specifics =
      std::make_unique<sync_pb::WebApkSpecifics>();
  used_specifics->set_manifest_id(manifest_id);
  used_specifics->set_name("used_app2");
  used_specifics->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1136232245.0));  // Mon Jan 02 2006 15:04:05 GMT-0500 (now)

  base::RunLoop run_loop;

  ON_CALL(processor(), Put)
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        EXPECT_EQ(ManifestIdStrToAppId(manifest_id), storage_key);
        EXPECT_EQ(manifest_id, entity_data->specifics.web_apk().manifest_id());
        EXPECT_EQ("used_app2", entity_data->specifics.web_apk().name());
        EXPECT_EQ(UnixTsSecToWindowsTsMsec(1136232245.0),
                  entity_data->specifics.web_apk()
                      .last_used_time_windows_epoch_micros());

        run_loop.Quit();
      });

  sync_bridge().OnWebApkUsed(std::move(used_specifics), false /* is_install */);

  run_loop.Run();

  const Registry& final_registry = sync_bridge().GetRegistryForTesting();
  EXPECT_EQ(1u, final_registry.size());

  EXPECT_EQ(manifest_id, final_registry.at(ManifestIdStrToAppId(manifest_id))
                             ->sync_data()
                             .manifest_id());
  EXPECT_EQ(
      "used_app2",
      final_registry.at(ManifestIdStrToAppId(manifest_id))->sync_data().name());
  EXPECT_EQ(UnixTsSecToWindowsTsMsec(1136232245.0),
            final_registry.at(ManifestIdStrToAppId(manifest_id))
                ->sync_data()
                .last_used_time_windows_epoch_micros());
  EXPECT_TRUE(final_registry.at(ManifestIdStrToAppId(manifest_id))
                  ->is_locally_installed());

  const Registry db_registry = data_type_store_service().ReadRegistry();
  EXPECT_EQ(1u, db_registry.size());

  EXPECT_EQ(manifest_id, db_registry.at(ManifestIdStrToAppId(manifest_id))
                             ->sync_data()
                             .manifest_id());
  EXPECT_EQ(
      "used_app2",
      db_registry.at(ManifestIdStrToAppId(manifest_id))->sync_data().name());
  EXPECT_EQ(UnixTsSecToWindowsTsMsec(1136232245.0),
            db_registry.at(ManifestIdStrToAppId(manifest_id))
                ->sync_data()
                .last_used_time_windows_epoch_micros());
  EXPECT_TRUE(db_registry.at(ManifestIdStrToAppId(manifest_id))
                  ->is_locally_installed());
}

TEST_F(WebApkSyncBridgeTest,
       OnWebApkUninstalled_DoesNotCrashOnEmptyStringOrInvalidManifestId) {
  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  InitSyncBridge();

  sync_bridge().OnWebApkUninstalled("");
  sync_bridge().OnWebApkUninstalled("%%");
}

TEST_F(WebApkSyncBridgeTest, OnWebApkUninstalled_AppTooOld) {
  const std::string manifest_id = "https://example.com/app1";

  Registry registry;

  std::unique_ptr<WebApkProto> registry_app =
      CreateWebApkProto(manifest_id, "registry_app");
  registry_app->mutable_sync_data()->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1133640244.0));  // Sat Dec 03 2005 15:04:04 GMT-0500 - 30 days and 1
                           // second before clock_.Now() (not recent enough)
  InsertAppIntoRegistry(&registry, std::move(registry_app));

  data_type_store_service().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Put).Times(0);

  base::RunLoop run_loop;

  ON_CALL(processor(), Delete)
      .WillByDefault([&](const std::string& storage_key,
                         const syncer::DeletionOrigin& origin,
                         syncer::MetadataChangeList* metadata_change_list) {
        EXPECT_EQ(ManifestIdStrToAppId(manifest_id), storage_key);
        run_loop.Quit();
      });

  InitSyncBridge();

  sync_bridge().OnWebApkUninstalled(manifest_id);

  const Registry& final_registry = sync_bridge().GetRegistryForTesting();
  EXPECT_EQ(0u, final_registry.size());

  const Registry db_registry = data_type_store_service().ReadRegistry();
  EXPECT_EQ(0u, db_registry.size());
}

TEST_F(WebApkSyncBridgeTest, OnWebApkUninstalled_AppDoesNotExist) {
  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  InitSyncBridge();

  sync_bridge().OnWebApkUninstalled("https://example.com/app");

  const Registry& final_registry = sync_bridge().GetRegistryForTesting();
  EXPECT_EQ(0u, final_registry.size());

  const Registry db_registry = data_type_store_service().ReadRegistry();
  EXPECT_EQ(0u, db_registry.size());
}

TEST_F(WebApkSyncBridgeTest, OnWebApkUninstalled_AppNewEnough) {
  const std::string manifest_id = "https://example.com/app1";

  Registry registry;

  std::unique_ptr<WebApkProto> registry_app =
      CreateWebApkProto(manifest_id, "registry_app");
  registry_app->mutable_sync_data()->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1136145845.0));  // Sun Jan 01 2006 15:04:05
                           // GMT-0500 timestamp (new enough)
  InsertAppIntoRegistry(&registry, std::move(registry_app));

  data_type_store_service().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  InitSyncBridge();

  sync_bridge().OnWebApkUninstalled(manifest_id);

  const Registry& final_registry = sync_bridge().GetRegistryForTesting();
  EXPECT_EQ(1u, final_registry.size());

  EXPECT_EQ(manifest_id, final_registry.at(ManifestIdStrToAppId(manifest_id))
                             ->sync_data()
                             .manifest_id());
  EXPECT_EQ(
      "registry_app",
      final_registry.at(ManifestIdStrToAppId(manifest_id))->sync_data().name());
  EXPECT_EQ(UnixTsSecToWindowsTsMsec(1136145845.0),  // uninstalling did not
                                                     // update the timestamp
            final_registry.at(ManifestIdStrToAppId(manifest_id))
                ->sync_data()
                .last_used_time_windows_epoch_micros());
  EXPECT_FALSE(final_registry.at(ManifestIdStrToAppId(manifest_id))
                   ->is_locally_installed());

  const Registry db_registry = data_type_store_service().ReadRegistry();
  EXPECT_EQ(1u, db_registry.size());

  EXPECT_EQ(manifest_id, db_registry.at(ManifestIdStrToAppId(manifest_id))
                             ->sync_data()
                             .manifest_id());
  EXPECT_EQ(
      "registry_app",
      db_registry.at(ManifestIdStrToAppId(manifest_id))->sync_data().name());
  EXPECT_EQ(UnixTsSecToWindowsTsMsec(1136145845.0),  // uninstalling did not
                                                     // update the timestamp
            db_registry.at(ManifestIdStrToAppId(manifest_id))
                ->sync_data()
                .last_used_time_windows_epoch_micros());
  EXPECT_FALSE(db_registry.at(ManifestIdStrToAppId(manifest_id))
                   ->is_locally_installed());
}

// Tests that the WebApkSyncBridge correctly reports data from the
// WebApkDatabase.
TEST_F(WebApkSyncBridgeTest, GetData) {
  Registry registry;

  std::unique_ptr<WebApkProto> synced_app1 =
      CreateWebApkProto("https://example.com/app1/", "name1");
  InsertAppIntoRegistry(&registry, std::move(synced_app1));

  std::unique_ptr<WebApkProto> synced_app2 =
      CreateWebApkProto("https://example.com/app2/", "name2");
  InsertAppIntoRegistry(&registry, std::move(synced_app2));

  data_type_store_service().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  InitSyncBridge();

  {
    WebApkSyncBridge::StorageKeyList storage_keys;
    // Add an unknown key to test this is handled gracefully.
    storage_keys.push_back("unknown");
    for (const Registry::value_type& id_and_web_app : registry) {
      storage_keys.push_back(id_and_web_app.first);
    }

    EXPECT_TRUE(RegistryContainsSyncDataBatchChanges(
        registry, sync_bridge().GetDataForCommit(std::move(storage_keys))));
  }

  EXPECT_TRUE(RegistryContainsSyncDataBatchChanges(
      registry, sync_bridge().GetAllDataForDebugging()));
}

// Tests that the client & storage tags are correct for entity data.
TEST_F(WebApkSyncBridgeTest, Identities) {
  // Should be kept up to date with
  // chrome/browser/web_applications/web_app_sync_bridge_unittest.cc's
  // WebAppSyncBridgeTest.Identities test.
  InitSyncBridge();

  std::unique_ptr<WebApkProto> app =
      CreateWebApkProto("https://example.com/", "name");
  std::unique_ptr<syncer::EntityData> entity_data = CreateSyncEntityData(*app);

  EXPECT_EQ("ocjeedicdelkkoefdcgeopgiagdjbcng",
            sync_bridge().GetClientTag(*entity_data));
  EXPECT_EQ("ocjeedicdelkkoefdcgeopgiagdjbcng",
            sync_bridge().GetStorageKey(*entity_data));
}

TEST_F(WebApkSyncBridgeTest, ApplyDisableSyncChanges) {
  Registry registry;

  InsertAppIntoRegistry(
      &registry, CreateWebApkProto("https://example.com/app1", "registry_app"));

  data_type_store_service().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  InitSyncBridge();

  ASSERT_THAT(sync_bridge().GetRegistryForTesting(), SizeIs(1));
  ASSERT_THAT(data_type_store_service().ReadRegistry(), SizeIs(1));

  sync_bridge().ApplyDisableSyncChanges(
      sync_bridge().CreateMetadataChangeList());

  EXPECT_THAT(sync_bridge().GetRegistryForTesting(), IsEmpty());
  EXPECT_THAT(data_type_store_service().ReadRegistry(), IsEmpty());
}

TEST_F(WebApkSyncBridgeTest, RemoveOldWebAPKsFromSync) {
  const std::string manifest_id_1 = "https://example.com/app1";
  const std::string manifest_id_2 = "https://example.com/app2";

  Registry registry;

  std::unique_ptr<WebApkProto> registry_app_1 =
      CreateWebApkProto(manifest_id_1, "registry_app_1");
  registry_app_1->mutable_sync_data()->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(1136145845.0));  // Sun Jan 01 2006 15:04:05
                                                // GMT-0500 timestamp (too old -
                                                // will be deleted from sync)
  registry_app_1->set_is_locally_installed(true);

  std::unique_ptr<WebApkProto> registry_app_2 =
      CreateWebApkProto(manifest_id_2, "registry_app_2");
  registry_app_2->mutable_sync_data()->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(1136946167.0));  // Tue Jan 10 2006 21:22:47
                                                // GMT-0500 timestamp (new
                                                // enough - will not be deleted)
  registry_app_2->set_is_locally_installed(true);

  InsertAppIntoRegistry(&registry, std::move(registry_app_1));
  InsertAppIntoRegistry(&registry, std::move(registry_app_2));

  data_type_store_service().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Put).Times(0);

  base::RunLoop run_loop;

  ON_CALL(processor(), Delete)
      .WillByDefault([&](const std::string& storage_key,
                         const syncer::DeletionOrigin& origin,
                         syncer::MetadataChangeList* metadata_change_list) {
        EXPECT_EQ(ManifestIdStrToAppId(manifest_id_1), storage_key);
        run_loop.Quit();
      });

  InitSyncBridge();

  sync_bridge().RemoveOldWebAPKsFromSync(
      1138933367000);  // Thu Feb 02 2006 21:22:47 GMT-0500 timestamp

  const Registry& final_registry = sync_bridge().GetRegistryForTesting();
  EXPECT_EQ(1u, final_registry.size());

  EXPECT_EQ(manifest_id_2,
            final_registry.at(ManifestIdStrToAppId(manifest_id_2))
                ->sync_data()
                .manifest_id());
  EXPECT_EQ("registry_app_2",
            final_registry.at(ManifestIdStrToAppId(manifest_id_2))
                ->sync_data()
                .name());
  EXPECT_EQ(UnixTsSecToWindowsTsMsec(1136946167.0),
            final_registry.at(ManifestIdStrToAppId(manifest_id_2))
                ->sync_data()
                .last_used_time_windows_epoch_micros());
  EXPECT_TRUE(final_registry.at(ManifestIdStrToAppId(manifest_id_2))
                  ->is_locally_installed());

  const Registry db_registry = data_type_store_service().ReadRegistry();
  EXPECT_EQ(1u, db_registry.size());

  EXPECT_EQ(manifest_id_2, db_registry.at(ManifestIdStrToAppId(manifest_id_2))
                               ->sync_data()
                               .manifest_id());
  EXPECT_EQ(
      "registry_app_2",
      db_registry.at(ManifestIdStrToAppId(manifest_id_2))->sync_data().name());
  EXPECT_EQ(UnixTsSecToWindowsTsMsec(1136946167.0),
            db_registry.at(ManifestIdStrToAppId(manifest_id_2))
                ->sync_data()
                .last_used_time_windows_epoch_micros());
  EXPECT_TRUE(db_registry.at(ManifestIdStrToAppId(manifest_id_2))
                  ->is_locally_installed());
}

TEST_F(WebApkSyncBridgeTest, GetRestorableAppsInfo) {
  InitSyncBridge();

  const GURL manifest_id_1("https://example.com/app1");
  const GURL manifest_id_2("https://example.com/app2");
  const GURL manifest_id_3("https://example.com/app3");
  const GURL manifest_id_4("https://example.com/app4");
  const GURL icon_url("https://example.com/app2/icon");

  std::unique_ptr<WebApkProto> registry_app_1 =
      CreateWebApkProto(manifest_id_1.spec(), "registry_app_1");
  registry_app_1->mutable_sync_data()->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1136145845.0));  // Sun Jan 01 2006 15:04:05 GMT-0500 - slightly
                           // before clock_.Now() (recent enough)
  registry_app_1->set_is_locally_installed(true);

  std::unique_ptr<WebApkProto> registry_app_2 =
      CreateWebApkProto(manifest_id_2.spec(), "registry_app_2");
  auto* icon = registry_app_2->mutable_sync_data()->add_icon_infos();
  icon->set_url(icon_url.spec());
  registry_app_2->mutable_sync_data()->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1136145845.0));  // Sun Jan 01 2006 15:04:05 GMT-0500 - slightly
                           // before clock_.Now() (recent enough)
  registry_app_2->set_is_locally_installed(false);

  std::unique_ptr<WebApkProto> registry_app_3 =
      CreateWebApkProto(manifest_id_3.spec(), "registry_app_3");
  registry_app_3->mutable_sync_data()->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1133726645.0));  // Sun Dec 04 2005 15:04:05 GMT-0500 - 29 days before
                           // clock_.Now() (recent enough)
  registry_app_3->set_is_locally_installed(false);

  std::unique_ptr<WebApkProto> registry_app_4 =
      CreateWebApkProto(manifest_id_4.spec(), "registry_app_4");
  registry_app_4->mutable_sync_data()->set_last_used_time_windows_epoch_micros(
      UnixTsSecToWindowsTsMsec(
          1133640244.0));  // Sat Dec 03 2005 15:04:04 GMT-0500 - 30 days and 1
                           // second before clock_.Now() (not recent enough)
  registry_app_4->set_is_locally_installed(false);

  Registry registry;

  InsertAppIntoRegistry(&registry, std::move(registry_app_1));
  InsertAppIntoRegistry(&registry, std::move(registry_app_2));
  InsertAppIntoRegistry(&registry, std::move(registry_app_3));
  InsertAppIntoRegistry(&registry, std::move(registry_app_4));

  data_type_store_service().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync).Times(1);
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  InitSyncBridge();

  auto result = sync_bridge().GetRestorableAppsShortcutInfo();
  EXPECT_EQ(result.size(), 2u);

  EXPECT_EQ(result[0].app_id, ManifestIdStrToAppId(manifest_id_2.spec()));
  EXPECT_EQ(result[0].shortcut_info->manifest_id, manifest_id_2);
  EXPECT_EQ(result[0].shortcut_info->name, u"registry_app_2");
  EXPECT_EQ(result[0].shortcut_info->best_primary_icon_url, icon_url);

  EXPECT_EQ(result[1].app_id, ManifestIdStrToAppId(manifest_id_3.spec()));
  EXPECT_EQ(result[1].shortcut_info->manifest_id, manifest_id_3);
  EXPECT_EQ(result[1].shortcut_info->name, u"registry_app_3");
  // No icon url specified for apps 3, fallback to use start url as place
  // holder.
  EXPECT_EQ(result[1].shortcut_info->best_primary_icon_url, manifest_id_3);
}

}  // namespace webapk
