// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/web_apps_sync_test_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_server_verifier.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

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
      absl::optional<std::string> relative_manifest_id = absl::nullopt) {
    WebApp app(app_id);
    app.SetName(app_id);
    app.SetStartUrl(url);
    app.SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);

    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = app_id;
    app.SetSyncFallbackData(std::move(sync_fallback_data));

    sync_pb::EntitySpecifics entity_specifics;

    *(entity_specifics.mutable_web_app()) = WebAppToSyncProto(app);
    if (relative_manifest_id) {
      entity_specifics.mutable_web_app()->set_relative_manifest_id(
          relative_manifest_id.value());
    } else {
      entity_specifics.mutable_web_app()->clear_relative_manifest_id();
    }

    fake_server_->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"", app_id, entity_specifics, kDefaultTime,
            kDefaultTime));
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       DisablingSelectedTypeDisablesModelType) {
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

  settings->SetSelectedTypes(false, UserSelectableTypeSet());
  ASSERT_FALSE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));
#endif
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       AppWithValidIdSyncInstalled) {
  GURL url("https://example.com/");
  const std::string app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);
  InjectWebAppEntityToFakeServer(app_id, url);
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  auto& web_app_registrar =
      WebAppProvider::GetForTest(GetProfile(0))->registrar_unsafe();
  EXPECT_TRUE(web_app_registrar.IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       AppWithMalformedIdNotSyncInstalled) {
  const std::string app_id = "invalid_id";
  GURL url("https://example.com/");
  InjectWebAppEntityToFakeServer(app_id, url);
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  auto& web_app_registrar =
      WebAppProvider::GetForTest(GetProfile(0))->registrar_unsafe();

  EXPECT_FALSE(web_app_registrar.IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       AppWithIdSpecifiedSyncInstalled) {
  const std::string relative_manifest_id = "explicit_id";
  GURL url("https://example.com/start");
  const std::string app_id = GenerateAppId(relative_manifest_id, url);

  InjectWebAppEntityToFakeServer(app_id, url, relative_manifest_id);
  ASSERT_TRUE(SetupSync());
  AwaitWebAppQuiescence();

  auto& web_app_registrar =
      WebAppProvider::GetForTest(GetProfile(0))->registrar_unsafe();

  EXPECT_TRUE(web_app_registrar.IsInstalled(app_id));

  WebAppInstallInfo info;
  std::string name = "Test name";
  info.title = base::UTF8ToUTF16(app_id);
  info.description = u"Test description";
  info.start_url = url;
  info.scope = url;
  info.manifest_id = GenerateManifestId(relative_manifest_id, url);
  const AppId installed_app_id =
      apps_helper::InstallWebApp(GetProfile(0), info);

  const std::string expected_app_id = GenerateAppId(
      /*manifest_id=*/absl::nullopt, GURL("https://example.com/explicit_id"));
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

  auto& web_app_registrar =
      WebAppProvider::GetForTest(GetProfile(0))->registrar_unsafe();

  EXPECT_TRUE(web_app_registrar.IsInstalled(app_id));

  WebAppInstallInfo info;
  std::string name = "Test name";
  info.title = base::UTF8ToUTF16(app_id);
  info.description = u"Test description";
  info.start_url = url;
  info.scope = url;
  info.manifest_id = GenerateManifestId(relative_manifest_id, url);
  const AppId installed_app_id =
      apps_helper::InstallWebApp(GetProfile(0), info);

  const std::string expected_app_id = GenerateAppId(
      /*manifest_id=*/absl::nullopt, GURL("https://example.com/"));
  EXPECT_EQ(expected_app_id, installed_app_id);
}

}  // namespace
}  // namespace web_app
