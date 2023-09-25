// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
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
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/common/chrome_constants.h"
#include "components/sync/service/sync_service_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {

namespace {

class DisplayModeChangeWaiter : public WebAppRegistrarObserver {
 public:
  explicit DisplayModeChangeWaiter(WebAppRegistrar& registrar) {
    observation_.Observe(&registrar);
  }

  void OnWebAppUserDisplayModeChanged(
      const webapps::AppId& app_id,
      mojom::UserDisplayMode user_display_mode) override {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  void OnAppRegistrarDestroyed() override { NOTREACHED(); }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<WebAppRegistrar, WebAppRegistrarObserver>
      observation_{this};
};

}  // namespace

class TwoClientWebAppsSyncTest : public WebAppsSyncTestBase {
 public:
  TwoClientWebAppsSyncTest() : WebAppsSyncTestBase(TWO_CLIENT) {}

  TwoClientWebAppsSyncTest(const TwoClientWebAppsSyncTest&) = delete;
  TwoClientWebAppsSyncTest& operator=(const TwoClientWebAppsSyncTest&) = delete;

  ~TwoClientWebAppsSyncTest() override = default;

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    ASSERT_TRUE(SetupSync());
    ASSERT_TRUE(AllProfilesHaveSameWebAppIds());
  }

  const WebAppRegistrar& GetRegistrar(Profile* profile) {
    return WebAppProvider::GetForTest(profile)->registrar_unsafe();
  }

  bool AllProfilesHaveSameWebAppIds() {
    absl::optional<base::flat_set<webapps::AppId>> app_ids;
    for (Profile* profile : GetAllProfiles()) {
      base::flat_set<webapps::AppId> profile_app_ids(
          GetRegistrar(profile).GetAppIds());
      if (!app_ids) {
        app_ids = profile_app_ids;
      } else {
        if (app_ids != profile_app_ids) {
          return false;
        }
      }
    }
    return true;
  }

 private:
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
};

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, Basic) {
  WebAppTestInstallObserver install_observer(GetProfile(1));
  install_observer.BeginListening();

  WebAppInstallInfo info;
  info.title = u"Test name";
  info.description = u"Test description";
  info.start_url = GURL("http://www.chromium.org/path");
  info.scope = GURL("http://www.chromium.org/");
  webapps::AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);

  EXPECT_EQ(install_observer.Wait(), app_id);
  const WebAppRegistrar& registrar = GetRegistrar(GetProfile(1));
  EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
  EXPECT_EQ(registrar.GetAppStartUrl(app_id), info.start_url);
  EXPECT_EQ(registrar.GetAppScope(app_id), info.scope);

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, Minimal) {
  WebAppTestInstallObserver install_observer(GetProfile(1));
  install_observer.BeginListening();

  WebAppInstallInfo info;
  info.title = u"Test name";
  info.start_url = GURL("http://www.chromium.org/");
  webapps::AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);

  EXPECT_EQ(install_observer.Wait(), app_id);
  const WebAppRegistrar& registrar = GetRegistrar(GetProfile(1));
  EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
  EXPECT_EQ(registrar.GetAppStartUrl(app_id), info.start_url);

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, ThemeColor) {
  WebAppTestInstallObserver install_observer(GetProfile(1));
  install_observer.BeginListening();

  WebAppInstallInfo info;
  info.title = u"Test name";
  info.start_url = GURL("http://www.chromium.org/");
  info.theme_color = SK_ColorBLUE;
  webapps::AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppThemeColor(app_id),
            info.theme_color);

  EXPECT_EQ(install_observer.Wait(), app_id);
  const WebAppRegistrar& registrar = GetRegistrar(GetProfile(1));
  EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
  EXPECT_EQ(registrar.GetAppStartUrl(app_id), info.start_url);
  EXPECT_EQ(registrar.GetAppThemeColor(app_id), info.theme_color);

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, IsLocallyInstalled) {
  WebAppTestInstallObserver install_observer(GetProfile(1));
  install_observer.BeginListening();

  WebAppInstallInfo info;
  info.title = u"Test name";
  info.start_url = GURL("http://www.chromium.org/");
  webapps::AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_TRUE(GetRegistrar(GetProfile(0)).IsLocallyInstalled(app_id));

  EXPECT_EQ(install_observer.Wait(), app_id);
  bool is_locally_installed =
      GetRegistrar(GetProfile(1)).IsLocallyInstalled(app_id);
  EXPECT_EQ(is_locally_installed, AreAppsLocallyInstalledBySync());

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, AppFieldsChangeDoesNotSync) {
  const WebAppRegistrar& registrar0 = GetRegistrar(GetProfile(0));
  const WebAppRegistrar& registrar1 = GetRegistrar(GetProfile(1));

  WebAppInstallInfo info_a;
  info_a.title = u"Test name A";
  info_a.description = u"Description A";
  info_a.start_url = GURL("http://www.chromium.org/path/to/start_url");
  info_a.scope = GURL("http://www.chromium.org/path/to/");
  info_a.theme_color = SK_ColorBLUE;
  webapps::AppId app_id_a = apps_helper::InstallWebApp(GetProfile(0), info_a);

  EXPECT_EQ(WebAppTestInstallObserver(GetProfile(1)).BeginListeningAndWait(),
            app_id_a);

  EXPECT_EQ(base::UTF8ToUTF16(registrar1.GetAppShortName(app_id_a)),
            info_a.title);
  EXPECT_EQ(registrar1.GetAppScope(app_id_a), info_a.scope);

  EXPECT_EQ(registrar1.GetAppThemeColor(app_id_a), info_a.theme_color);
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());

  // Reinstall same app in Profile 0 with a different metadata aside from the
  // name and start_url.
  WebAppInstallInfo info_b;
  info_b.title = u"Test name B";
  info_b.description = u"Description B";
  info_b.start_url = GURL("http://www.chromium.org/path/to/start_url");
  info_b.scope = GURL("http://www.chromium.org/path/to/");
  info_b.theme_color = SK_ColorRED;
  webapps::AppId app_id_b = apps_helper::InstallWebApp(GetProfile(0), info_b);
  EXPECT_EQ(app_id_a, app_id_b);
  EXPECT_EQ(base::UTF8ToUTF16(registrar0.GetAppShortName(app_id_a)),
            info_b.title);
  EXPECT_EQ(base::UTF8ToUTF16(registrar0.GetAppDescription(app_id_a)),
            info_b.description);
  EXPECT_EQ(registrar0.GetAppScope(app_id_a), info_b.scope);
  EXPECT_EQ(registrar0.GetAppThemeColor(app_id_a), info_b.theme_color);

  // Install a separate app just to have something to await on to ensure the
  // sync has propagated to the other profile.
  WebAppInstallInfo infoC;
  infoC.title = u"Different test name";
  infoC.start_url = GURL("http://www.notchromium.org/");
  webapps::AppId app_id_c = apps_helper::InstallWebApp(GetProfile(0), infoC);
  EXPECT_NE(app_id_a, app_id_c);
  EXPECT_EQ(WebAppTestInstallObserver(GetProfile(1)).BeginListeningAndWait(),
            app_id_c);

  // After sync we should not see the metadata update in Profile 1.
  EXPECT_EQ(base::UTF8ToUTF16(registrar1.GetAppShortName(app_id_a)),
            info_a.title);
  EXPECT_EQ(registrar1.GetAppScope(app_id_a), info_a.scope);

  EXPECT_EQ(registrar1.GetAppThemeColor(app_id_a), info_a.theme_color);

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

// Tests that we don't crash when syncing an icon info with no size.
// Context: https://crbug.com/1058283
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, SyncFaviconOnly) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile* sourceProfile = GetProfile(0);
  Profile* destProfile = GetProfile(1);

  WebAppTestInstallObserver destInstallObserver(destProfile);
  destInstallObserver.BeginListening();

  // Install favicon only page as web app.
  webapps::AppId app_id;
  {
    Browser* browser = CreateBrowser(sourceProfile);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser,
        embedded_test_server()->GetURL("/web_apps/favicon_only.html")));
    chrome::SetAutoAcceptWebAppDialogForTesting(true, true);
    WebAppTestInstallObserver installObserver(sourceProfile);
    installObserver.BeginListening();
    chrome::ExecuteCommand(browser, IDC_CREATE_SHORTCUT);
    app_id = installObserver.Wait();
    chrome::SetAutoAcceptWebAppDialogForTesting(false, false);
    chrome::CloseWindow(browser);
  }
  EXPECT_EQ(GetRegistrar(sourceProfile).GetAppShortName(app_id),
            "Favicon only");
  std::vector<apps::IconInfo> manifest_icons =
      GetRegistrar(sourceProfile).GetAppIconInfos(app_id);
  ASSERT_EQ(manifest_icons.size(), 1u);
  EXPECT_FALSE(manifest_icons[0].square_size_px.has_value());

  // Wait for app to sync across.
  webapps::AppId synced_app_id = destInstallObserver.Wait();
  EXPECT_EQ(synced_app_id, app_id);
  EXPECT_EQ(GetRegistrar(destProfile).GetAppShortName(app_id), "Favicon only");
  manifest_icons = GetRegistrar(destProfile).GetAppIconInfos(app_id);
  ASSERT_EQ(manifest_icons.size(), 1u);
  EXPECT_FALSE(manifest_icons[0].square_size_px.has_value());
}

// Tests that we don't use the manifest start_url if it differs from what came
// through sync.
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, SyncUsingStartUrlFallback) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile* source_profile = GetProfile(0);
  Profile* dest_profile = GetProfile(1);

  WebAppTestInstallObserver dest_install_observer(dest_profile);
  dest_install_observer.BeginListening();

  // Install app with name.
  WebAppInstallInfo info;
  info.title = u"Test app";
  info.start_url =
      embedded_test_server()->GetURL("/web_apps/different_start_url.html");
  webapps::AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_EQ(GetRegistrar(source_profile).GetAppShortName(app_id), "Test app");
  EXPECT_EQ(GetRegistrar(source_profile).GetAppStartUrl(app_id),
            info.start_url);

  // Wait for app to sync across.
  webapps::AppId synced_app_id = dest_install_observer.Wait();
  ASSERT_EQ(synced_app_id, app_id);
  EXPECT_EQ(GetRegistrar(dest_profile).GetAppShortName(app_id), "Test app");
  EXPECT_EQ(GetRegistrar(dest_profile).GetAppStartUrl(app_id), info.start_url);
}

// Tests that we don't use the page title if there's no manifest.
// Pages without a manifest are usually not the correct page to draw information
// from e.g. login redirects or loading pages.
// Context: https://crbug.com/1078286
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, SyncUsingNameFallback) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile* source_profile = GetProfile(0);
  Profile* dest_profile = GetProfile(1);

  WebAppTestInstallObserver dest_install_observer(dest_profile);
  dest_install_observer.BeginListening();

  // Install app with name.
  WebAppInstallInfo info;
  info.title = u"Correct App Name";
  info.start_url =
      embedded_test_server()->GetURL("/web_apps/bad_title_only.html");
  webapps::AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_EQ(GetRegistrar(source_profile).GetAppShortName(app_id),
            "Correct App Name");

  // Wait for app to sync across.
  webapps::AppId synced_app_id = dest_install_observer.Wait();
  EXPECT_EQ(synced_app_id, app_id);
  EXPECT_EQ(GetRegistrar(dest_profile).GetAppShortName(app_id),
            "Correct App Name");
}

// Negative test of SyncUsingNameFallback above. Don't use the app name fallback
// if there's a name provided by the manifest during sync.
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, SyncWithoutUsingNameFallback) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile* source_profile = GetProfile(0);
  Profile* dest_profile = GetProfile(1);

  WebAppTestInstallObserver dest_install_observer(dest_profile);
  dest_install_observer.BeginListening();

  // Install app with name.
  WebAppInstallInfo info;
  info.title = u"Incorrect App Name";
  info.start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  webapps::AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_EQ(GetRegistrar(source_profile).GetAppShortName(app_id),
            "Incorrect App Name");

  // Wait for app to sync across.
  webapps::AppId synced_app_id = dest_install_observer.Wait();
  EXPECT_EQ(synced_app_id, app_id);
  EXPECT_EQ(GetRegistrar(dest_profile).GetAppShortName(app_id),
            "Basic web app");
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, SyncUsingIconUrlFallback) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile* source_profile = GetProfile(0);
  Profile* dest_profile = GetProfile(1);

  WebAppTestInstallObserver dest_install_observer(dest_profile);
  dest_install_observer.BeginListening();

  // Install app with name.
  WebAppInstallInfo info;
  info.title = u"Blue icon";
  info.start_url = GURL("https://does-not-exist.org");
  info.theme_color = SK_ColorBLUE;
  info.scope = GURL("https://does-not-exist.org/scope");
  apps::IconInfo icon_info;
  icon_info.square_size_px = 192;
  icon_info.url = embedded_test_server()->GetURL("/web_apps/blue-192.png");
  icon_info.purpose = apps::IconInfo::Purpose::kAny;
  info.manifest_icons.push_back(icon_info);
  webapps::AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_EQ(GetRegistrar(source_profile).GetAppShortName(app_id), "Blue icon");

  // Wait for app to sync across.
  webapps::AppId synced_app_id = dest_install_observer.Wait();
  EXPECT_EQ(synced_app_id, app_id);
  EXPECT_EQ(GetRegistrar(dest_profile).GetAppShortName(app_id), "Blue icon");

  // Make sure icon downloaded despite not loading start_url.
  {
    base::RunLoop run_loop;
    WebAppProvider::GetForTest(dest_profile)
        ->icon_manager()
        .ReadSmallestIcon(
            synced_app_id, {IconPurpose::ANY}, 192,
            base::BindLambdaForTesting(
                [&run_loop](IconPurpose purpose, SkBitmap bitmap) {
                  EXPECT_EQ(purpose, IconPurpose::ANY);
                  EXPECT_EQ(bitmap.getColor(0, 0), SK_ColorBLUE);
                  run_loop.Quit();
                }));
    run_loop.Run();
  }

  EXPECT_EQ(GetRegistrar(dest_profile).GetAppScope(synced_app_id),
            GURL("https://does-not-exist.org/scope"));
  EXPECT_EQ(GetRegistrar(dest_profile).GetAppThemeColor(synced_app_id),
            SK_ColorBLUE);
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, SyncUserDisplayModeChange) {
  WebAppTestInstallObserver install_observer(GetProfile(1));
  install_observer.BeginListening();

  WebAppInstallInfo info;
  info.title = u"Test name";
  info.description = u"Test description";
  info.start_url = GURL("http://www.chromium.org/path");
  info.scope = GURL("http://www.chromium.org/");
  info.user_display_mode = mojom::UserDisplayMode::kStandalone;
  webapps::AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);

  EXPECT_EQ(install_observer.Wait(), app_id);
  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());

  auto* provider0 = WebAppProvider::GetForTest(GetProfile(0));
  auto* provider1 = WebAppProvider::GetForTest(GetProfile(1));
  WebAppRegistrar& registrar1 = provider1->registrar_unsafe();
  EXPECT_EQ(registrar1.GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);

  DisplayModeChangeWaiter display_mode_change_waiter(registrar1);
  provider0->sync_bridge_unsafe().SetAppUserDisplayMode(
      app_id, mojom::UserDisplayMode::kTabbed,
      /*is_user_action=*/true);
  display_mode_change_waiter.Wait();

  EXPECT_EQ(registrar1.GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kTabbed);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class TwoClientLacrosWebAppsSyncTest : public SyncTest {
 public:
  TwoClientLacrosWebAppsSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientLacrosWebAppsSyncTest() override = default;

  // SyncTest:
  base::FilePath GetProfileBaseName(int index) override {
    if (index == 0)
      return base::FilePath(chrome::kInitialProfile);
    return SyncTest::GetProfileBaseName(index);
  }
};

IN_PROC_BROWSER_TEST_F(TwoClientLacrosWebAppsSyncTest,
                       SyncDisabledUnlessPrimary) {
  ASSERT_TRUE(SetupSync());

  {
    EXPECT_TRUE(GetProfile(0)->IsMainProfile());
    syncer::SyncServiceImpl* service = GetSyncService(0);
    syncer::ModelTypeSet types = service->GetActiveDataTypes();
    EXPECT_TRUE(types.Has(syncer::APPS));
    EXPECT_TRUE(types.Has(syncer::APP_SETTINGS));
    EXPECT_TRUE(types.Has(syncer::WEB_APPS));
  }

  {
    EXPECT_FALSE(GetProfile(1)->IsMainProfile());
    syncer::SyncServiceImpl* service = GetSyncService(1);
    syncer::ModelTypeSet types = service->GetActiveDataTypes();
    EXPECT_FALSE(types.Has(syncer::APPS));
    EXPECT_FALSE(types.Has(syncer::APP_SETTINGS));
    EXPECT_FALSE(types.Has(syncer::WEB_APPS));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace web_app
