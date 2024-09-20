// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/android/webapk/webapk_registrar.h"
#include "chrome/browser/android/webapk/webapk_sync_service.h"
#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/webapks_helper.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"

using ::testing::_;
using testing::AllOf;
using testing::UnorderedElementsAre;

using sync_pb::WebApkSpecifics;

using webapks_helper::LocalAppIdIs;
using webapks_helper::LocalIconInfoIs;
using webapks_helper::LocalIsLocallyInstalledIs;
using webapks_helper::LocalLastUsedTimeWindowsEpochMicrosIs;
using webapks_helper::LocalManifestIdIs;
using webapks_helper::LocalNameIs;
using webapks_helper::LocalScopeIs;
using webapks_helper::LocalStartUrlIs;
using webapks_helper::LocalThemeColorIs;

using webapks_helper::ServerIconInfoIs;
using webapks_helper::ServerLastUsedTimeWindowsEpochMicrosIs;
using webapks_helper::ServerManifestIdIs;
using webapks_helper::ServerNameIs;
using webapks_helper::ServerScopeIs;
using webapks_helper::ServerStartUrlIs;
using webapks_helper::ServerThemeColorIs;

namespace {

std::unique_ptr<WebApkSpecifics> CreateWebApkSpecifics(
    const std::string& manifest_id) {
  std::unique_ptr<WebApkSpecifics> app = std::make_unique<WebApkSpecifics>();
  app->set_manifest_id(manifest_id);
  app->set_name("app name");
  app->set_last_used_time_windows_epoch_micros(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  return app;
}

std::unique_ptr<syncer::LoopbackServerEntity> CreateFakeServerEntity(
    std::unique_ptr<WebApkSpecifics> specifics) {
  sync_pb::EntitySpecifics entity;
  *entity.mutable_web_apk() = *specifics;
  return syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
      /*non_unique_name=*/"",
      /*client_tag=*/
      webapk::ManifestIdStrToAppId(specifics->manifest_id()), entity,
      /*creation_time=*/0,
      /*last_modified_time=*/0);
}

class SingleClientWebApksSyncTest : public SyncTest {
 public:
  SingleClientWebApksSyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitAndEnableFeature(syncer::kWebApkBackupAndRestoreBackend);
  }

  ~SingleClientWebApksSyncTest() override = default;

  bool WaitForServerWebApks(
      testing::Matcher<std::vector<WebApkSpecifics>> matcher) {
    return webapks_helper::ServerWebApkMatchChecker(matcher).Wait();
  }

  bool WaitForLocalWebApks(testing::Matcher<webapk::Registry> matcher) {
    return webapks_helper::LocalWebApkMatchChecker(/*profile_index=*/0,
                                                   GetSyncService(0), matcher)
        .Wait();
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientWebApksSyncTest, UploadsAllFields) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string manifest_id = "https://example.com/app";
  const std::string start_url = "https://example.com/app/start";
  const std::string name = "app";
  const uint32_t theme_color = 12358;
  const std::string scope = "https://example.com/";
  const int64_t last_used_time_windows_epoch_micros = 34632451252;

  const int32_t icon_size_in_px = 256;
  const std::string icon_url = "https://example.com/icon.png";
  const sync_pb::WebApkIconInfo_Purpose icon_purpose =
      sync_pb::WebApkIconInfo_Purpose_MASKABLE;

  std::unique_ptr<WebApkSpecifics> app = std::make_unique<WebApkSpecifics>();
  app->set_manifest_id(manifest_id);
  app->set_start_url(start_url);
  app->set_name(name);
  app->set_theme_color(theme_color);
  app->set_scope(scope);
  app->set_last_used_time_windows_epoch_micros(
      last_used_time_windows_epoch_micros);

  sync_pb::WebApkIconInfo* icon_info = app->add_icon_infos();
  icon_info->set_size_in_px(icon_size_in_px);
  icon_info->set_url(icon_url);
  icon_info->set_purpose(icon_purpose);

  sync_pb::WebApkIconInfo icon_info_copy = *icon_info;

  webapk::WebApkSyncServiceFactory::GetForProfile(GetProfile(0))
      ->OnWebApkUsed(std::move(app), /*is_install=*/false);

  // Note: the local proto says is_locally_installed = true because of the call
  // to OnWebApkUsed().
  EXPECT_TRUE(WaitForLocalWebApks(UnorderedElementsAre(
      AllOf(LocalAppIdIs(webapk::ManifestIdStrToAppId(manifest_id)),
            LocalIsLocallyInstalledIs(true), LocalManifestIdIs(manifest_id),
            LocalStartUrlIs(start_url), LocalNameIs(name),
            LocalThemeColorIs(theme_color), LocalScopeIs(scope),
            LocalLastUsedTimeWindowsEpochMicrosIs(
                last_used_time_windows_epoch_micros),
            LocalIconInfoIs(&icon_info_copy)))));

  EXPECT_TRUE(WaitForServerWebApks(UnorderedElementsAre(AllOf(
      ServerManifestIdIs(manifest_id), ServerStartUrlIs(start_url),
      ServerNameIs(name), ServerThemeColorIs(theme_color), ServerScopeIs(scope),
      ServerLastUsedTimeWindowsEpochMicrosIs(
          last_used_time_windows_epoch_micros),
      ServerIconInfoIs(&icon_info_copy)))));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebApksSyncTest, DownloadsAllFields) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const std::string manifest_id = "https://example.com/app";
  const std::string start_url = "https://example.com/app/start";
  const std::string name = "app";
  const uint32_t theme_color = 12358;
  const std::string scope = "https://example.com/";
  const int64_t last_used_time_windows_epoch_micros =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();

  const int32_t icon_size_in_px = 256;
  const std::string icon_url = "https://example.com/icon.png";
  const sync_pb::WebApkIconInfo_Purpose icon_purpose =
      sync_pb::WebApkIconInfo_Purpose_MASKABLE;

  std::unique_ptr<WebApkSpecifics> app = std::make_unique<WebApkSpecifics>();
  app->set_manifest_id(manifest_id);
  app->set_start_url(start_url);
  app->set_name(name);
  app->set_theme_color(theme_color);
  app->set_scope(scope);
  app->set_last_used_time_windows_epoch_micros(
      last_used_time_windows_epoch_micros);

  sync_pb::WebApkIconInfo* icon_info = app->add_icon_infos();
  icon_info->set_size_in_px(icon_size_in_px);
  icon_info->set_url(icon_url);
  icon_info->set_purpose(icon_purpose);

  sync_pb::WebApkIconInfo icon_info_copy = *icon_info;

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(std::move(app)));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_TRUE(WaitForServerWebApks(UnorderedElementsAre(AllOf(
      ServerManifestIdIs(manifest_id), ServerStartUrlIs(start_url),
      ServerNameIs(name), ServerThemeColorIs(theme_color), ServerScopeIs(scope),
      ServerLastUsedTimeWindowsEpochMicrosIs(
          last_used_time_windows_epoch_micros),
      ServerIconInfoIs(&icon_info_copy)))));

  // Note: the local proto says is_locally_installed = false because syncing the
  // app's data is not the same thing as installing the app.
  EXPECT_TRUE(WaitForLocalWebApks(UnorderedElementsAre(
      AllOf(LocalAppIdIs(webapk::ManifestIdStrToAppId(manifest_id)),
            LocalIsLocallyInstalledIs(false), LocalManifestIdIs(manifest_id),
            LocalStartUrlIs(start_url), LocalNameIs(name),
            LocalThemeColorIs(theme_color), LocalScopeIs(scope),
            LocalLastUsedTimeWindowsEpochMicrosIs(
                last_used_time_windows_epoch_micros),
            LocalIconInfoIs(&icon_info_copy)))));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebApksSyncTest,
                       DoesNotUploadRetroactively) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  webapk::WebApkSyncService* web_apk_sync_service =
      webapk::WebApkSyncServiceFactory::GetForProfile(GetProfile(0));

  // Use a WebAPK before sync is turned on.
  std::unique_ptr<WebApkSpecifics> app1 = std::make_unique<WebApkSpecifics>();
  app1->set_manifest_id("https://example.com/app1");
  app1->set_name("app1");

  web_apk_sync_service->OnWebApkUsed(std::move(app1), /*is_install=*/false);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // After sync was enabled, use another WebAPK.
  const std::string manifest_id_2 = "https://example.com/app2";
  std::unique_ptr<WebApkSpecifics> app2 = std::make_unique<WebApkSpecifics>();
  app2->set_manifest_id(manifest_id_2);
  app2->set_name("app2");

  web_apk_sync_service->OnWebApkUsed(std::move(app2), /*is_install=*/false);

  // Only the WebAPK accessed after sync was turned on is uploaded.
  EXPECT_TRUE(WaitForServerWebApks(
      UnorderedElementsAre(ServerManifestIdIs(manifest_id_2))));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebApksSyncTest,
                       ClearsForeignWebApksOnTurningSyncOff) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const std::string manifest_id = "https://example.com/app";
  GetFakeServer()->InjectEntity(
      CreateFakeServerEntity(CreateWebApkSpecifics(manifest_id)));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_TRUE(WaitForLocalWebApks(
      UnorderedElementsAre(LocalManifestIdIs(manifest_id))));

  // Turn Sync off by removing the primary account.
  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_TRUE(WaitForLocalWebApks(UnorderedElementsAre()));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebApksSyncTest,
                       SetsNeedsPwaRestoreOnFirstSyncWhenDownloadingNewApps) {
  WebappRegistry webapp_registry;
  webapp_registry.SetNeedsPwaRestore(false);
  EXPECT_FALSE(webapp_registry.GetNeedsPwaRestore());

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const std::string manifest_id = "https://example.com/app";
  GetFakeServer()->InjectEntity(
      CreateFakeServerEntity(CreateWebApkSpecifics(manifest_id)));

  EXPECT_FALSE(webapp_registry.GetNeedsPwaRestore());

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_TRUE(WaitForLocalWebApks(
      UnorderedElementsAre(LocalManifestIdIs(manifest_id))));

  EXPECT_TRUE(webapp_registry.GetNeedsPwaRestore());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientWebApksSyncTest,
    DoesNotSetNeedsPwaRestoreOnFirstSyncWhenNotDownloadingNewApps) {
  WebappRegistry webapp_registry;
  webapp_registry.SetNeedsPwaRestore(false);
  EXPECT_FALSE(webapp_registry.GetNeedsPwaRestore());

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_FALSE(webapp_registry.GetNeedsPwaRestore());
}

IN_PROC_BROWSER_TEST_F(SingleClientWebApksSyncTest,
                       RemovesOldAppFromServerOnUninstall) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string manifest_id = "https://example.com/app";

  std::unique_ptr<WebApkSpecifics> app = std::make_unique<WebApkSpecifics>();
  app->set_manifest_id(manifest_id);
  app->set_name("app name");
  app->set_last_used_time_windows_epoch_micros(
      (base::Time::Now() - base::Days(31))
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());

  webapk::WebApkSyncService* web_apk_sync_service =
      webapk::WebApkSyncServiceFactory::GetForProfile(GetProfile(0));

  web_apk_sync_service->OnWebApkUsed(std::move(app), /*is_install=*/false);

  EXPECT_TRUE(WaitForLocalWebApks(
      UnorderedElementsAre(LocalManifestIdIs(manifest_id))));
  EXPECT_TRUE(WaitForServerWebApks(
      UnorderedElementsAre(ServerManifestIdIs(manifest_id))));

  web_apk_sync_service->OnWebApkUninstalled(manifest_id);

  EXPECT_TRUE(WaitForLocalWebApks(UnorderedElementsAre()));
  EXPECT_TRUE(WaitForServerWebApks(UnorderedElementsAre()));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebApksSyncTest,
                       KeepsRecentAppOnServerOnUninstall) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string manifest_id = "https://example.com/app";

  std::unique_ptr<WebApkSpecifics> app = std::make_unique<WebApkSpecifics>();
  app->set_manifest_id(manifest_id);
  app->set_name("app name");
  app->set_last_used_time_windows_epoch_micros(
      base::DefaultClock().Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  webapk::WebApkSyncService* web_apk_sync_service =
      webapk::WebApkSyncServiceFactory::GetForProfile(GetProfile(0));
  web_apk_sync_service->OnWebApkUsed(std::move(app), /*is_install=*/false);

  EXPECT_TRUE(WaitForLocalWebApks(UnorderedElementsAre(
      AllOf(LocalIsLocallyInstalledIs(true), LocalManifestIdIs(manifest_id)))));
  EXPECT_TRUE(WaitForServerWebApks(
      UnorderedElementsAre(ServerManifestIdIs(manifest_id))));

  web_apk_sync_service->OnWebApkUninstalled(manifest_id);

  EXPECT_TRUE(WaitForLocalWebApks(UnorderedElementsAre(AllOf(
      LocalIsLocallyInstalledIs(false), LocalManifestIdIs(manifest_id)))));
  EXPECT_TRUE(WaitForServerWebApks(
      UnorderedElementsAre(ServerManifestIdIs(manifest_id))));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebApksSyncTest, MergesSyncConflicts) {
  // This test shows that newer actions, like a new local install, or an update
  // from a remote device to the sync server, overwrite old ones and get synced
  // bidirectionally. Thus the client and server both end up with the newest
  // available information.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  webapk::WebApkSyncService* web_apk_sync_service =
      webapk::WebApkSyncServiceFactory::GetForProfile(GetProfile(0));

  // Start with 2 distinct apps on the sync server, which get synced to the
  // client.
  const std::string manifest_id_1 = "https://example.com/app1";
  const std::string server_name_1 = "app1 server";
  std::unique_ptr<WebApkSpecifics> server_app_1 =
      std::make_unique<WebApkSpecifics>();
  server_app_1->set_manifest_id(manifest_id_1);
  server_app_1->set_name(server_name_1);
  GetFakeServer()->InjectEntity(
      CreateFakeServerEntity(std::move(server_app_1)));

  const std::string manifest_id_2 = "https://example.com/app2";
  const std::string server_name_2 = "app2 server";
  std::unique_ptr<WebApkSpecifics> server_app_2 =
      std::make_unique<WebApkSpecifics>();
  server_app_2->set_manifest_id(manifest_id_2);
  server_app_2->set_name(server_name_2);
  GetFakeServer()->InjectEntity(
      CreateFakeServerEntity(std::move(server_app_2)));

  EXPECT_TRUE(WaitForLocalWebApks(UnorderedElementsAre(
      AllOf(LocalManifestIdIs(manifest_id_1), LocalNameIs(server_name_1)),
      AllOf(LocalManifestIdIs(manifest_id_2), LocalNameIs(server_name_2)))));
  EXPECT_TRUE(WaitForServerWebApks(UnorderedElementsAre(
      AllOf(ServerManifestIdIs(manifest_id_1), ServerNameIs(server_name_1)),
      AllOf(ServerManifestIdIs(manifest_id_2), ServerNameIs(server_name_2)))));

  // Add 3 apps on client - the first one overwrites one of the existing apps
  // (this will happen if, for example, the app updates its last-used
  // timestamp). The other two apps are new. All 3 changes get synced to the
  // server.
  const std::string client_name_2 = "app2 client";
  std::unique_ptr<WebApkSpecifics> client_app_2 =
      std::make_unique<WebApkSpecifics>();
  client_app_2->set_manifest_id(manifest_id_2);
  client_app_2->set_name(client_name_2);
  web_apk_sync_service->OnWebApkUsed(std::move(client_app_2),
                                     /*is_install=*/false);

  const std::string manifest_id_3 = "https://example.com/app3";
  const std::string client_name_3 = "app3 client";
  std::unique_ptr<WebApkSpecifics> client_app_3 =
      std::make_unique<WebApkSpecifics>();
  client_app_3->set_manifest_id(manifest_id_3);
  client_app_3->set_name(client_name_3);
  web_apk_sync_service->OnWebApkUsed(std::move(client_app_3),
                                     /*is_install=*/false);

  const std::string manifest_id_4 = "https://example.com/app4";
  const std::string client_name_4 = "app4 client";
  std::unique_ptr<WebApkSpecifics> client_app_4 =
      std::make_unique<WebApkSpecifics>();
  client_app_4->set_manifest_id(manifest_id_4);
  client_app_4->set_name(client_name_4);
  web_apk_sync_service->OnWebApkUsed(std::move(client_app_4),
                                     /*is_install=*/false);

  EXPECT_TRUE(WaitForLocalWebApks(UnorderedElementsAre(
      AllOf(LocalManifestIdIs(manifest_id_1), LocalNameIs(server_name_1)),
      AllOf(LocalManifestIdIs(manifest_id_2), LocalNameIs(client_name_2)),
      AllOf(LocalManifestIdIs(manifest_id_3), LocalNameIs(client_name_3)),
      AllOf(LocalManifestIdIs(manifest_id_4), LocalNameIs(client_name_4)))));
  EXPECT_TRUE(WaitForServerWebApks(UnorderedElementsAre(
      AllOf(ServerManifestIdIs(manifest_id_1), ServerNameIs(server_name_1)),
      AllOf(ServerManifestIdIs(manifest_id_2), ServerNameIs(client_name_2)),
      AllOf(ServerManifestIdIs(manifest_id_3), ServerNameIs(client_name_3)),
      AllOf(ServerManifestIdIs(manifest_id_4), ServerNameIs(client_name_4)))));

  // Finally, add an app on the server that overwrites an existing one. Again,
  // this change gets synced back down to the client.
  const std::string server_name_3 = "app3 server";
  std::unique_ptr<WebApkSpecifics> server_app_3 =
      std::make_unique<WebApkSpecifics>();
  server_app_3->set_manifest_id(manifest_id_3);
  server_app_3->set_name(server_name_3);
  GetFakeServer()->InjectEntity(
      CreateFakeServerEntity(std::move(server_app_3)));

  EXPECT_TRUE(WaitForLocalWebApks(UnorderedElementsAre(
      AllOf(LocalManifestIdIs(manifest_id_1), LocalNameIs(server_name_1)),
      AllOf(LocalManifestIdIs(manifest_id_2), LocalNameIs(client_name_2)),
      AllOf(LocalManifestIdIs(manifest_id_3), LocalNameIs(server_name_3)),
      AllOf(LocalManifestIdIs(manifest_id_4), LocalNameIs(client_name_4)))));
  EXPECT_TRUE(WaitForServerWebApks(UnorderedElementsAre(
      AllOf(ServerManifestIdIs(manifest_id_1), ServerNameIs(server_name_1)),
      AllOf(ServerManifestIdIs(manifest_id_2), ServerNameIs(client_name_2)),
      AllOf(ServerManifestIdIs(manifest_id_3), ServerNameIs(server_name_3)),
      AllOf(ServerManifestIdIs(manifest_id_4), ServerNameIs(client_name_4)))));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebApksSyncTest,
                       OldWebApksArePeriodicallyRemovedFromSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Start with 4 sync'd WebAPKs - 2 that have been used within the last 30
  // days, and 2 that haven't.
  const std::string manifest_id_1 = "https://example.com/app1";
  const std::string name_1 = "app1";
  std::unique_ptr<WebApkSpecifics> app_1 = std::make_unique<WebApkSpecifics>();
  app_1->set_manifest_id(manifest_id_1);
  app_1->set_name(name_1);
  app_1->set_last_used_time_windows_epoch_micros(
      (base::Time::Now() - base::Days(500))
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());  // More than a year ago (not recent enough).
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(std::move(app_1)));

  const std::string manifest_id_2 = "https://example.com/app2";
  const std::string name_2 = "app2";
  std::unique_ptr<WebApkSpecifics> app_2 = std::make_unique<WebApkSpecifics>();
  app_2->set_manifest_id(manifest_id_2);
  app_2->set_name(name_2);
  app_2->set_last_used_time_windows_epoch_micros(
      (base::Time::Now() - base::Hours(1))
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());  // Recent enough.
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(std::move(app_2)));

  const std::string manifest_id_3 = "https://example.com/app3";
  const std::string name_3 = "app3";
  std::unique_ptr<WebApkSpecifics> app_3 = std::make_unique<WebApkSpecifics>();
  app_3->set_manifest_id(manifest_id_3);
  app_3->set_name(name_3);
  app_3->set_last_used_time_windows_epoch_micros(
      (base::Time::Now() - base::Days(31))
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());  // Not recent enough.
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(std::move(app_3)));

  const std::string manifest_id_4 = "https://example.com/app4";
  const std::string name_4 = "app4";
  std::unique_ptr<WebApkSpecifics> app_4 = std::make_unique<WebApkSpecifics>();
  app_4->set_manifest_id(manifest_id_4);
  app_4->set_name(name_4);
  app_4->set_last_used_time_windows_epoch_micros(
      (base::Time::Now() - base::Days(29))
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());  // Recent enough.
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(std::move(app_4)));

  // Wait until the data exists on both client and server.
  EXPECT_TRUE(WaitForLocalWebApks(UnorderedElementsAre(
      LocalManifestIdIs(manifest_id_1), LocalManifestIdIs(manifest_id_2),
      LocalManifestIdIs(manifest_id_3), LocalManifestIdIs(manifest_id_4))));
  EXPECT_TRUE(WaitForServerWebApks(UnorderedElementsAre(
      ServerManifestIdIs(manifest_id_1), ServerManifestIdIs(manifest_id_2),
      ServerManifestIdIs(manifest_id_3), ServerManifestIdIs(manifest_id_4))));

  webapk::WebApkSyncService* web_apk_sync_service =
      webapk::WebApkSyncServiceFactory::GetForProfile(GetProfile(0));

  // Note that normally this gets called as a deferred startup task on every
  // Chrome launch on Android:
  // ProcessInitializationHandler.initAsyncDiskTask
  // (ProcessInitializationHandler.java)
  // -> WebappRegistry.unregisterOldWebapps (WebappRegistry.java)
  // -> WebApkSyncService.removeOldWebAPKsFromSync (WebApkSyncService.java)
  // -> JNI -> JNI_WebApkSyncService_RemoveOldWebAPKsFromSync
  // (webapk_sync_service.cc)
  // -> WebApkSyncService::RemoveOldWebAPKsFromSync.
  web_apk_sync_service->RemoveOldWebAPKsFromSync(
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds());

  // Only the recently-used WebAPKs remain.
  EXPECT_TRUE(WaitForLocalWebApks(UnorderedElementsAre(
      LocalManifestIdIs(manifest_id_2), LocalManifestIdIs(manifest_id_4))));
  EXPECT_TRUE(WaitForServerWebApks(UnorderedElementsAre(
      ServerManifestIdIs(manifest_id_2), ServerManifestIdIs(manifest_id_4))));
}

}  // namespace
