// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/web_apps_sync_test_base.h"
#include "chrome/browser/web_applications/commands/set_user_display_mode_command.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_server_verifier.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;

#if BUILDFLAG(IS_CHROMEOS_ASH)
using syncer::UserSelectableOsType;
using syncer::UserSelectableOsTypeSet;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {
namespace {

// Default time (creation and last modified) used when creating entities.
const int64_t kDefaultTime = 1234L;

class SingleClientWebAppsSyncTest : public WebAppsSyncTestBase {
 public:
  SingleClientWebAppsSyncTest() : WebAppsSyncTestBase(SINGLE_CLIENT) {}
  ~SingleClientWebAppsSyncTest() override = default;

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Apps sync is controlled by a dedicated preference on Lacros,
    // corresponding to the Apps toggle in OS Sync settings. which
    // need to be enabled for this test.
    if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
      syncer::SyncServiceImpl* service = GetSyncService(0);
      syncer::SyncUserSettings* settings = service->GetUserSettings();
      settings->SetAppsSyncEnabledByOs(true);
    }
#endif

    for (Profile* profile : GetAllProfiles()) {
      auto* web_app_provider = WebAppProvider::GetForTest(profile);
      base::RunLoop loop;
      web_app_provider->on_registry_ready().Post(FROM_HERE, loop.QuitClosure());
      loop.Run();
    }
    return true;
  }

  void AwaitWebAppQuiescence() {
    ASSERT_TRUE(apps_helper::AwaitWebAppQuiescence(GetAllProfiles()));
    content::RunAllTasksUntilIdle();
    base::RunLoop run_loop;
    internals::GetShortcutIOTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] { run_loop.Quit(); }));
    run_loop.Run();
    content::RunAllTasksUntilIdle();
  }

  void InjectWebAppEntityToFakeServer(
      const std::string& app_id,
      const GURL& url,
      std::optional<std::string> relative_manifest_id = std::nullopt) {
    sync_pb::EntitySpecifics entity_specifics;
    entity_specifics.mutable_web_app()->set_name(app_id);
    entity_specifics.mutable_web_app()->set_start_url(url.spec());
    if (relative_manifest_id) {
      entity_specifics.mutable_web_app()->set_relative_manifest_id(
          relative_manifest_id.value());
    }

    fake_server_->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"", app_id, entity_specifics, kDefaultTime,
            kDefaultTime));
  }

  int GetNumWebAppsInSync() {
    return GetFakeServer()->GetSyncEntitiesByDataType(syncer::WEB_APPS).size();
  }

  WebAppRegistrar& registrar_unsafe() {
    return WebAppProvider::GetForTest(GetProfile(0))->registrar_unsafe();
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       DisablingSelectedTypeDisablesDataType) {
  ASSERT_TRUE(SetupSync());
  syncer::SyncServiceImpl* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Apps is an OS type on Ash.
  ASSERT_TRUE(
      settings->GetSelectedOsTypes().Has(UserSelectableOsType::kOsApps));
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));

  settings->SetSelectedOsTypes(false, UserSelectableOsTypeSet());
  ASSERT_FALSE(
      settings->GetSelectedOsTypes().Has(UserSelectableOsType::kOsApps));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));
#else  // BUILDFLAG(IS_CHROMEOS_ASH)

  ASSERT_TRUE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Apps sync is controlled by a dedicated preference on Lacros,
  // corresponding to the Apps toggle in OS Sync settings if
  // kSyncChromeOSAppsToggleSharing is enabled. Disabling Apps sync requires
  // disabling Apps toggle in OS.
  if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
    settings->SetAppsSyncEnabledByOs(false);
  } else {
    settings->SetSelectedTypes(false, UserSelectableTypeSet());
  }
#else
  settings->SetSelectedTypes(false, UserSelectableTypeSet());
#endif

  ASSERT_FALSE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));
#endif
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       AppWithValidIdSyncInstalled) {
  GURL url("https://example.com/");
  const std::string app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, url);
  InjectWebAppEntityToFakeServer(app_id, url);
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       SyncInstalledAppDoesNotChangeResponseToSync) {
  GURL url("https://example.com/");
  const std::string app_id =
      GenerateAppId(/*manifest_id_path=*/"manifest-id", url);

  // Create a sync proto with all relevant fields set (so the client won't try
  // to fill in any missing fields).
  sync_pb::WebAppSpecifics synced_web_app;
  synced_web_app.set_start_url(url.spec());
  synced_web_app.set_name(app_id);
  synced_web_app.set_user_display_mode_default(
      sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
  synced_web_app.set_theme_color(SK_ColorRED);
  synced_web_app.set_scope("https://example.com/scope/");
  synced_web_app.set_relative_manifest_id("manifest-id");
  synced_web_app.set_user_display_mode_cros(
      sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);

  sync_pb::WebAppIconInfo icon_info;
  icon_info.set_size_in_px(32);
  icon_info.set_url("https://example.com/icon.png");
  icon_info.set_purpose(sync_pb::WebAppIconInfo_Purpose_ANY);
  *(synced_web_app.add_icon_infos()) = std::move(icon_info);

  sync_pb::EntitySpecifics entity_specifics;
  *(entity_specifics.mutable_web_app()) = synced_web_app;

  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"", app_id, entity_specifics, kDefaultTime,
          kDefaultTime));

  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  // Installed app should store sync data.
  const WebApp* web_app = registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);
  EXPECT_EQ(syncer::WebAppSpecificsToValue(synced_web_app),
            syncer::WebAppSpecificsToValue(web_app->sync_proto()));

  std::string entity_id;
  {
    std::vector<sync_pb::SyncEntity> sync_entities =
        GetFakeServer()->GetSyncEntitiesByDataType(syncer::WEB_APPS);
    ASSERT_EQ(sync_entities.size(), 1u);
    entity_id = sync_entities[0].id_string();
    sync_pb::WebAppSpecifics result_web_app =
        sync_entities[0].specifics().web_app();

    // The client will get the update but should not send out any updates in
    // response. Verify the server data matches what was given to it.
    EXPECT_EQ(syncer::WebAppSpecificsToValue(synced_web_app),
              syncer::WebAppSpecificsToValue(result_web_app));
  }

  // Start listening for incoming changes from sync.
  WebAppTestRegistryObserverAdapter registry_observer{&registrar_unsafe()};
  base::test::TestFuture<const std::vector<const WebApp*>&>
      updated_from_sync_future;
  registry_observer.SetWebAppWillBeUpdatedFromSyncDelegate(
      updated_from_sync_future.GetRepeatingCallback());

  // Modify the entity on the server, simulating another client making a change.
  sync_pb::WebAppSpecifics modified_web_app = synced_web_app;
  modified_web_app.set_user_display_mode_default(
      sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
  modified_web_app.clear_scope();
  modified_web_app.set_theme_color(SK_ColorBLUE);
  sync_pb::EntitySpecifics modified_entity_specifics;
  *(modified_entity_specifics.mutable_web_app()) = modified_web_app;
  GetFakeServer()->ModifyEntitySpecifics(entity_id, modified_entity_specifics);

  // Wait for the client to get the update.
  ASSERT_TRUE(updated_from_sync_future.Wait());
  // Allow the client to finish processing and potentially update sync.
  AwaitWebAppQuiescence();

  // Installed app should have the modified sync data.
  EXPECT_EQ(syncer::WebAppSpecificsToValue(modified_web_app),
            syncer::WebAppSpecificsToValue(web_app->sync_proto()));

  std::vector<sync_pb::SyncEntity> sync_entities =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::WEB_APPS);
  ASSERT_EQ(sync_entities.size(), 1u);
  sync_pb::WebAppSpecifics result_web_app =
      sync_entities[0].specifics().web_app();

  // The client should not send out any updates in response.
  // Verify the server data matches what was given to it.
  EXPECT_EQ(syncer::WebAppSpecificsToValue(modified_web_app),
            syncer::WebAppSpecificsToValue(result_web_app));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest, InstalledAppUpdatesSync) {
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  apps::IconInfo icon(GURL("https://example.com/icon.png"), /*size=*/32);
  GURL app_url("https://example.com/");
  GURL scope("https://example.com/scope/");
  webapps::ManifestId manifest_id("https://example.com/manifest-id");
  std::string app_name = "app name";
  auto install_info = std::make_unique<WebAppInstallInfo>(manifest_id, app_url);
  install_info->scope = scope;
  install_info->title = base::UTF8ToUTF16(app_name);
  install_info->description = base::UTF8ToUTF16(app_name);
  install_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  install_info->install_url = app_url;
  install_info->manifest_icons.push_back(icon);
  install_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  install_info->theme_color = SK_ColorRED;

  const std::string app_id =
      test::InstallWebApp(GetProfile(0), std::move(install_info),
                          /*overwrite_existing_manifest_fields=*/true,
                          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  sync_pb::WebAppSpecifics expected_sync_data;
  expected_sync_data.set_start_url(app_url.spec());
  expected_sync_data.set_name(app_name);
#if BUILDFLAG(IS_CHROMEOS)
  expected_sync_data.set_user_display_mode_cros(
      sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
#else
  expected_sync_data.set_user_display_mode_default(
      sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
#endif
  expected_sync_data.set_theme_color(SK_ColorRED);
  expected_sync_data.set_scope("https://example.com/scope/");
  expected_sync_data.set_relative_manifest_id("manifest-id");
  sync_pb::WebAppIconInfo icon_info;
  icon_info.set_size_in_px(icon.square_size_px.value());
  icon_info.set_url(icon.url.spec());
  icon_info.set_purpose(sync_pb::WebAppIconInfo_Purpose_ANY);
  *(expected_sync_data.add_icon_infos()) = std::move(icon_info);

  // Check the locally-installed app stored the expected sync data.
  const WebApp* web_app = registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);
  EXPECT_EQ(syncer::WebAppSpecificsToValue(expected_sync_data),
            syncer::WebAppSpecificsToValue(web_app->sync_proto()));

  AwaitWebAppQuiescence();

  {
    std::vector<sync_pb::SyncEntity> sync_entities =
        GetFakeServer()->GetSyncEntitiesByDataType(syncer::WEB_APPS);
    ASSERT_EQ(sync_entities.size(), 1u);
    sync_pb::WebAppSpecifics synced_web_app =
        sync_entities[0].specifics().web_app();

    // The client should have sent the data to the server.
    EXPECT_EQ(syncer::WebAppSpecificsToValue(expected_sync_data),
              syncer::WebAppSpecificsToValue(synced_web_app));
  }

  // Make a change to the installed app that affects sync.
  base::test::TestFuture<void> done_future;
  WebAppProvider::GetForTest(GetProfile(0))
      ->command_manager()
      .ScheduleCommand(absl::make_unique<SetUserDisplayModeCommand>(
          app_id, mojom::UserDisplayMode::kBrowser, done_future.GetCallback()));
  ASSERT_TRUE(done_future.Wait());

  AwaitWebAppQuiescence();

  sync_pb::WebAppSpecifics updated_sync_data = expected_sync_data;
#if BUILDFLAG(IS_CHROMEOS)
  updated_sync_data.set_user_display_mode_cros(
      sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
#else
  updated_sync_data.set_user_display_mode_default(
      sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
#endif

  {
    std::vector<sync_pb::SyncEntity> sync_entities =
        GetFakeServer()->GetSyncEntitiesByDataType(syncer::WEB_APPS);
    ASSERT_EQ(sync_entities.size(), 1u);
    sync_pb::WebAppSpecifics synced_web_app =
        sync_entities[0].specifics().web_app();

    // The client should have sent the data to the server.
    EXPECT_EQ(syncer::WebAppSpecificsToValue(updated_sync_data),
              syncer::WebAppSpecificsToValue(synced_web_app));
  }
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       SyncUpdateWithDifferentStartUrl_UpdatesSyncNotApp) {
  // Install a web app.
  GURL start_url("https://example.com/");
  std::string manifest_id_path = "manifest-id";
  const std::string app_id = GenerateAppId(manifest_id_path, start_url);
  InjectWebAppEntityToFakeServer(app_id, start_url, manifest_id_path);
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  // start_url should have been set in the app and sync proto.
  const WebApp* web_app = registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);
  EXPECT_EQ(web_app->start_url(), start_url);
  EXPECT_EQ(web_app->sync_proto().start_url(), start_url.spec());

  // Create a sync proto with an updated start_url for the same app.
  GURL updated_start_url("https://example.com/updated/");
  std::vector<sync_pb::SyncEntity> sync_entities =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::WEB_APPS);
  ASSERT_EQ(sync_entities.size(), 1u);
  std::string entity_id = sync_entities[0].id_string();
  sync_pb::EntitySpecifics modified_entity_specifics =
      sync_entities[0].specifics();
  modified_entity_specifics.mutable_web_app()->set_start_url(
      updated_start_url.spec());

  // Start listening for incoming changes from sync.
  WebAppTestRegistryObserverAdapter registry_observer{&registrar_unsafe()};
  base::test::TestFuture<const std::vector<const WebApp*>&>
      updated_from_sync_future;
  registry_observer.SetWebAppWillBeUpdatedFromSyncDelegate(
      updated_from_sync_future.GetRepeatingCallback());

  // Modify the entity on the server, simulating another client receiving a
  // new/different manifest.
  GetFakeServer()->ModifyEntitySpecifics(entity_id, modified_entity_specifics);

  // Wait for the client to get the update and process.
  ASSERT_TRUE(updated_from_sync_future.Wait());
  AwaitWebAppQuiescence();

  // The web app should keep its original start_url (because different clients
  // may receive different manifests), but its sync proto should have been
  // updated.
  EXPECT_EQ(web_app->start_url(), start_url);
  EXPECT_EQ(web_app->sync_proto().start_url(), updated_start_url.spec());
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       AppWithMalformedIdNotSyncInstalled) {
  const std::string app_id = "invalid_id";
  GURL url("https://example.com/");
  InjectWebAppEntityToFakeServer(app_id, url);
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       AppWithIdSpecifiedSyncInstalled) {
  const std::string relative_manifest_id = "explicit_id";
  GURL url("https://example.com/start");
  const std::string app_id = GenerateAppId(relative_manifest_id, url);

  InjectWebAppEntityToFakeServer(app_id, url, relative_manifest_id);
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id));

  auto manifest_id = GenerateManifestId(relative_manifest_id, url);
  auto info = std::make_unique<WebAppInstallInfo>(manifest_id, url);
  info->title = base::UTF8ToUTF16(app_id);
  info->description = u"Test description";
  info->scope = url;
  const webapps::AppId installed_app_id =
      apps_helper::InstallWebApp(GetProfile(0), std::move(info));

  const std::string expected_app_id = GenerateAppId(
      /*manifest_id_path=*/std::nullopt,
      GURL("https://example.com/explicit_id"));
  EXPECT_EQ(expected_app_id, installed_app_id);
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       AppWithIdSpecifiedAsEmptyStringSyncInstalled) {
  const std::string relative_manifest_id = "";
  GURL url("https://example.com/start");
  const std::string app_id = GenerateAppId(relative_manifest_id, url);

  InjectWebAppEntityToFakeServer(app_id, url, relative_manifest_id);
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id));

  auto manifest_id = GenerateManifestId(relative_manifest_id, url);
  auto info = std::make_unique<WebAppInstallInfo>(manifest_id, url);
  info->title = base::UTF8ToUTF16(app_id);
  info->description = u"Test description";
  info->scope = url;
  const webapps::AppId installed_app_id =
      apps_helper::InstallWebApp(GetProfile(0), std::move(info));

  const std::string expected_app_id = GenerateAppId(
      /*manifest_id_path=*/std::nullopt, GURL("https://example.com/"));
  EXPECT_EQ(expected_app_id, installed_app_id);
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       AppWithFragmentInManifestIdSyncInstalled) {
  base::HistogramTester histogram_tester;
  const std::string relative_manifest_id = "explicit_id#fragment";
  const std::string stripped_manifest_id = "explicit_id";
  GURL url("https://example.com/start");
  const std::string app_id = GenerateAppId(relative_manifest_id, url);
  // Sanity check GenerateAppId strips the fragment part.
  EXPECT_EQ(app_id, GenerateAppId(stripped_manifest_id, url));

  InjectWebAppEntityToFakeServer(app_id, url, relative_manifest_id);
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id));
  const WebApp* web_app = registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  // Fragment part of ID should have been stripped off.
  EXPECT_EQ(web_app->manifest_id(),
            webapps::ManifestId("https://example.com/explicit_id"));
  EXPECT_EQ(web_app->sync_proto().relative_manifest_id(), stripped_manifest_id);

  histogram_tester.ExpectUniqueSample(
      "WebApp.ApplySyncDataToApp.ManifestIdMatch", false, 1);
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       NoDisplayModeMeansStandalone) {
  GURL url("https://example.com/start");
  const std::string app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, url);

  InjectWebAppEntityToFakeServer(app_id, url);
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id));
  EXPECT_EQ(registrar_unsafe().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest, InvalidStartUrl) {
  ASSERT_TRUE(SetupClients());
  EXPECT_EQ(0, GetNumWebAppsInSync());

  GURL url("https://example.com/start");
  const std::string app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, url);
  InjectWebAppEntityToFakeServer(app_id, GURL());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id));

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Sync.InvalidEntity"),
              base::BucketsAre(
                  base::Bucket(StorageKeyParseResult::kInvalidStartUrl, 1)));
  // Since this makes the entity not parse-able for an AppId, the entity cannot
  // be deleted yet from Sync.
  EXPECT_EQ(1, GetNumWebAppsInSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest, NoStartUrl) {
  ASSERT_TRUE(SetupClients());
  EXPECT_EQ(0, GetNumWebAppsInSync());

  GURL url("https://example.com/start");
  const std::string app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, url);

  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_web_app()->set_name(app_id);
  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"", app_id, entity_specifics, kDefaultTime,
          kDefaultTime));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id));

  std::vector<sync_pb::SyncEntity> server_apps =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::WEB_APPS);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.Sync.InvalidEntity"),
      base::BucketsAre(base::Bucket(StorageKeyParseResult::kNoStartUrl, 1)));
  // Since this makes the entity not parse-able for an AppId, the entity cannot
  // be deleted yet from Sync.
  EXPECT_EQ(1, GetNumWebAppsInSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest, InvalidManifestId) {
  ASSERT_TRUE(SetupClients());
  EXPECT_EQ(0, GetNumWebAppsInSync());

  GURL url("https://example.com/start");
  const std::string app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, url);

  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_web_app()->set_name(app_id);
  entity_specifics.mutable_web_app()->set_start_url("about:blank");
  entity_specifics.mutable_web_app()->set_relative_manifest_id("");
  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"", app_id, entity_specifics, kDefaultTime,
          kDefaultTime));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id));

  std::vector<sync_pb::SyncEntity> server_apps =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::WEB_APPS);

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Sync.InvalidEntity"),
              base::BucketsAre(
                  base::Bucket(StorageKeyParseResult::kInvalidManifestId, 1)));
  // Since this makes the entity not parse-able for an AppId, the entity cannot
  // be deleted yet from Sync.
  EXPECT_EQ(1, GetNumWebAppsInSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       InstalledAppsDontEnterSync) {
  ASSERT_TRUE(SetupClients());
  EXPECT_EQ(0, GetNumWebAppsInSync());

  const std::string app_id = test::InstallDummyWebApp(
      GetProfile(0), "app name", GURL("https://example.com/"),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  if (base::FeatureList::IsEnabled(
          features::kWebAppDontAddExistingAppsToSync)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // On Chrome OS it is not possible to install apps before signing in to
    // sync. So in that case we do expect the app to exist in sync.
    EXPECT_EQ(1, GetNumWebAppsInSync());
#else
    EXPECT_EQ(0, GetNumWebAppsInSync());
#endif
  } else {
    EXPECT_EQ(1, GetNumWebAppsInSync());
  }
}

}  // namespace
}  // namespace web_app
