// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace web_app {

class TwoClientWebAppsSyncTest : public SyncTest {
 public:
  TwoClientWebAppsSyncTest() : SyncTest(TWO_CLIENT) {}

  ~TwoClientWebAppsSyncTest() override = default;

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    ASSERT_TRUE(SetupSync());
    ASSERT_TRUE(AllProfilesHaveSameWebAppIds());

    os_hooks_suppress_ =
        OsIntegrationManager::ScopedSuppressOsHooksForTesting();
  }

  const AppRegistrar& GetRegistrar(Profile* profile) {
    return WebAppProvider::Get(profile)->registrar();
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
  ScopedOsHooksSuppress os_hooks_suppress_;

  DISALLOW_COPY_AND_ASSIGN(TwoClientWebAppsSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, Basic) {
  WebApplicationInfo info;
  info.title = u"Test name";
  info.description = u"Test description";
  info.start_url = GURL("http://www.chromium.org/path");
  info.scope = GURL("http://www.chromium.org/");
  AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);

  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id);
  const AppRegistrar& registrar = GetRegistrar(GetProfile(1));
  EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
  EXPECT_EQ(registrar.GetAppStartUrl(app_id), info.start_url);
  EXPECT_EQ(registrar.GetAppScope(app_id), info.scope);

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, Minimal) {
  WebApplicationInfo info;
  info.title = u"Test name";
  info.start_url = GURL("http://www.chromium.org/");
  AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);

  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id);
  const AppRegistrar& registrar = GetRegistrar(GetProfile(1));
  EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
  EXPECT_EQ(registrar.GetAppStartUrl(app_id), info.start_url);

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, ThemeColor) {
  WebApplicationInfo info;
  info.title = u"Test name";
  info.start_url = GURL("http://www.chromium.org/");
  info.theme_color = SK_ColorBLUE;
  AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppThemeColor(app_id),
            info.theme_color);

  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id);
  const AppRegistrar& registrar = GetRegistrar(GetProfile(1));
  EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
  EXPECT_EQ(registrar.GetAppStartUrl(app_id), info.start_url);
  EXPECT_EQ(registrar.GetAppThemeColor(app_id), info.theme_color);

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, IsLocallyInstalled) {
  WebApplicationInfo info;
  info.title = u"Test name";
  info.start_url = GURL("http://www.chromium.org/");
  AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_TRUE(GetRegistrar(GetProfile(0)).IsLocallyInstalled(app_id));

  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id);
  bool is_locally_installed =
      GetRegistrar(GetProfile(1)).IsLocallyInstalled(app_id);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(is_locally_installed);
#else
  EXPECT_FALSE(is_locally_installed);
#endif

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, AppFieldsChangeDoesNotSync) {
  const AppRegistrar& registrar0 = GetRegistrar(GetProfile(0));
  const AppRegistrar& registrar1 = GetRegistrar(GetProfile(1));

  WebApplicationInfo info_a;
  info_a.title = u"Test name A";
  info_a.description = u"Description A";
  info_a.start_url = GURL("http://www.chromium.org/path/to/start_url");
  info_a.scope = GURL("http://www.chromium.org/path/to/");
  info_a.theme_color = SK_ColorBLUE;
  AppId app_id_a = apps_helper::InstallWebApp(GetProfile(0), info_a);

  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id_a);

  EXPECT_EQ(base::UTF8ToUTF16(registrar1.GetAppShortName(app_id_a)),
            info_a.title);
  EXPECT_EQ(registrar1.GetAppScope(app_id_a), info_a.scope);

  EXPECT_EQ(registrar1.GetAppThemeColor(app_id_a), info_a.theme_color);
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());

  // Reinstall same app in Profile 0 with a different metadata aside from the
  // name and start_url.
  WebApplicationInfo info_b;
  info_b.title = u"Test name B";
  info_b.description = u"Description B";
  info_b.start_url = GURL("http://www.chromium.org/path/to/start_url");
  info_b.scope = GURL("http://www.chromium.org/path/to/");
  info_b.theme_color = SK_ColorRED;
  AppId app_id_b = apps_helper::InstallWebApp(GetProfile(0), info_b);
  EXPECT_EQ(app_id_a, app_id_b);
  EXPECT_EQ(base::UTF8ToUTF16(registrar0.GetAppShortName(app_id_a)),
            info_b.title);
  EXPECT_EQ(base::UTF8ToUTF16(registrar0.GetAppDescription(app_id_a)),
            info_b.description);
  EXPECT_EQ(registrar0.GetAppScope(app_id_a), info_b.scope);
  EXPECT_EQ(registrar0.GetAppThemeColor(app_id_a), info_b.theme_color);

  // Install a separate app just to have something to await on to ensure the
  // sync has propagated to the other profile.
  WebApplicationInfo infoC;
  infoC.title = u"Different test name";
  infoC.start_url = GURL("http://www.notchromium.org/");
  AppId app_id_c = apps_helper::InstallWebApp(GetProfile(0), infoC);
  EXPECT_NE(app_id_a, app_id_c);
  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id_c);

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

  WebAppInstallObserver destInstallObserver(destProfile);

  // Install favicon only page as web app.
  AppId app_id;
  {
    Browser* browser = CreateBrowser(sourceProfile);
    ui_test_utils::NavigateToURL(
        browser, embedded_test_server()->GetURL("/web_apps/favicon_only.html"));
    chrome::SetAutoAcceptWebAppDialogForTesting(true, true);
    WebAppInstallObserver installObserver(sourceProfile);
    chrome::ExecuteCommand(browser, IDC_CREATE_SHORTCUT);
    app_id = installObserver.AwaitNextInstall();
    chrome::SetAutoAcceptWebAppDialogForTesting(false, false);
    chrome::CloseWindow(browser);
  }
  EXPECT_EQ(GetRegistrar(sourceProfile).GetAppShortName(app_id),
            "Favicon only");
  std::vector<WebApplicationIconInfo> icon_infos =
      GetRegistrar(sourceProfile).GetAppIconInfos(app_id);
  ASSERT_EQ(icon_infos.size(), 1u);
  EXPECT_FALSE(icon_infos[0].square_size_px.has_value());

  // Wait for app to sync across.
  AppId synced_app_id = destInstallObserver.AwaitNextInstall();
  EXPECT_EQ(synced_app_id, app_id);
  EXPECT_EQ(GetRegistrar(destProfile).GetAppShortName(app_id), "Favicon only");
  icon_infos = GetRegistrar(destProfile).GetAppIconInfos(app_id);
  ASSERT_EQ(icon_infos.size(), 1u);
  EXPECT_FALSE(icon_infos[0].square_size_px.has_value());
}

// Tests that we don't use the manifest start_url if it differs from what came
// through sync.
IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, SyncUsingStartUrlFallback) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile* source_profile = GetProfile(0);
  Profile* dest_profile = GetProfile(1);

  WebAppInstallObserver dest_install_observer(dest_profile);

  // Install app with name.
  WebApplicationInfo info;
  info.title = u"Test app";
  info.start_url =
      embedded_test_server()->GetURL("/web_apps/different_start_url.html");
  AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_EQ(GetRegistrar(source_profile).GetAppShortName(app_id), "Test app");
  EXPECT_EQ(GetRegistrar(source_profile).GetAppStartUrl(app_id),
            info.start_url);

  // Wait for app to sync across.
  AppId synced_app_id = dest_install_observer.AwaitNextInstall();
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

  WebAppInstallObserver dest_install_observer(dest_profile);

  // Install app with name.
  WebApplicationInfo info;
  info.title = u"Correct App Name";
  info.start_url =
      embedded_test_server()->GetURL("/web_apps/bad_title_only.html");
  AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_EQ(GetRegistrar(source_profile).GetAppShortName(app_id),
            "Correct App Name");

  // Wait for app to sync across.
  AppId synced_app_id = dest_install_observer.AwaitNextInstall();
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

  WebAppInstallObserver dest_install_observer(dest_profile);

  // Install app with name.
  WebApplicationInfo info;
  info.title = u"Incorrect App Name";
  info.start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_EQ(GetRegistrar(source_profile).GetAppShortName(app_id),
            "Incorrect App Name");

  // Wait for app to sync across.
  AppId synced_app_id = dest_install_observer.AwaitNextInstall();
  EXPECT_EQ(synced_app_id, app_id);
  EXPECT_EQ(GetRegistrar(dest_profile).GetAppShortName(app_id),
            "Basic web app");
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, SyncUsingIconUrlFallback) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile* source_profile = GetProfile(0);
  Profile* dest_profile = GetProfile(1);

  // Both bookmark app sync and web app sync happen at the same time. Disable
  // one of them to simulate the other winning the "race".
  InstallManager& install_manager =
      WebAppProvider::Get(dest_profile)->install_manager();
  install_manager.DisableBookmarkAppSyncInstallForTesting();

  WebAppInstallObserver dest_install_observer(dest_profile);

  // Install app with name.
  WebApplicationInfo info;
  info.title = u"Blue icon";
  info.start_url = GURL("https://does-not-exist.org");
  info.theme_color = SK_ColorBLUE;
  info.scope = GURL("https://does-not-exist.org/scope");
  WebApplicationIconInfo icon_info;
  icon_info.square_size_px = 192;
  icon_info.url = embedded_test_server()->GetURL("/web_apps/blue-192.png");
  icon_info.purpose = blink::mojom::ManifestImageResource_Purpose::ANY;
  info.icon_infos.push_back(icon_info);
  AppId app_id = apps_helper::InstallWebApp(GetProfile(0), info);
  EXPECT_EQ(GetRegistrar(source_profile).GetAppShortName(app_id), "Blue icon");

  // Wait for app to sync across.
  AppId synced_app_id = dest_install_observer.AwaitNextInstall();
  EXPECT_EQ(synced_app_id, app_id);
  EXPECT_EQ(GetRegistrar(dest_profile).GetAppShortName(app_id), "Blue icon");

  // Make sure icon downloaded despite not loading start_url.
  {
    base::RunLoop run_loop;
    WebAppProvider::Get(dest_profile)
        ->icon_manager()
        .ReadSmallestIconAny(
            synced_app_id, 192,
            base::BindLambdaForTesting([&run_loop](const SkBitmap& bitmap) {
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

}  // namespace web_app
