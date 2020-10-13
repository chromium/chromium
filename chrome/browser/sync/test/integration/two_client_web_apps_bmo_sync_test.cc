// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_system.h"

namespace web_app {
namespace {

class TwoClientWebAppsBMOSyncTest : public SyncTest {
 public:
  TwoClientWebAppsBMOSyncTest() : SyncTest(TWO_CLIENT) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDesktopPWAsWithoutExtensions);

    DisableVerifier();
  }

  ~TwoClientWebAppsBMOSyncTest() override = default;

  bool SetupClients() override {
    bool result = SyncTest::SetupClients();
    if (!result)
      return result;
    // All of the tests need to have os integration suppressed & the
    // WebAppProvider ready.
    for (Profile* profile : GetAllProfiles()) {
      auto* web_app_provider = WebAppProvider::Get(profile);
      web_app_provider->os_integration_manager().SuppressOsHooksForTesting();
      web_app_provider->install_finalizer()
          .RemoveLegacyInstallFinalizerForTesting();
      base::RunLoop loop;
      web_app_provider->on_registry_ready().Post(FROM_HERE, loop.QuitClosure());
      loop.Run();
    }
    return true;
  }

  // Installs a dummy app with the given |url| on |profile1| and waits for it to
  // sync to |profile2|. This ensures that the sync system has fully flushed any
  // pending changes from |profile1| to |profile2|.
  AppId InstallDummyAppAndWaitForSync(const GURL& url,
                                      Profile* profile1,
                                      Profile* profile2) {
    WebApplicationInfo info = WebApplicationInfo();
    info.title = base::UTF8ToUTF16(url.spec());
    info.start_url = url;
    AppId dummy_app_id = InstallApp(info, profile1);
    EXPECT_EQ(
        WebAppInstallObserver::CreateInstallListener(profile2, {dummy_app_id})
            ->AwaitNextInstall(),
        dummy_app_id);
    return dummy_app_id;
  }

  GURL GetUserInitiatedAppURL() const {
    return embedded_test_server()->GetURL("/web_apps/basic.html");
  }

  GURL GetUserInitiatedAppURL2() const {
    return embedded_test_server()->GetURL("/web_apps/no_service_worker.html");
  }

  AppId InstallAppAsUserInitiated(
      Profile* profile,
      WebappInstallSource source = WebappInstallSource::OMNIBOX_INSTALL_ICON,
      GURL start_url = GURL()) {
    Browser* browser = CreateBrowser(profile);
    if (!start_url.is_valid())
      start_url = GetUserInitiatedAppURL();
    ui_test_utils::NavigateToURL(browser, start_url);

    AppId app_id;
    base::RunLoop run_loop;
    WebAppProvider::Get(profile)
        ->install_manager()
        .InstallWebAppFromManifestWithFallback(
            browser->tab_strip_model()->GetActiveWebContents(),
            /*force_shortcut_app=*/false, source,
            base::BindOnce(TestAcceptDialogCallback),
            base::BindLambdaForTesting(
                [&](const AppId& new_app_id, InstallResultCode code) {
                  EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
                  app_id = new_app_id;
                  run_loop.Quit();
                }));
    run_loop.Run();
    return app_id;
  }

  AppId InstallApp(const WebApplicationInfo& info, Profile* profile) {
    return InstallApp(info, profile, WebappInstallSource::OMNIBOX_INSTALL_ICON);
  }

  AppId InstallApp(const WebApplicationInfo& info,
                   Profile* profile,
                   WebappInstallSource source) {
    DCHECK(info.start_url.is_valid());

    base::RunLoop run_loop;
    AppId app_id;

    WebAppProvider::Get(profile)->install_manager().InstallWebAppFromInfo(
        std::make_unique<WebApplicationInfo>(info), ForInstallableSite::kYes,
        source,
        base::BindLambdaForTesting(
            [&run_loop, &app_id](const AppId& new_app_id,
                                 InstallResultCode code) {
              DCHECK_EQ(code, InstallResultCode::kSuccessNewInstall);
              app_id = new_app_id;
              run_loop.Quit();
            }));
    run_loop.Run();

    const AppRegistrar& registrar = GetRegistrar(profile);
    EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
    EXPECT_EQ(registrar.GetAppStartUrl(app_id), info.start_url);

    return app_id;
  }

  const WebAppRegistrar& GetRegistrar(Profile* profile) {
    auto* web_app_registrar =
        WebAppProvider::Get(profile)->registrar().AsWebAppRegistrar();
    EXPECT_TRUE(web_app_registrar);
    return *web_app_registrar;
  }

  extensions::AppSorting* GetAppSorting(Profile* profile) {
    return extensions::ExtensionSystem::Get(profile)->app_sorting();
  }

  bool AllProfilesHaveSameWebAppIds() {
    base::Optional<base::flat_set<AppId>> app_ids;
    for (Profile* profile : GetAllProfiles()) {
      base::flat_set<AppId> profile_app_ids(GetRegistrar(profile).GetAppIds());
      if (!app_ids) {
        app_ids = profile_app_ids;
      } else {
        if (app_ids != profile_app_ids)
          return false;
      }
    }
    return true;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(TwoClientWebAppsBMOSyncTest);
};

// Test is flaky (crbug.com/1097050)
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       DISABLED_SyncDoubleInstallation) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());

  // Install web app to both profiles.
  AppId app_id = InstallAppAsUserInitiated(GetProfile(0));
  AppId app_id2 = InstallAppAsUserInitiated(GetProfile(1));

  EXPECT_EQ(app_id, app_id2);

  // Install a 'dummy' app & wait for installation to ensure sync has processed
  // the initial apps.
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy.org/"), GetProfile(0),
                                GetProfile(1));

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       SyncDoubleInstallationDifferentNames) {
  ASSERT_TRUE(SetupClients());
  WebApplicationInfo info;
  info.title = base::UTF8ToUTF16("Test name");
  info.start_url = GURL("http://www.chromium.org/path");

  // Install web app to both profiles.
  AppId app_id = InstallApp(info, GetProfile(0));
  // The web app has a different title on the second profile.
  info.title = base::UTF8ToUTF16("Test name 2");
  AppId app_id2 = InstallApp(info, GetProfile(1));

  EXPECT_EQ(app_id, app_id2);

  ASSERT_TRUE(SetupSync());

  // Install a 'dummy' app & wait for installation to ensure sync has processed
  // the initial apps.
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy1.org/"), GetProfile(0),
                                GetProfile(1));
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy2.org/"), GetProfile(1),
                                GetProfile(0));

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
  // The titles should respect the installation, even though the sync system
  // would only have one name.
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppShortName(app_id), "Test name");
  EXPECT_EQ(GetRegistrar(GetProfile(1)).GetAppShortName(app_id), "Test name 2");
}

// Flaky, see crbug.com/1126404.
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_SyncDoubleInstallationDifferentUserDisplayMode \
  DISABLED_SyncDoubleInstallationDifferentUserDisplayMode
#else
#define MAYBE_SyncDoubleInstallationDifferentUserDisplayMode \
  SyncDoubleInstallationDifferentUserDisplayMode
#endif
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       MAYBE_SyncDoubleInstallationDifferentUserDisplayMode) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());

  WebApplicationInfo info;
  info.title = base::UTF8ToUTF16("Test name");
  info.start_url = GURL("http://www.chromium.org/path");
  info.open_as_window = true;

  // Install web app to both profiles.
  AppId app_id = InstallApp(info, GetProfile(0));
  // The web app has a different open on the second profile.
  info.open_as_window = false;
  AppId app_id2 = InstallApp(info, GetProfile(1));

  EXPECT_EQ(app_id, app_id2);

  // Install a 'dummy' app & wait for installation to ensure sync has processed
  // the initial apps.
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy.org/"), GetProfile(0),
                                GetProfile(1));

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());

  // The user display setting is syned, so these should match. However, the
  // actual value here is racy.
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppUserDisplayMode(app_id),
            GetRegistrar(GetProfile(1)).GetAppUserDisplayMode(app_id));
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, DisplayMode) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install web app to profile 0 and wait for it to sync to profile 1.
  AppId app_id = InstallAppAsUserInitiated(GetProfile(0));
  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id);

  WebAppProvider::Get(GetProfile(1))
      ->registry_controller()
      .SetAppUserDisplayMode(app_id, web_app::DisplayMode::kBrowser,
                             /*is_user_action=*/false);

  // Install a 'dummy' app & wait for installation to ensure sync has processed
  // the initial apps.
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy.org/"), GetProfile(1),
                                GetProfile(0));

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());

  // The change should have synced to profile 0.
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppUserDisplayMode(app_id),
            web_app::DisplayMode::kBrowser);
  // The user display settings is synced, so it should match.
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppUserDisplayMode(app_id),
            GetRegistrar(GetProfile(1)).GetAppUserDisplayMode(app_id));
}

// Although the logic is allowed to be racy, the profiles should still end up
// with the same web app ids.
#if defined(OS_WIN)
// Flaky on windows, https://crbug.com/1111533
#define MAYBE_DoubleInstallWithUninstall DISABLED_DoubleInstallWithUninstall
#else
#define MAYBE_DoubleInstallWithUninstall DoubleInstallWithUninstall
#endif
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       MAYBE_DoubleInstallWithUninstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install web app to both profiles.
  AppId app_id = InstallAppAsUserInitiated(GetProfile(0));
  AppId app_id2 = InstallAppAsUserInitiated(GetProfile(1));
  EXPECT_EQ(app_id, app_id2);

  // Uninstall the app from one of the profiles.
  UninstallWebApp(GetProfile(0), app_id);

  // Install a 'dummy' app & wait for installation to ensure sync has processed
  // the initial apps.
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy.org/"), GetProfile(0),
                                GetProfile(1));

  // The apps should either be installed on both or uninstalled on both. This
  // fails, hence disabled test.
  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, NotSynced) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a non-syncing web app.
  AppId app_id = InstallAppAsUserInitiated(
      GetProfile(0), WebappInstallSource::EXTERNAL_DEFAULT);

  // Install a 'dummy' app & wait for installation to ensure sync has processed
  // the initial apps.
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy.org/"), GetProfile(0),
                                GetProfile(1));

  // Profile 0 should have an extra unsynced app, and it should not be in
  // profile 1.
  EXPECT_FALSE(AllProfilesHaveSameWebAppIds());
  EXPECT_FALSE(GetRegistrar(GetProfile(1)).IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, NotSyncedThenSynced) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a non-syncing web app.
  AppId app_id = InstallAppAsUserInitiated(
      GetProfile(0), WebappInstallSource::EXTERNAL_DEFAULT);

  // Install the same app as a syncing app on profile 1.
  AppId app_id2 = InstallAppAsUserInitiated(GetProfile(1));
  EXPECT_EQ(app_id, app_id2);

  // Install a 'dummy' app & wait for installation to ensure sync has processed
  // the initial apps.
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy.org/"), GetProfile(0),
                                GetProfile(1));

  // The app is in both profiles.
  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());

  // The app should have synced from profile 0 to profile 1, which enables sync
  // on profile 0. So changes should propagate from profile 0 to profile 1 now.
  WebAppProvider::Get(GetProfile(0))
      ->registry_controller()
      .SetAppUserDisplayMode(app_id, web_app::DisplayMode::kBrowser,
                             /*is_user_action=*/false);

  // Install a 'dummy' app & wait for installation to ensure sync has processed
  // the initial apps.
  InstallDummyAppAndWaitForSync(GURL("http://www.seconddummy.org/"),
                                GetProfile(0), GetProfile(1));

  // Check that profile 1 has the display mode change.
  EXPECT_EQ(GetRegistrar(GetProfile(1)).GetAppUserDisplayMode(app_id),
            web_app::DisplayMode::kBrowser);

  // The user display settings is syned, so it should match.
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppUserDisplayMode(app_id),
            GetRegistrar(GetProfile(1)).GetAppUserDisplayMode(app_id));
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       PolicyAppPersistsUninstalledOnSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a non-syncing web app.
  AppId app_id = InstallAppAsUserInitiated(
      GetProfile(0), WebappInstallSource::EXTERNAL_POLICY);

  // Install the same app as a syncing app on profile 1.
  AppId app_id2 = InstallAppAsUserInitiated(GetProfile(1));
  EXPECT_EQ(app_id, app_id2);

  // Install a 'dummy' app & wait for installation to ensure sync has processed
  // the initial apps.
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy.org/"), GetProfile(1),
                                GetProfile(0));

  // The app is in both profiles.
  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
  const WebApp* app = GetRegistrar(GetProfile(0)).GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_TRUE(app->IsPolicyInstalledApp());
  EXPECT_TRUE(app->IsSynced());

  // Uninstall the web app on the sync profile.
  UninstallWebApp(GetProfile(1), app_id);

  // Install a 'dummy' app & wait for installation to ensure sync has processed
  // the initial apps.
  InstallDummyAppAndWaitForSync(GURL("http://www.seconddummy.org/"),
                                GetProfile(1), GetProfile(0));

  // The policy app should remain on profile 0.
  EXPECT_FALSE(AllProfilesHaveSameWebAppIds());
  app = GetRegistrar(GetProfile(0)).GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_TRUE(app->IsPolicyInstalledApp());
  EXPECT_FALSE(app->IsSynced());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, AppSortingSynced) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());
  ASSERT_TRUE(embedded_test_server()->Start());

  AppId app_id = InstallAppAsUserInitiated(GetProfile(0));

  syncer::StringOrdinal page_ordinal =
      GetAppSorting(GetProfile(0))->GetNaturalAppPageOrdinal();
  syncer::StringOrdinal launch_ordinal =
      GetAppSorting(GetProfile(0))->CreateNextAppLaunchOrdinal(page_ordinal);
  GetAppSorting(GetProfile(0))->SetPageOrdinal(app_id, page_ordinal);
  GetAppSorting(GetProfile(0))->SetAppLaunchOrdinal(app_id, launch_ordinal);

  // Install a 'dummy' app & wait for installation to ensure sync has processed
  // the initial apps.
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy.org/"), GetProfile(0),
                                GetProfile(1));

  // The app is in both profiles.
  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
  EXPECT_EQ(page_ordinal, GetAppSorting(GetProfile(1))->GetPageOrdinal(app_id));
  EXPECT_EQ(launch_ordinal,
            GetAppSorting(GetProfile(1))->GetAppLaunchOrdinal(app_id));
}

// Flakily fails on Windows only (crbug.com/1099816)
#if defined(OS_WIN)
#define MAYBE_AppSortingFixCollisions DISABLED_AppSortingFixCollisions
#else
#define MAYBE_AppSortingFixCollisions AppSortingFixCollisions
#endif
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest,
                       MAYBE_AppSortingFixCollisions) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install two different apps.
  AppId app_id1 = InstallAppAsUserInitiated(GetProfile(0));
  AppId app_id2 = InstallAppAsUserInitiated(
      GetProfile(0), WebappInstallSource::OMNIBOX_INSTALL_ICON,
      GetUserInitiatedAppURL2());

  ASSERT_NE(app_id1, app_id2);

  // Wait for both of the webapps to be installed on profile 1.
  WebAppInstallObserver::CreateInstallListener(GetProfile(1),
                                               {app_id1, app_id2})
      ->AwaitNextInstall();
  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());

  syncer::StringOrdinal page_ordinal =
      GetAppSorting(GetProfile(0))->CreateFirstAppPageOrdinal();
  syncer::StringOrdinal launch_ordinal =
      GetAppSorting(GetProfile(0))->CreateNextAppLaunchOrdinal(page_ordinal);

  GetAppSorting(GetProfile(0))->SetPageOrdinal(app_id1, page_ordinal);
  GetAppSorting(GetProfile(0))->SetAppLaunchOrdinal(app_id1, launch_ordinal);
  GetAppSorting(GetProfile(1))->SetPageOrdinal(app_id2, page_ordinal);
  GetAppSorting(GetProfile(1))->SetAppLaunchOrdinal(app_id2, launch_ordinal);

  // Install 'dummy' apps & wait for installation to ensure sync has processed
  // the ordinals both ways.
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy1.org/"), GetProfile(0),
                                GetProfile(1));
  InstallDummyAppAndWaitForSync(GURL("http://www.dummy2.org/"), GetProfile(1),
                                GetProfile(0));

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

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsBMOSyncTest, UninstallSynced) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());
  ASSERT_TRUE(embedded_test_server()->Start());

  AppId app_id;
  // Install & uninstall on profile 0, and validate profile 1 sees it.
  {
    base::RunLoop loop;
    WebAppInstallObserver app_listener(GetProfile(1));
    app_listener.SetWebAppInstalledDelegate(
        base::BindLambdaForTesting([&](const AppId& installed_app_id) {
          app_id = installed_app_id;
          loop.Quit();
        }));
    app_id = InstallAppAsUserInitiated(GetProfile(0));
    loop.Run();
    EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
  }

  // Uninstall the webapp on profile 0, and validate profile 1 gets the change.
  {
    base::RunLoop loop;
    WebAppInstallObserver app_listener(GetProfile(1));
    app_listener.SetWebAppUninstalledDelegate(
        base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
          app_id = uninstalled_app_id;
          loop.Quit();
        }));
    UninstallWebApp(GetProfile(0), app_id);
    loop.Run();
    EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
  }

  // Next, install on profile 1, uninstall on profile 0, and validate that
  // profile 1 sees it.
  {
    base::RunLoop loop;
    WebAppInstallObserver app_listener(GetProfile(0));
    app_listener.SetWebAppInstalledDelegate(
        base::BindLambdaForTesting([&](const AppId& installed_app_id) {
          app_id = installed_app_id;
          loop.Quit();
        }));
    app_id = InstallAppAsUserInitiated(GetProfile(1));
    loop.Run();
    EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
  }
  {
    base::RunLoop loop;
    WebAppInstallObserver app_listener(GetProfile(1));
    app_listener.SetWebAppUninstalledDelegate(
        base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
          app_id = uninstalled_app_id;
          loop.Quit();
        }));
    UninstallWebApp(GetProfile(0), app_id);
    loop.Run();
  }

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

}  // namespace
}  // namespace web_app
