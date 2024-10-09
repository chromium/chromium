// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/web_apps_sync_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_system.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace web_app {
namespace {

using testing::ElementsAreArray;
using testing::Not;

std::unique_ptr<KeyedService> CreateFakeWebAppProvider(Profile* profile) {
  auto provider = std::make_unique<FakeWebAppProvider>(profile);
  provider->SetOsIntegrationManager(std::make_unique<FakeOsIntegrationManager>(
      profile, /*file_handler_manager=*/nullptr,
      /*protocol_handling_manager=*/nullptr));
  provider->StartWithSubsystems();
  DCHECK(provider);
  return provider;
}

class TwoClientWebAppsBMOSyncTest : public WebAppsSyncTestBase {
 public:
  TwoClientWebAppsBMOSyncTest()
      : WebAppsSyncTestBase(TWO_CLIENT),
        fake_web_app_provider_creator_(
            base::BindRepeating(&CreateFakeWebAppProvider)) {}

  TwoClientWebAppsBMOSyncTest(const TwoClientWebAppsBMOSyncTest&) = delete;
  TwoClientWebAppsBMOSyncTest& operator=(const TwoClientWebAppsBMOSyncTest&) =
      delete;

  ~TwoClientWebAppsBMOSyncTest() override = default;

  bool SetupClients() override {
    bool result = SyncTest::SetupClients();
    if (!result) {
      return result;
    }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Apps sync is controlled by a dedicated preference on Lacros,
    // corresponding to the Apps toggle in OS Sync settings.
    // Enable the Apps toggle for both clients.
    if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
      GetSyncService(0)->GetUserSettings()->SetAppsSyncEnabledByOs(true);
      GetSyncService(1)->GetUserSettings()->SetAppsSyncEnabledByOs(true);
    }
#endif

    for (Profile* profile : GetAllProfiles()) {
      web_app::test::WaitUntilWebAppProviderAndSubsystemsReady(
          WebAppProvider::GetForTest(profile));
    }
    return true;
  }

  bool AwaitWebAppQuiescence() {
    return apps_helper::AwaitWebAppQuiescence(GetAllProfiles());
  }

  GURL GetUserInitiatedAppURL() const {
    return embedded_test_server()->GetURL("/web_apps/basic.html");
  }

  GURL GetUserInitiatedAppURL2() const {
    return embedded_test_server()->GetURL("/web_apps/no_service_worker.html");
  }

  webapps::AppId InstallAppAsUserInitiated(
      Profile* profile,
      webapps::WebappInstallSource source =
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      GURL start_url = GURL()) {
    Browser* browser = CreateBrowser(profile);
    if (!start_url.is_valid()) {
      start_url = GetUserInitiatedAppURL();
    }
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, start_url));

    webapps::AppId app_id;
    base::RunLoop run_loop;
    auto* provider = WebAppProvider::GetForTest(profile);
    provider->scheduler().FetchManifestAndInstall(
        source,
        browser->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
        base::BindOnce(test::TestAcceptDialogCallback),
        base::BindLambdaForTesting([&](const webapps::AppId& new_app_id,
                                       webapps::InstallResultCode code) {
          EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
          app_id = new_app_id;
          run_loop.Quit();
        }),
        FallbackBehavior::kAllowFallbackDataAlways);
    run_loop.Run();
    return app_id;
  }

  webapps::AppId InstallApp(std::unique_ptr<WebAppInstallInfo> info,
                            Profile* profile) {
    GURL start_url = info->start_url();
    std::u16string title = info->title;

    base::RunLoop run_loop;
    webapps::AppId app_id;
    auto* provider = WebAppProvider::GetForTest(profile);
    provider->scheduler().InstallFromInfoNoIntegrationForTesting(
        std::move(info),
        /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindLambdaForTesting(
            [&run_loop, &app_id](const webapps::AppId& new_app_id,
                                 webapps::InstallResultCode code) {
              DCHECK_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
              app_id = new_app_id;
              run_loop.Quit();
            }));

    run_loop.Run();

    const WebAppRegistrar& registrar = GetRegistrar(profile);
    EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), title);
    EXPECT_EQ(registrar.GetAppStartUrl(app_id), start_url);

    return app_id;
  }

  const WebAppRegistrar& GetRegistrar(Profile* profile) {
    return WebAppProvider::GetForTest(profile)->registrar_unsafe();
  }

  FakeOsIntegrationManager& GetOsIntegrationManager(Profile* profile) {
    return reinterpret_cast<FakeOsIntegrationManager&>(
        WebAppProvider::GetForTest(profile)->os_integration_manager());
  }

  extensions::AppSorting* GetAppSorting(Profile* profile) {
    return extensions::ExtensionSystem::Get(profile)->app_sorting();
  }

  std::vector<webapps::AppId> GetAllAppIdsForProfile(Profile* profile) {
    return GetRegistrar(profile).GetAppIds();
  }

 private:
  FakeWebAppProviderCreator fake_web_app_provider_creator_;
};

// Test is flaky (crbug.com/1097050)
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       DISABLED_SyncDoubleInstallation) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));

  // Install web app to both profiles.
  webapps::AppId app_id = InstallAppAsUserInitiated(GetProfile(0));
  webapps::AppId app_id2 = InstallAppAsUserInitiated(GetProfile(1));

  EXPECT_EQ(app_id, app_id2);

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitWebAppQuiescence());

  EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       SyncDoubleInstallationDifferentNames) {
  ASSERT_TRUE(SetupClients());

  // Install web app to both profiles.
  webapps::AppId app_id = test::InstallDummyWebApp(
      GetProfile(0), "Test name", GURL("http://www.chromium.org/path"));
  // The web app has a different title on the second profile.
  webapps::AppId app_id2 = test::InstallDummyWebApp(
      GetProfile(1), "Test name 2", GURL("http://www.chromium.org/path"));

  EXPECT_EQ(app_id, app_id2);

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitWebAppQuiescence());

  EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  // The titles should respect the installation, even though the sync system
  // would only have one name.
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppShortName(app_id), "Test name");
  EXPECT_EQ(GetRegistrar(GetProfile(1)).GetAppShortName(app_id), "Test name 2");
}

// Flaky, see crbug.com/1126404.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SyncDoubleInstallationDifferentUserDisplayMode \
  DISABLED_SyncDoubleInstallationDifferentUserDisplayMode
#else
#define MAYBE_SyncDoubleInstallationDifferentUserDisplayMode \
  SyncDoubleInstallationDifferentUserDisplayMode
#endif
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       MAYBE_SyncDoubleInstallationDifferentUserDisplayMode) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));

  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("http://www.chromium.org/path"));
  info->title = u"Test name";
  info->user_display_mode = mojom::UserDisplayMode::kStandalone;

  // Install web app to both profiles.
  webapps::AppId app_id = InstallApp(
      std::make_unique<WebAppInstallInfo>(info->Clone()), GetProfile(0));
  // The web app has a different open on the second profile.
  info->user_display_mode = mojom::UserDisplayMode::kBrowser;
  webapps::AppId app_id2 = InstallApp(std::move(info), GetProfile(1));

  EXPECT_EQ(app_id, app_id2);

  ASSERT_TRUE(AwaitWebAppQuiescence());

  EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));

  // The user display setting is syned, so these should match. However, the
  // actual value here is racy.
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppUserDisplayMode(app_id),
            GetRegistrar(GetProfile(1)).GetAppUserDisplayMode(app_id));
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, DisplayMode) {
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  ASSERT_TRUE(embedded_test_server()->Start());

  WebAppTestInstallObserver install_observer(GetProfile(1));
  WebAppTestInstallWithOsHooksObserver install_observer_with_os_hooks(
      GetProfile(1));
  install_observer.BeginListening();
  install_observer_with_os_hooks.BeginListening();
  // Install web app to profile 0 and wait for it to sync to profile 1.
  webapps::AppId app_id = InstallAppAsUserInitiated(GetProfile(0));
  if (AreAppsLocallyInstalledBySync()) {
    EXPECT_EQ(install_observer_with_os_hooks.Wait(), app_id);
  } else {
    EXPECT_EQ(install_observer.Wait(), app_id);
  }
  WebAppProvider::GetForTest(GetProfile(1))
      ->sync_bridge_unsafe()
      .SetAppUserDisplayModeForTesting(app_id,
                                       mojom::UserDisplayMode::kBrowser);

  ASSERT_TRUE(AwaitWebAppQuiescence());

  EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));

  // The change should have synced to profile 0.
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kBrowser);
  // The user display settings is synced, so it should match.
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppUserDisplayMode(app_id),
            GetRegistrar(GetProfile(1)).GetAppUserDisplayMode(app_id));
}

// Although the logic is allowed to be racy, the profiles should still end up
// with the same web app ids.
#if BUILDFLAG(IS_WIN)
// Flaky on windows, https://crbug.com/1111533
#define MAYBE_DoubleInstallWithUninstall DISABLED_DoubleInstallWithUninstall
#else
#define MAYBE_DoubleInstallWithUninstall DoubleInstallWithUninstall
#endif
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       MAYBE_DoubleInstallWithUninstall) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install web app to both profiles.
  webapps::AppId app_id = InstallAppAsUserInitiated(GetProfile(0));
  webapps::AppId app_id2 = InstallAppAsUserInitiated(GetProfile(1));
  EXPECT_EQ(app_id, app_id2);

  // Uninstall the app from one of the profiles.
  test::UninstallWebApp(GetProfile(0), app_id);

  ASSERT_TRUE(AwaitWebAppQuiescence());

  // The apps should either be installed on both or uninstalled on both. This
  // fails, hence disabled test.
  EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, NotSynced) {
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a non-syncing web app.
  webapps::AppId app_id = InstallAppAsUserInitiated(
      GetProfile(0), webapps::WebappInstallSource::EXTERNAL_DEFAULT);

  ASSERT_TRUE(AwaitWebAppQuiescence());

  // Profile 0 should have an extra unsynced app, and it should not be in
  // profile 1.
  EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              Not(ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1)))));
  EXPECT_FALSE(GetRegistrar(GetProfile(1)).IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, NotSyncedThenSynced) {
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a non-syncing web app.
  webapps::AppId app_id = InstallAppAsUserInitiated(
      GetProfile(0), webapps::WebappInstallSource::EXTERNAL_DEFAULT);

  // Install the same app as a syncing app on profile 1.
  webapps::AppId app_id2 = InstallAppAsUserInitiated(GetProfile(1));
  EXPECT_EQ(app_id, app_id2);

  ASSERT_TRUE(AwaitWebAppQuiescence());

  // The app is in both profiles.
  EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));

  // The app should have synced from profile 0 to profile 1, which enables sync
  // on profile 0. So changes should propagate from profile 0 to profile 1 now.
  WebAppProvider::GetForTest(GetProfile(0))
      ->sync_bridge_unsafe()
      .SetAppUserDisplayModeForTesting(app_id,
                                       mojom::UserDisplayMode::kBrowser);

  ASSERT_TRUE(AwaitWebAppQuiescence());

  // Check that profile 1 has the display mode change.
  EXPECT_EQ(GetRegistrar(GetProfile(1)).GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kBrowser);

  // The user display settings is syned, so it should match.
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppUserDisplayMode(app_id),
            GetRegistrar(GetProfile(1)).GetAppUserDisplayMode(app_id));
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       PolicyAppPersistsUninstalledOnSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a non-syncing web app.
  webapps::AppId app_id = InstallAppAsUserInitiated(
      GetProfile(0), webapps::WebappInstallSource::EXTERNAL_POLICY);

  // Install the same app as a syncing app on profile 1.
  webapps::AppId app_id2 = InstallAppAsUserInitiated(GetProfile(1));
  EXPECT_EQ(app_id, app_id2);

  ASSERT_TRUE(AwaitWebAppQuiescence());

  // The app is in both profiles.
  EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  const WebApp* app = GetRegistrar(GetProfile(0)).GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_TRUE(app->IsPolicyInstalledApp());
  EXPECT_TRUE(app->IsSynced());

  // Uninstall the web app on the sync profile.
  test::UninstallWebApp(GetProfile(1), app_id);

  ASSERT_TRUE(AwaitWebAppQuiescence());

  // The policy app should remain on profile 0.
  EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              Not(ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1)))));
  app = GetRegistrar(GetProfile(0)).GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_TRUE(app->IsPolicyInstalledApp());
  EXPECT_FALSE(app->IsSynced());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, AppSortingSynced) {
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  ASSERT_TRUE(embedded_test_server()->Start());

  webapps::AppId app_id = InstallAppAsUserInitiated(GetProfile(0));

  syncer::StringOrdinal page_ordinal =
      GetAppSorting(GetProfile(0))->GetNaturalAppPageOrdinal();
  syncer::StringOrdinal launch_ordinal =
      GetAppSorting(GetProfile(0))->CreateNextAppLaunchOrdinal(page_ordinal);
  GetAppSorting(GetProfile(0))->SetPageOrdinal(app_id, page_ordinal);
  GetAppSorting(GetProfile(0))->SetAppLaunchOrdinal(app_id, launch_ordinal);

  ASSERT_TRUE(AwaitWebAppQuiescence());

  // The app is in both profiles.
  EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  EXPECT_EQ(page_ordinal, GetAppSorting(GetProfile(1))->GetPageOrdinal(app_id));
  EXPECT_EQ(launch_ordinal,
            GetAppSorting(GetProfile(1))->GetAppLaunchOrdinal(app_id));
}

// Test is flaky (crbug.com/1313368).
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       DISABLED_AppSortingFixCollisions) {
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install two different apps.
  webapps::AppId app_id1 = InstallAppAsUserInitiated(GetProfile(0));
  webapps::AppId app_id2 = InstallAppAsUserInitiated(
      GetProfile(0), webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      GetUserInitiatedAppURL2());
  ASSERT_NE(app_id1, app_id2);

  // Wait for both of the webapps to be installed on profile 1.
  ASSERT_TRUE(AwaitWebAppQuiescence());

  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));

  syncer::StringOrdinal page_ordinal =
      GetAppSorting(GetProfile(0))->CreateFirstAppPageOrdinal();
  syncer::StringOrdinal launch_ordinal =
      GetAppSorting(GetProfile(0))->CreateNextAppLaunchOrdinal(page_ordinal);

  GetAppSorting(GetProfile(0))->SetPageOrdinal(app_id1, page_ordinal);
  GetAppSorting(GetProfile(0))->SetAppLaunchOrdinal(app_id1, launch_ordinal);
  GetAppSorting(GetProfile(1))->SetPageOrdinal(app_id2, page_ordinal);
  GetAppSorting(GetProfile(1))->SetAppLaunchOrdinal(app_id2, launch_ordinal);

  ASSERT_TRUE(AwaitWebAppQuiescence());

  // Page & launch ordinals should be synced.
  EXPECT_EQ(GetAppSorting(GetProfile(0))->GetPageOrdinal(app_id1),
            GetAppSorting(GetProfile(1))->GetPageOrdinal(app_id1));
  EXPECT_EQ(GetAppSorting(GetProfile(0))->GetAppLaunchOrdinal(app_id1),
            GetAppSorting(GetProfile(1))->GetAppLaunchOrdinal(app_id1));
  EXPECT_EQ(GetAppSorting(GetProfile(0))->GetPageOrdinal(app_id2),
            GetAppSorting(GetProfile(1))->GetPageOrdinal(app_id2));
  EXPECT_EQ(GetAppSorting(GetProfile(0))->GetAppLaunchOrdinal(app_id2),
            GetAppSorting(GetProfile(1))->GetAppLaunchOrdinal(app_id2));

  // The page of app1 and app2 should be the same.
  EXPECT_EQ(GetAppSorting(GetProfile(0))->GetPageOrdinal(app_id1),
            GetAppSorting(GetProfile(0))->GetPageOrdinal(app_id2));
  // But the launch ordinal must be different.
  EXPECT_NE(GetAppSorting(GetProfile(0))->GetAppLaunchOrdinal(app_id1),
            GetAppSorting(GetProfile(0))->GetAppLaunchOrdinal(app_id2));
}

// Flaky on Linux TSan (crbug.com/1108172).
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
    defined(THREAD_SANITIZER)
#define MAYBE_UninstallSynced DISABLED_UninstallSynced
#else
#define MAYBE_UninstallSynced UninstallSynced
#endif
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, MAYBE_UninstallSynced) {
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  ASSERT_TRUE(embedded_test_server()->Start());

  webapps::AppId app_id;
  // Install & uninstall on profile 0, and validate profile 1 sees it.
  {
    WebAppTestInstallObserver app_listener(GetProfile(1));
    app_listener.BeginListening();
    InstallAppAsUserInitiated(GetProfile(0));
    app_id = app_listener.Wait();
    EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
                ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  }

  // Uninstall the webapp on profile 0, and validate profile 1 gets the change.
  {
    WebAppTestUninstallObserver app_listener(GetProfile(1));
    app_listener.BeginListening();
    test::UninstallWebApp(GetProfile(0), app_id);
    app_listener.Wait();
    EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
                ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  }

  // Next, install on profile 1, uninstall on profile 0, and validate that
  // profile 1 sees it.
  {
    WebAppTestInstallObserver app_listener(GetProfile(0));
    app_listener.BeginListening();
    InstallAppAsUserInitiated(GetProfile(1));
    app_id = app_listener.Wait();
    EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
                ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  }
  {
    WebAppTestUninstallObserver app_listener(GetProfile(1));
    app_listener.BeginListening();
    test::UninstallWebApp(GetProfile(0), app_id);
    app_listener.Wait();
  }

  EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, UninstallDoesNotReinstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  ASSERT_TRUE(embedded_test_server()->Start());

  webapps::AppId app_id;
  // Install & uninstall on profile 0, and validate profile 1 sees it.
  {
    WebAppTestInstallObserver app_listener(GetProfile(1));
    app_listener.BeginListening();
    InstallAppAsUserInitiated(GetProfile(0));
    app_id = app_listener.Wait();
    EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
                ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  }

  // Uninstall the webapp on profile 0.
  {
    WebAppTestUninstallObserver app_listener(GetProfile(0));
    app_listener.BeginListening();
    test::UninstallWebApp(GetProfile(0), app_id);
    app_listener.Wait();
  }
  // Propagate the change to profile 1.
  ASSERT_TRUE(AwaitQuiescence());
  // Propagate any possible re-installs back to profile 0.
  ASSERT_TRUE(AwaitQuiescence());

  WebAppProvider::GetForTest(GetProfile(0))
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();
  WebAppProvider::GetForTest(GetProfile(1))
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();

  // No apps pending install.
  EXPECT_TRUE(GetRegistrar(GetProfile(0))
                  .GetAppsFromSyncAndPendingInstallation()
                  .empty());
  EXPECT_TRUE(GetRegistrar(GetProfile(1))
                  .GetAppsFromSyncAndPendingInstallation()
                  .empty());
  // No apps.
  // https://crbug.com/1323003: Update this once AppSet & size is resolved.
  auto calculate_size = [](WebAppRegistrar::AppSet set) {
    int size = 0;
    for (auto it = set.begin(); it != set.end(); ++it) {
      ++size;
    }
    return size;
  };
  EXPECT_EQ(calculate_size(GetRegistrar(GetProfile(0)).GetAppsIncludingStubs()),
            0);
  EXPECT_EQ(calculate_size(GetRegistrar(GetProfile(1)).GetAppsIncludingStubs()),
            0);

  // No pending installs tasks.
  EXPECT_EQ(WebAppProvider::GetForTest(GetProfile(1))
                ->command_manager()
                .GetCommandCountForTesting(),
            0ul);
  EXPECT_EQ(WebAppProvider::GetForTest(GetProfile(0))
                ->command_manager()
                .GetCommandCountForTesting(),
            0ul);
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, NoShortcutsCreatedOnSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
              ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install & uninstall on profile 0, and validate profile 1 sees it.
  {
    base::RunLoop loop;
    base::RepeatingCallback<void(const webapps::AppId&)> on_installed_closure;
    base::RepeatingCallback<void(const webapps::AppId&)> on_hooks_closure;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    on_installed_closure = base::DoNothing();
    on_hooks_closure = base::BindLambdaForTesting(
        [&](const webapps::AppId& installed_app_id) { loop.Quit(); });
#else
    on_installed_closure = base::BindLambdaForTesting(
        [&](const webapps::AppId& installed_app_id) { loop.Quit(); });
    on_hooks_closure = base::BindLambdaForTesting(
        [](const webapps::AppId& installed_app_id) { FAIL(); });
#endif
    WebAppInstallManagerObserverAdapter app_listener(GetProfile(1));
    app_listener.SetWebAppInstalledDelegate(on_installed_closure);
    app_listener.SetWebAppInstalledWithOsHooksDelegate(on_hooks_closure);
    InstallAppAsUserInitiated(GetProfile(0));
    loop.Run();
    EXPECT_THAT(GetAllAppIdsForProfile(GetProfile(0)),
                ElementsAreArray(GetAllAppIdsForProfile(GetProfile(1))));
  }
}

}  // namespace
}  // namespace web_app
