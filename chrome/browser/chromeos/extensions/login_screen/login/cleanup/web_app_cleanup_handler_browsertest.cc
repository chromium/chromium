// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/web_app_cleanup_handler.h"

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace web_app {

namespace {

const std::u16string kApp1Title = u"Example App1";
constexpr char kApp1StartURL[] = "https://example_url1.com/";
constexpr char kApp1InstallURL[] = "https://example_url1.com/install";

constexpr char kApp2InstallURL[] = "https://example_url2.com/install";

const std::u16string kApp3Title = u"Example App3";
constexpr char kApp3StartURL[] = "https://example_url3.com/";
constexpr char kApp3InstallURL[] = "https://example_url3.com/install";

const std::u16string kApp4Title = u"Example App4";
constexpr char kApp4StartURL[] = "https://example_url4.com/";
constexpr char kApp4InstallURL[] = "https://example_url4.com/install";

}  // namespace

class WebAppCleanupHandlerBrowserTest : public WebAppBrowserTestBase {
 protected:
  WebAppCleanupHandlerBrowserTest()
      : skip_preinstalled_web_app_startup_(
            PreinstalledWebAppManager::SkipStartupForTesting()) {}
  ~WebAppCleanupHandlerBrowserTest() override = default;

  webapps::AppId InstallWebApp(std::u16string title,
                               GURL start_url,
                               GURL install_url,
                               webapps::WebappInstallSource install_source) {
    auto app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    app_info->title = title;
    app_info->install_url = install_url;
    return test::InstallWebApp(profile(), std::move(app_info),
                               /*overwrite_existing_manifest_fields=*/false,
                               install_source);
  }

  webapps::AppId InstallWebAppFromPolicy(const std::string& install_url) {
    WebAppTestInstallWithOsHooksObserver observer(profile());
    observer.BeginListening();
    {
      ScopedListPrefUpdate update(profile()->GetPrefs(),
                                  prefs::kWebAppInstallForceList);
      update->Append(base::Value::Dict().Set(web_app::kUrlKey, install_url));
    }
    webapps::AppId app_id = observer.Wait();
    return app_id;
  }

  WebAppRegistrar& registrar_unsafe() { return provider().registrar_unsafe(); }

  base::AutoReset<bool> skip_preinstalled_web_app_startup_;
  chromeos::WebAppCleanupHandler web_app_cleanup_handler_;
};

IN_PROC_BROWSER_TEST_F(WebAppCleanupHandlerBrowserTest,
                       NoUserInstalledWebApps) {
  webapps::AppId app_id1 =
      InstallWebApp(kApp1Title, GURL(kApp1StartURL), GURL(kApp1InstallURL),
                    webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  webapps::AppId app_id2 = InstallWebAppFromPolicy(kApp2InstallURL);

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id1));
  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id2));

  base::test::TestFuture<const std::optional<std::string>&> future;
  web_app_cleanup_handler_.Cleanup(future.GetCallback());
  EXPECT_EQ(future.Get(), std::nullopt);

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id1));
  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id2));
}

IN_PROC_BROWSER_TEST_F(WebAppCleanupHandlerBrowserTest,
                       UninstallsUserInstalledWebApps) {
  webapps::AppId app_id1 =
      InstallWebApp(kApp1Title, GURL(kApp1StartURL), GURL(kApp1InstallURL),
                    webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  webapps::AppId app_id2 = InstallWebAppFromPolicy(kApp2InstallURL);

  // User-installed apps.
  webapps::AppId app_id3 =
      InstallWebApp(kApp3Title, GURL(kApp3StartURL), GURL(kApp3InstallURL),
                    webapps::WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB);
  webapps::AppId app_id4 =
      InstallWebApp(kApp4Title, GURL(kApp4StartURL), GURL(kApp4InstallURL),
                    webapps::WebappInstallSource::SYNC);

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id1));
  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id2));
  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id3));
  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id4));

  base::test::TestFuture<const std::optional<std::string>&> future;
  web_app_cleanup_handler_.Cleanup(future.GetCallback());
  EXPECT_EQ(future.Get(), std::nullopt);

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id1));
  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id2));
  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id3));
  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id4));
}

IN_PROC_BROWSER_TEST_F(WebAppCleanupHandlerBrowserTest,
                       DoesNotUninstallUserAndPolicyInstalledApp) {
  webapps::AppId app_id1 =
      InstallWebApp(kApp1Title, GURL(kApp1StartURL), GURL(kApp1InstallURL),
                    webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  webapps::AppId app_id2 = InstallWebAppFromPolicy(kApp2InstallURL);

  // App that is both installed by the user and by policy.
  webapps::AppId app_id3 =
      InstallWebApp(kApp3Title, GURL(kApp3InstallURL), GURL(kApp3InstallURL),
                    webapps::WebappInstallSource::SYNC);
  webapps::AppId policy_app_id = InstallWebAppFromPolicy(kApp3InstallURL);
  EXPECT_EQ(policy_app_id, app_id3);

  // User-installed app.
  webapps::AppId app_id4 =
      InstallWebApp(kApp4Title, GURL(kApp4StartURL), GURL(kApp4InstallURL),
                    webapps::WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB);

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id1));
  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id2));
  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id3));
  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id4));

  // Web App 3 has two install sources out if which one is a user install source
  // (kSync).
  const WebApp* web_app3 = registrar_unsafe().GetAppById(app_id3);
  EXPECT_TRUE(web_app3->GetSources().Has(WebAppManagement::kSync));
  EXPECT_TRUE(web_app3->GetSources().Has(WebAppManagement::kPolicy));

  base::test::TestFuture<const std::optional<std::string>&> future;
  web_app_cleanup_handler_.Cleanup(future.GetCallback());
  EXPECT_EQ(future.Get(), std::nullopt);

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id1));
  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id2));
  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id3));
  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id4));

  // Web App 3 is still installed but the user install source (kSync) is
  // removed.
  EXPECT_FALSE(web_app3->GetSources().Has(WebAppManagement::kSync));
  EXPECT_TRUE(web_app3->GetSources().Has(WebAppManagement::kPolicy));
}

}  // namespace web_app
