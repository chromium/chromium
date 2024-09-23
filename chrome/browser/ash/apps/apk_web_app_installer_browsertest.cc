// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/apps/apk_web_app_installer.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPackageName[] = "com.google.maps";
const char kAppTitle[] = "Google Maps";
const char kAppUrl[] = "https://www.google.com/maps/";
const char kAppScope[] = "https://www.google.com/";
constexpr char kLastAppId[] = "last_app_id";
const char kAppActivity[] = "test.app.activity";
const char kAppActivity1[] = "test.app1.activity";
const char kPackageName1[] = "com.test.app";
const char kAppTitle1[] = "Test";
const char kAppUrl1[] = "https://www.test.app";
const char kAppScope1[] = "https://www.test.app/";

std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInstallInfo(
    const GURL& url) {
  auto web_app_install_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(url);
  web_app_install_info->title = u"App Title";
  web_app_install_info->theme_color = SK_ColorBLUE;
  web_app_install_info->scope = url.Resolve("scope");
  web_app_install_info->display_mode = web_app::DisplayMode::kBrowser;
  web_app_install_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  const std::vector<web_app::SquareSizePx> sizes_px{web_app::icon_size::k256,
                                                    web_app::icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorRED, SK_ColorYELLOW};
  web_app::AddIconsToWebAppInstallInfo(
      web_app_install_info.get(), url,
      {{web_app::IconPurpose::ANY, sizes_px, colors}});

  return web_app_install_info;
}

void ExpectInitialIconInfosFromWebAppInstallInfo(
    const std::vector<apps::IconInfo>& icon_infos,
    const GURL& url) {
  EXPECT_EQ(2u, icon_infos.size());

  EXPECT_EQ(url.Resolve("icon-256.png"), icon_infos[0].url);
  EXPECT_EQ(256, icon_infos[0].square_size_px);
  EXPECT_EQ(apps::IconInfo::Purpose::kAny, icon_infos[0].purpose);

  EXPECT_EQ(url.Resolve("icon-512.png"), icon_infos[1].url);
  EXPECT_EQ(512, icon_infos[1].square_size_px);
  EXPECT_EQ(apps::IconInfo::Purpose::kAny, icon_infos[1].purpose);
}

void ExpectInitialManifestFieldsFromWebAppInstallInfo(
    web_app::WebAppIconManager& icon_manager,
    const web_app::WebApp* web_app,
    const GURL& url) {
  // Manifest fields:
  EXPECT_EQ(web_app->untranslated_name(), "App Title");
  EXPECT_EQ(web_app->start_url(), url);
  EXPECT_EQ(web_app->scope().spec(), url.Resolve("scope"));
  EXPECT_EQ(web_app->display_mode(), web_app::DisplayMode::kBrowser);

  ASSERT_TRUE(web_app->theme_color().has_value());
  EXPECT_EQ(SK_ColorBLUE, web_app->theme_color().value());

  ASSERT_TRUE(web_app->sync_proto().has_theme_color());
  EXPECT_EQ(SK_ColorBLUE, web_app->sync_proto().theme_color());

  EXPECT_EQ("App Title", web_app->sync_proto().name());
  EXPECT_EQ(url.Resolve("scope").spec(), web_app->sync_proto().scope());
  {
    SCOPED_TRACE("web_app->manifest_icons()");
    ExpectInitialIconInfosFromWebAppInstallInfo(web_app->manifest_icons(), url);
  }
  {
    SCOPED_TRACE("web_app->sync_proto().icon_infos");
    std::optional<std::vector<apps::IconInfo>> parsed_icon_infos =
        web_app::ParseAppIconInfos(
            "ExpectInitialManifestFieldsFromWebAppInstallInfo",
            web_app->sync_proto().icon_infos());
    ASSERT_TRUE(parsed_icon_infos.has_value());
    ExpectInitialIconInfosFromWebAppInstallInfo(parsed_icon_infos.value(), url);
  }

  // Manifest Resources:
  EXPECT_EQ(web_app::IconManagerReadAppIconPixel(
                icon_manager, web_app->app_id(), /*size=*/256),
            SK_ColorRED);

  EXPECT_EQ(web_app::IconManagerReadAppIconPixel(
                icon_manager, web_app->app_id(), /*size=*/512),
            SK_ColorYELLOW);

  // User preferences:
  EXPECT_EQ(web_app->user_display_mode(),
            web_app::mojom::UserDisplayMode::kStandalone);
}

}  // namespace

namespace ash {

class ApkWebAppInstallerBrowserTest
    : public InProcessBrowserTest,
      public web_app::WebAppInstallManagerObserver,
      public ArcAppListPrefs::Observer {
 public:
  ApkWebAppInstallerBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void EnableArc() {
    arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);

    arc_app_list_prefs_ = ArcAppListPrefs::Get(browser()->profile());
    DCHECK(arc_app_list_prefs_);

    base::RunLoop run_loop;
    arc_app_list_prefs_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_prefs_);
    arc_app_list_prefs_->app_connection_holder()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_app_list_prefs_->app_connection_holder());

    arc_app_list_prefs_->AddObserver(this);
  }

  void DisableArc() {
    arc_app_list_prefs_->app_connection_holder()->CloseInstance(
        app_instance_.get());
    app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
    arc_app_list_prefs_ = nullptr;
  }

  void SetUpWebApps() {
    provider_ = web_app::WebAppProvider::GetForTest(browser()->profile());
    DCHECK(provider_);
    observation_.Observe(&provider_->install_manager());
  }

  void TearDownWebApps() {
    provider_ = nullptr;
    observation_.Reset();
  }

  void SetUpOnMainThread() override {
    EnableArc();
    app_instance_->SendRefreshPackageList({});
    SetUpWebApps();
  }

  void TearDownOnMainThread() override {
    DisableArc();
    TearDownWebApps();
  }

  arc::mojom::ArcPackageInfoPtr GetWebAppPackage(
      const std::string& package_name,
      const std::string& app_title = kAppTitle,
      const std::string& app_url = kAppUrl,
      const std::string& app_scope = kAppScope) {
    auto package = GetArcAppPackage(package_name, app_title);
    package->web_app_info = GetWebAppInfo(app_title, app_url, app_scope);

    return package;
  }

  arc::mojom::ArcPackageInfoPtr GetArcAppPackage(
      const std::string& package_name,
      const std::string& app_title) {
    auto package = arc::mojom::ArcPackageInfo::New();
    package->package_name = package_name;
    package->package_version = 1;
    package->last_backup_android_id = 1;
    package->last_backup_time = 1;
    package->sync = true;

    return package;
  }

  arc::mojom::WebAppInfoPtr GetWebAppInfo(
      const std::string& app_title,
      const std::string& app_url = kAppUrl,
      const std::string& app_scope = kAppScope) {
    return arc::mojom::WebAppInfo::New(app_title, app_url, app_scope, 100000);
  }

  ApkWebAppService* apk_web_app_service() {
    return ApkWebAppService::Get(browser()->profile());
  }

  web_app::WebAppIconManager& icon_manager() {
    return web_app::WebAppProvider::GetForTest(browser()->profile())
        ->icon_manager();
  }

  // Sets a callback to be called whenever an app is completely uninstalled and
  // removed from the Registrar.
  void set_app_uninstalled_callback(
      base::RepeatingCallback<void(const webapps::AppId&)> callback) {
    app_uninstalled_callback_ = callback;
  }

  // web_app::WebAppInstallManagerObserver overrides.
  void OnWebAppInstalled(const webapps::AppId& web_app_id) override {
    installed_web_app_ids_.push_back(web_app_id);
    installed_web_app_names_.push_back(
        provider_->registrar_unsafe().GetAppShortName(web_app_id));
  }

  void OnWebAppWillBeUninstalled(const webapps::AppId& web_app_id) override {
    uninstalled_web_app_ids_.push_back(web_app_id);
  }

  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override {
    if (app_uninstalled_callback_) {
      app_uninstalled_callback_.Run(app_id);
    }
  }

  // ArcAppListPrefs::Observer:
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override {
    if (uninstalled) {
      removed_packages_.push_back(package_name);
    }
  }

  void Reset() {
    removed_packages_.clear();
    installed_web_app_ids_.clear();
    installed_web_app_names_.clear();
    uninstalled_web_app_ids_.clear();
  }

 protected:
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      observation_{this};
  raw_ptr<ArcAppListPrefs, DanglingUntriaged> arc_app_list_prefs_ = nullptr;
  raw_ptr<web_app::WebAppProvider> provider_ = nullptr;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  base::RepeatingCallback<void(const webapps::AppId&)>
      app_uninstalled_callback_;

  std::vector<std::string> removed_packages_;
  std::vector<webapps::AppId> installed_web_app_ids_;
  std::vector<std::string> installed_web_app_names_;
  std::vector<webapps::AppId> uninstalled_web_app_ids_;
};

class ApkWebAppInstallerDelayedArcStartBrowserTest
    : public ApkWebAppInstallerBrowserTest {
  // Don't start ARC.
  void SetUpOnMainThread() override { SetUpWebApps(); }

  // Don't tear down ARC.
  void TearDownOnMainThread() override { TearDownWebApps(); }
};

class ApkWebAppInstallerWithShelfControllerBrowserTest
    : public ApkWebAppInstallerBrowserTest {
 public:
  // ApkWebAppInstallerBrowserTest
  void SetUpOnMainThread() override {
    ApkWebAppInstallerBrowserTest::SetUpOnMainThread();
    shelf_controller_ = ChromeShelfController::instance();
    ASSERT_TRUE(shelf_controller_);
  }

 protected:
  raw_ptr<ChromeShelfController, DanglingUntriaged> shelf_controller_;
};

// Test the full installation and uninstallation flow.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest, InstallAndUninstall) {
  ApkWebAppService* service = apk_web_app_service();

  webapps::AppId app_id;
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const webapps::AppId& web_app_id) {
          EXPECT_EQ(1u, installed_web_app_names_.size());
          EXPECT_EQ(1u, installed_web_app_ids_.size());
          EXPECT_EQ(kAppTitle, installed_web_app_names_[0]);
          EXPECT_EQ(web_app_id, installed_web_app_ids_[0]);
          EXPECT_EQ(kPackageName, package_name);
          app_id = web_app_id;
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  EXPECT_TRUE(service->IsWebAppShellPackage(kPackageName));
  EXPECT_TRUE(service->IsWebAppInstalledFromArc(app_id));

  EXPECT_EQ(service->GetPackageNameForWebApp(app_id), kPackageName);
  EXPECT_EQ(service->GetWebAppIdForPackageName(kPackageName), app_id);

  // Now send an uninstallation call from ARC, which should uninstall the
  // installed web app.
  {
    base::RunLoop run_loop;
    service->SetWebAppUninstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const webapps::AppId& web_app_id) {
          EXPECT_EQ(1u, uninstalled_web_app_ids_.size());
          EXPECT_EQ(app_id, uninstalled_web_app_ids_[0]);
          // No UninstallPackage happened.
          EXPECT_EQ("", package_name);
          run_loop.Quit();
        }));

    app_instance_->SendPackageUninstalled(kPackageName);
    run_loop.Run();
  }

  EXPECT_FALSE(service->IsWebAppShellPackage(kPackageName));
  EXPECT_FALSE(service->IsWebAppInstalledFromArc(app_id));

  EXPECT_EQ(service->GetPackageNameForWebApp(app_id), std::nullopt);
  EXPECT_EQ(service->GetWebAppIdForPackageName(kPackageName), std::nullopt);
}

// Test installation via PackageListRefreshed.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest, PackageListRefreshed) {
  ApkWebAppService* service = apk_web_app_service();

  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.push_back(GetWebAppPackage(kPackageName, kAppTitle));

  base::RunLoop run_loop;
  service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
      [&](const std::string& package_name, const webapps::AppId& web_app_id) {
        EXPECT_EQ(1u, installed_web_app_names_.size());
        EXPECT_EQ(1u, installed_web_app_ids_.size());
        EXPECT_EQ(kAppTitle, installed_web_app_names_[0]);
        EXPECT_EQ(web_app_id, installed_web_app_ids_[0]);
        run_loop.Quit();
      }));

  app_instance_->SendRefreshPackageList(std::move(packages));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerDelayedArcStartBrowserTest,
                       PRE_DelayedUninstall) {
  ApkWebAppService* service = apk_web_app_service();

  // Start up ARC and install two web app packages.
  EnableArc();
  app_instance_->SendRefreshPackageList({});

  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const webapps::AppId& web_app_id) {
          EXPECT_EQ(1u, installed_web_app_names_.size());
          EXPECT_EQ(1u, installed_web_app_ids_.size());
          EXPECT_EQ(kAppTitle, installed_web_app_names_[0]);
          EXPECT_EQ(web_app_id, installed_web_app_ids_[0]);
          EXPECT_EQ(kPackageName, package_name);
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const webapps::AppId& web_app_id) {
          EXPECT_EQ(2u, installed_web_app_names_.size());
          EXPECT_EQ(2u, installed_web_app_ids_.size());
          EXPECT_EQ(kAppTitle1, installed_web_app_names_[1]);
          EXPECT_EQ(web_app_id, installed_web_app_ids_[1]);
          EXPECT_EQ(kPackageName1, package_name);
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(
        GetWebAppPackage(kPackageName1, kAppTitle1, kAppUrl1, kAppScope1));
    run_loop.Run();
  }

  DisableArc();
}

// Tests uninstallation while ARC isn't running.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerDelayedArcStartBrowserTest,
                       DelayedUninstall) {
  ApkWebAppService* service = apk_web_app_service();

  // Uninstall both apps from the PRE_ test on the web apps side. ARC
  // uninstallation should be processed once ARC starts up.
  std::vector<webapps::AppId> uninstall_ids = {
      web_app::GenerateAppId(std::nullopt, GURL(kAppUrl)),
      web_app::GenerateAppId(std::nullopt, GURL(kAppUrl1))};

  for (const auto& id : uninstall_ids) {
    base::RunLoop run_loop;
    provider_->scheduler().RemoveUserUninstallableManagements(
        id, webapps::WebappUninstallSource::kShelf,
        base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
          EXPECT_EQ(code, webapps::UninstallResultCode::kAppRemoved);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  EnableArc();

  // Trigger a package refresh, which should call to ARC to remove the packages.
  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.push_back(GetWebAppPackage(kPackageName, kAppTitle));
  packages.push_back(
      GetWebAppPackage(kPackageName1, kAppTitle1, kAppUrl1, kAppScope1));
  app_instance_->SendRefreshPackageList(std::move(packages));

  EXPECT_EQ(2u, removed_packages_.size());
  EXPECT_TRUE(base::Contains(removed_packages_, kPackageName));
  EXPECT_TRUE(base::Contains(removed_packages_, kPackageName1));
  EXPECT_EQ(std::nullopt, service->GetPackageNameForWebApp(kAppUrl));
  EXPECT_EQ(std::nullopt, service->GetPackageNameForWebApp(kAppUrl1));

  arc_app_list_prefs_->RemoveObserver(this);
  DisableArc();
}

// Test an upgrade that becomes a web app and then stops being a web app.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest,
                       UpgradeToWebAppAndToArcApp) {
  ApkWebAppService* service = apk_web_app_service();
  app_instance_->SendPackageAdded(GetArcAppPackage(kPackageName, kAppTitle));

  EXPECT_TRUE(installed_web_app_ids_.empty());
  EXPECT_TRUE(uninstalled_web_app_ids_.empty());

  // Send a second package added call from ARC, upgrading the package to a web
  // app.
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const webapps::AppId& web_app_id) {
          EXPECT_EQ(1u, installed_web_app_names_.size());
          EXPECT_EQ(1u, installed_web_app_ids_.size());
          EXPECT_TRUE(uninstalled_web_app_ids_.empty());
          EXPECT_EQ(kAppTitle, installed_web_app_names_[0]);
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  // Send an package added call from ARC, upgrading the package to not be a
  // web app. The web app should be uninstalled.
  {
    base::RunLoop run_loop;
    service->SetWebAppUninstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const webapps::AppId& web_app_id) {
          EXPECT_EQ(uninstalled_web_app_ids_[0], installed_web_app_ids_[0]);
          run_loop.Quit();
        }));
    app_instance_->SendPackageAdded(GetArcAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  Reset();
  EXPECT_TRUE(installed_web_app_ids_.empty());
  EXPECT_TRUE(installed_web_app_names_.empty());

  // Upgrade the package to a web app again and make sure it is installed again.
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const webapps::AppId& web_app_id) {
          EXPECT_EQ(1u, installed_web_app_names_.size());
          EXPECT_EQ(1u, installed_web_app_ids_.size());
          EXPECT_EQ(kAppTitle, installed_web_app_names_[0]);
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }
}

// Test that when an ARC-installed Web App is uninstalled and then reinstalled
// as a regular web app, it is not treated as ARC-installed.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest,
                       UninstallAndReinstallAsWebApp) {
  ApkWebAppService* service = apk_web_app_service();

  // Install the Web App from ARC.
  webapps::AppId app_id;
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const webapps::AppId& web_app_id) {
          EXPECT_EQ(web_app_id, installed_web_app_ids_[0]);
          app_id = web_app_id;
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  ASSERT_TRUE(service->IsWebAppInstalledFromArc(app_id));

  // Uninstall the Web App from ARC.
  {
    base::RunLoop run_loop;
    // Wait until the app is completely uninstalled.
    set_app_uninstalled_callback(
        base::BindLambdaForTesting([&](const webapps::AppId& web_app_id) {
          EXPECT_EQ(app_id, web_app_id);
          run_loop.Quit();
        }));

    app_instance_->SendPackageUninstalled(kPackageName);
    run_loop.Run();
  }

  ASSERT_FALSE(service->IsWebAppInstalledFromArc(app_id));

  // Reinstall the Web App through the Browser.
  webapps::AppId non_arc_app_id = web_app::test::InstallDummyWebApp(
      browser()->profile(), kAppTitle, GURL(kAppUrl));
  ASSERT_EQ(app_id, non_arc_app_id);
  ASSERT_FALSE(service->IsWebAppInstalledFromArc(app_id));
}

IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerWithShelfControllerBrowserTest,
                       CheckPinStateAfterUpdate) {
  ApkWebAppService* service = apk_web_app_service();
  app_instance_->SendPackageAdded(GetArcAppPackage(kPackageName, kAppTitle));
  const std::string arc_app_id =
      ArcAppListPrefs::GetAppId(kPackageName, kAppActivity);

  /// Create an app and add to the package.
  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(arc::mojom::AppInfo::New(kAppTitle, kPackageName,
                                             kAppActivity, true /* sticky */));
  app_instance_->SendPackageAppListRefreshed(kPackageName, apps);

  EXPECT_TRUE(installed_web_app_ids_.empty());
  EXPECT_TRUE(uninstalled_web_app_ids_.empty());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id));

  // Pin the app to the shelf.
  PinAppWithIDToShelf(arc_app_id);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id));

  int pin_index = shelf_controller_->PinnedItemIndexByAppID(arc_app_id);

  arc_app_list_prefs_->SetPackagePrefs(kPackageName, kLastAppId,
                                       base::Value(arc_app_id));

  std::string keep_web_app_id;
  // Update ARC app to web app and check that the pinned app has
  // been updated.
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const webapps::AppId& web_app_id) {
          keep_web_app_id = web_app_id;
          EXPECT_EQ(1u, installed_web_app_names_.size());
          EXPECT_EQ(1u, installed_web_app_ids_.size());
          EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id));
          EXPECT_TRUE(shelf_controller_->IsAppPinned(keep_web_app_id));
          int new_index =
              shelf_controller_->PinnedItemIndexByAppID(keep_web_app_id);
          EXPECT_EQ(pin_index, new_index);
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  // Move the pin location of the app.
  app_instance_->SendPackageAdded(GetArcAppPackage(kPackageName1, kAppTitle));
  const std::string arc_app_id1 =
      ArcAppListPrefs::GetAppId(kPackageName1, kAppActivity1);
  shelf_controller_->PinAppAtIndex(arc_app_id1, pin_index);
  EXPECT_EQ(pin_index, shelf_controller_->PinnedItemIndexByAppID(arc_app_id1));

  // The app that was previously pinned will be shifted one to the right.
  pin_index += 1;
  EXPECT_EQ(pin_index,
            shelf_controller_->PinnedItemIndexByAppID(keep_web_app_id));

  // Update to ARC app and check the pinned app has updated.
  {
    base::RunLoop run_loop;
    service->SetWebAppUninstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const webapps::AppId& web_app_id) {
          EXPECT_EQ(1u, uninstalled_web_app_ids_.size());
          EXPECT_FALSE(shelf_controller_->IsAppPinned(web_app_id));
          EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id));
          int new_index = shelf_controller_->PinnedItemIndexByAppID(arc_app_id);
          EXPECT_EQ(pin_index, new_index);
          EXPECT_FALSE(shelf_controller_->IsAppPinned(keep_web_app_id));
          run_loop.Quit();
        }));
    app_instance_->SendPackageAdded(GetArcAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }
}

// Test that when a regular synced Web App is installed first and the same ARC
// Web App is installed we don't overwrite manifest fields obtained from full
// online install (especially sync fallback data).
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest,
                       InstallRegularWebAppFirstThenInstallFromArc) {
  ApkWebAppService* service = apk_web_app_service();

  // Install the Web App as if the user installs it.
  std::unique_ptr<web_app::WebAppInstallInfo> web_app_install_info =
      CreateWebAppInstallInfo(GURL(kAppUrl));

  webapps::AppId app_id = web_app::test::InstallWebApp(
      browser()->profile(), std::move(web_app_install_info),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::SYNC);

  ASSERT_FALSE(service->IsWebAppInstalledFromArc(app_id));

  const web_app::WebApp* web_app =
      provider_->registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  EXPECT_TRUE(web_app->IsSynced());
  EXPECT_FALSE(web_app->IsWebAppStoreInstalledApp());

  {
    SCOPED_TRACE("Expect initial manifest fields.");
    ExpectInitialManifestFieldsFromWebAppInstallInfo(icon_manager(), web_app,
                                                     GURL(kAppUrl));
  }

  // Install the Web App from ARC.
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(
        base::BindLambdaForTesting([&](const std::string& package_name,
                                       const webapps::AppId& installed_app_id) {
          EXPECT_EQ(app_id, installed_app_id);
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  ASSERT_TRUE(service->IsWebAppInstalledFromArc(app_id));

  EXPECT_EQ(web_app, provider_->registrar_unsafe().GetAppById(app_id));

  EXPECT_TRUE(web_app->IsSynced());
  EXPECT_TRUE(web_app->IsWebAppStoreInstalledApp());

  {
    SCOPED_TRACE("Expect same manifest fields, no overwrites.");
    ExpectInitialManifestFieldsFromWebAppInstallInfo(icon_manager(), web_app,
                                                     GURL(kAppUrl));
  }
}

// Test that when ARC Web App is installed first and then same regular synced
// Web App is installed we overwrite the apk manifest fields with fields
// obtained from full online install (especially sync fallback data).
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest,
                       InstallFromArcFirstThenRegularWebApp) {
  ApkWebAppService* service = apk_web_app_service();

  webapps::AppId app_id;

  // Install the Web App from ARC.
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const webapps::AppId& apk_app_id) {
          app_id = apk_app_id;
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  ASSERT_TRUE(service->IsWebAppInstalledFromArc(app_id));

  const web_app::WebApp* web_app =
      provider_->registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  EXPECT_TRUE(web_app->IsWebAppStoreInstalledApp());
  EXPECT_FALSE(web_app->IsSynced());

  // Install the Web App as if the user installs it.
  std::unique_ptr<web_app::WebAppInstallInfo> web_app_install_info =
      CreateWebAppInstallInfo(GURL(kAppUrl));

  webapps::AppId web_app_id = web_app::test::InstallWebApp(
      browser()->profile(), std::move(web_app_install_info),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::SYNC);
  ASSERT_EQ(app_id, web_app_id);

  EXPECT_TRUE(service->IsWebAppInstalledFromArc(app_id));

  EXPECT_EQ(web_app, provider_->registrar_unsafe().GetAppById(app_id));

  EXPECT_TRUE(web_app->IsWebAppStoreInstalledApp());
  EXPECT_TRUE(web_app->IsSynced());

  {
    SCOPED_TRACE(
        "Expect online manifest fields, the offline fields from ARC have been "
        "overwritten.");
    ExpectInitialManifestFieldsFromWebAppInstallInfo(icon_manager(), web_app,
                                                     GURL(kAppUrl));
  }
}

IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerDelayedArcStartBrowserTest,
                       RemoveWebAppWhenArcDisabled) {
  EnableArc();
  app_instance_->SendRefreshPackageList({});

  ApkWebAppService* service = apk_web_app_service();

  // Install the Web App from ARC.
  base::test::TestFuture<const std::string&, const webapps::AppId&>
      installed_future;
  service->SetWebAppInstalledCallbackForTesting(installed_future.GetCallback());
  app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));

  webapps::AppId installed_app_id = installed_future.Get<1>();
  ASSERT_TRUE(service->IsWebAppInstalledFromArc(installed_app_id));

  // Disable ARC through settings and check that the app was uninstalled.
  base::test::TestFuture<const std::string&, const webapps::AppId&>
      uninstalled_future;
  service->SetWebAppUninstalledCallbackForTesting(
      uninstalled_future.GetCallback());

  arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), false);
  DisableArc();

  ASSERT_EQ(uninstalled_future.Get<1>(), installed_app_id);
}

IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest,
                       MigrateFromDeprecatedToCanonical) {
  constexpr char kCanonicalPackage[] = "com.google.android.apps.tachyon";
  constexpr char kDeprecatedPackage[] = "com.google.android.apps.meetings";

  ApkWebAppService* service = apk_web_app_service();

  base::test::TestFuture<const std::string&, const webapps::AppId&>
      installed_future;
  service->SetWebAppInstalledCallbackForTesting(installed_future.GetCallback());
  app_instance_->SendPackageAdded(GetWebAppPackage(kDeprecatedPackage));

  webapps::AppId installed_app_id = installed_future.Get<1>();
  ASSERT_EQ(service->GetPackageNameForWebApp(installed_app_id),
            kDeprecatedPackage);

  // Installing the canonical web app package should migrate away from the
  // deprecated package.
  app_instance_->SendPackageAdded(GetWebAppPackage(kCanonicalPackage));

  ASSERT_EQ(service->GetPackageNameForWebApp(installed_app_id),
            kCanonicalPackage);
  ASSERT_THAT(removed_packages_, testing::Contains(kDeprecatedPackage));
}

IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerDelayedArcStartBrowserTest,
                       MigrateAndInstallWebApp) {
  constexpr char kCanonicalPackage[] = "com.google.android.apps.tachyon";
  constexpr char kDeprecatedPackage[] = "com.google.android.apps.meetings";

  ApkWebAppService* service = apk_web_app_service();

  base::test::TestFuture<const std::string&, const webapps::AppId&>
      installed_future;
  service->SetWebAppInstalledCallbackForTesting(installed_future.GetCallback());

  // Install both canonical and deprecated apps at once, without the web app
  // being installed yet. The deprecated package should uninstall and the web
  // app should install associated with the canonical app.
  auto deprecated_package = GetWebAppPackage(kDeprecatedPackage);
  auto canonical_package = GetWebAppPackage(kCanonicalPackage);
  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.push_back(std::move(deprecated_package));
  packages.push_back(std::move(canonical_package));

  EnableArc();
  app_instance_->SendRefreshPackageList(std::move(packages));

  webapps::AppId installed_app_id = installed_future.Get<1>();
  ASSERT_EQ(service->GetPackageNameForWebApp(installed_app_id),
            kCanonicalPackage);
  ASSERT_THAT(removed_packages_, testing::Contains(kDeprecatedPackage));
}

IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest,
                       MigrateOnUpgradeToWebApp) {
  constexpr char kCanonicalPackage[] = "com.google.android.apps.tachyon";
  constexpr char kDeprecatedPackage[] = "com.google.android.apps.meetings";

  ApkWebAppService* service = apk_web_app_service();

  base::test::TestFuture<const std::string&, const webapps::AppId&>
      installed_future;
  service->SetWebAppInstalledCallbackForTesting(installed_future.GetCallback());

  // Install the canonical app as an Android app, and the deprecated app as a
  // web app. We should not migrate yet.
  app_instance_->SendPackageAdded(
      GetArcAppPackage(kCanonicalPackage, kAppTitle));
  app_instance_->SendPackageAdded(GetWebAppPackage(kDeprecatedPackage));

  webapps::AppId installed_app_id = installed_future.Get<1>();

  ASSERT_EQ(service->GetPackageNameForWebApp(installed_app_id),
            kDeprecatedPackage);

  // When the canonical package updates to a web app, the migration should go
  // ahead.
  app_instance_->SendPackageAdded(GetWebAppPackage(kCanonicalPackage));

  ASSERT_EQ(service->GetPackageNameForWebApp(installed_app_id),
            kCanonicalPackage);
  ASSERT_THAT(removed_packages_, testing::Contains(kDeprecatedPackage));
}

IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest,
                       InstallAndUninstallArcOverUserInstall) {
  ApkWebAppService* service = apk_web_app_service();

  // Install the Web App as if the user installed it.
  std::unique_ptr<web_app::WebAppInstallInfo> web_app_install_info =
      CreateWebAppInstallInfo(GURL(kAppUrl));

  webapps::AppId app_id = web_app::test::InstallWebApp(
      browser()->profile(), std::move(web_app_install_info),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::SYNC);

  // Then also install the Web App from ARC.
  {
    base::test::TestFuture<const std::string&, const webapps::AppId&>
        installed_future;
    service->SetWebAppInstalledCallbackForTesting(
        installed_future.GetCallback());

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    webapps::AppId arc_app_id = installed_future.Get<1>();

    ASSERT_EQ(app_id, arc_app_id);
  }

  ASSERT_TRUE(service->IsWebAppInstalledFromArc(app_id));

  // Uninstall the Web App from ARC.
  base::test::TestFuture<const std::string&, const webapps::AppId&>
      uninstalled_future;
  service->SetWebAppUninstalledCallbackForTesting(
      uninstalled_future.GetCallback());

  app_instance_->SendPackageUninstalled(kPackageName);
  ASSERT_TRUE(uninstalled_future.Wait());

  // The app should still be installed, but is no longer registered in
  // ApkWebAppService.
  ASSERT_FALSE(service->IsWebAppInstalledFromArc(app_id));
  ASSERT_TRUE(provider_->registrar_unsafe().GetAppById(app_id));
}

}  // namespace ash
