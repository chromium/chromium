// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/web_application_info.h"
#include "content/public/test/test_utils.h"

namespace web_app {

class TwoClientWebAppsSyncTest : public SyncTest {
 public:
  TwoClientWebAppsSyncTest() : SyncTest(TWO_CLIENT) { DisableVerifier(); }
  ~TwoClientWebAppsSyncTest() override = default;

  AppId InstallApp(const WebApplicationInfo& info, Profile* profile) {
    DCHECK(info.app_url.is_valid());
    base::RunLoop run_loop;
    AppId app_id;

    WebAppProvider::Get(profile)->install_manager().InstallWebAppFromInfo(
        std::make_unique<WebApplicationInfo>(info), ForInstallableSite::kYes,
        WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindLambdaForTesting(
            [&run_loop, &app_id](const AppId& new_app_id,
                                 InstallResultCode code) {
              DCHECK_EQ(code, InstallResultCode::kSuccessNewInstall);
              app_id = new_app_id;
              run_loop.Quit();
            }));
    run_loop.Run();

    const AppRegistrar& registrar = GetRegistrar(profile);
    DCHECK_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
    DCHECK_EQ(registrar.GetAppLaunchURL(app_id), info.app_url);

    return app_id;
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
  DISALLOW_COPY_AND_ASSIGN(TwoClientWebAppsSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, Basic) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());

  WebApplicationInfo info;
  info.title = base::UTF8ToUTF16("Test name");
  info.description = base::UTF8ToUTF16("Test description");
  info.app_url = GURL("http://www.chromium.org/path");
  info.scope = GURL("http://www.chromium.org/");
  AppId app_id = InstallApp(info, GetProfile(0));

  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id);
  const AppRegistrar& registrar = GetRegistrar(GetProfile(1));
  EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
  EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppDescription(app_id)),
            info.description);
  EXPECT_EQ(registrar.GetAppLaunchURL(app_id), info.app_url);
  EXPECT_EQ(registrar.GetAppScope(app_id), info.scope);

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, Minimal) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());

  WebApplicationInfo info;
  info.title = base::UTF8ToUTF16("Test name");
  info.app_url = GURL("http://www.chromium.org/");
  AppId app_id = InstallApp(info, GetProfile(0));

  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id);
  const AppRegistrar& registrar = GetRegistrar(GetProfile(1));
  EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
  EXPECT_EQ(registrar.GetAppLaunchURL(app_id), info.app_url);

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, ThemeColor) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());

  WebApplicationInfo info;
  info.title = base::UTF8ToUTF16("Test name");
  info.app_url = GURL("http://www.chromium.org/");
  info.theme_color = SK_ColorBLUE;
  AppId app_id = InstallApp(info, GetProfile(0));
  EXPECT_EQ(GetRegistrar(GetProfile(0)).GetAppThemeColor(app_id),
            info.theme_color);

  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id);
  const AppRegistrar& registrar = GetRegistrar(GetProfile(1));
  EXPECT_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
  EXPECT_EQ(registrar.GetAppLaunchURL(app_id), info.app_url);
  EXPECT_EQ(registrar.GetAppThemeColor(app_id), info.theme_color);

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, IsLocallyInstalled) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());

  WebApplicationInfo info;
  info.title = base::UTF8ToUTF16("Test name");
  info.app_url = GURL("http://www.chromium.org/");
  AppId app_id = InstallApp(info, GetProfile(0));
  EXPECT_TRUE(GetRegistrar(GetProfile(0)).IsLocallyInstalled(app_id));

  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id);
  bool is_locally_installed =
      GetRegistrar(GetProfile(1)).IsLocallyInstalled(app_id);
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(is_locally_installed);
#else
  EXPECT_FALSE(is_locally_installed);
#endif

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

IN_PROC_BROWSER_TEST_F(TwoClientWebAppsSyncTest, AppFieldsChangeDoesNotSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());

  const AppRegistrar& registrar0 = GetRegistrar(GetProfile(0));
  const AppRegistrar& registrar1 = GetRegistrar(GetProfile(1));

  WebApplicationInfo info_a;
  info_a.title = base::UTF8ToUTF16("Test name A");
  info_a.description = base::UTF8ToUTF16("Description A");
  info_a.app_url = GURL("http://www.chromium.org/path/to/start_url");
  info_a.scope = GURL("http://www.chromium.org/path/to/");
  info_a.theme_color = SK_ColorBLUE;
  AppId app_id_a = InstallApp(info_a, GetProfile(0));

  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id_a);
  EXPECT_EQ(base::UTF8ToUTF16(registrar1.GetAppShortName(app_id_a)),
            info_a.title);
  EXPECT_EQ(base::UTF8ToUTF16(registrar1.GetAppDescription(app_id_a)),
            info_a.description);
  EXPECT_EQ(registrar1.GetAppScope(app_id_a), info_a.scope);
  EXPECT_EQ(registrar1.GetAppThemeColor(app_id_a), info_a.theme_color);
  ASSERT_TRUE(AllProfilesHaveSameWebAppIds());

  // Reinstall same app in Profile 0 with a different metadata aside from the
  // name and start_url.
  WebApplicationInfo info_b;
  info_b.title = base::UTF8ToUTF16("Test name B");
  info_b.description = base::UTF8ToUTF16("Description B");
  info_b.app_url = GURL("http://www.chromium.org/path/to/start_url");
  info_b.scope = GURL("http://www.chromium.org/path/to/");
  info_b.theme_color = SK_ColorRED;
  AppId app_id_b = InstallApp(info_b, GetProfile(0));
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
  infoC.title = base::UTF8ToUTF16("Different test name");
  infoC.app_url = GURL("http://www.notchromium.org/");
  AppId app_id_c = InstallApp(infoC, GetProfile(0));
  EXPECT_NE(app_id_a, app_id_c);
  EXPECT_EQ(WebAppInstallObserver(GetProfile(1)).AwaitNextInstall(), app_id_c);

  // After sync we should not see the metadata update in Profile 1.
  EXPECT_EQ(base::UTF8ToUTF16(registrar1.GetAppShortName(app_id_a)),
            info_a.title);
  EXPECT_EQ(base::UTF8ToUTF16(registrar1.GetAppDescription(app_id_a)),
            info_a.description);
  EXPECT_EQ(registrar1.GetAppScope(app_id_a), info_a.scope);
  EXPECT_EQ(registrar1.GetAppThemeColor(app_id_a), info_a.theme_color);

  EXPECT_TRUE(AllProfilesHaveSameWebAppIds());
}

}  // namespace web_app
