// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/shelf_model.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/apps/apk_web_app_installer.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "content/public/test/browser_test.h"
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

arc::mojom::RawIconPngDataPtr GetFakeIconBytes() {
  auto fake_app_instance =
      std::make_unique<arc::FakeAppInstance>(/*app_host=*/nullptr);
  return fake_app_instance->GenerateIconResponse(128, /*app_icon=*/true);
}

}  // namespace

namespace ash {

class ApkWebAppInstallerBrowserTest : public InProcessBrowserTest,
                                      public web_app::AppRegistrarObserver,
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
  }

  void DisableArc() {
    arc_app_list_prefs_->app_connection_holder()->CloseInstance(
        app_instance_.get());
    app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
    arc_app_list_prefs_ = nullptr;
  }

  void SetUpWebApps() {
    provider_ = web_app::WebAppProvider::Get(browser()->profile());
    DCHECK(provider_);
    observer_.Add(&provider_->registrar());
  }

  void TearDownWebApps() {
    provider_ = nullptr;
    observer_.RemoveAll();
  }

  void SetUpOnMainThread() override {
    EnableArc();
    SetUpWebApps();
  }

  void TearDownOnMainThread() override {
    DisableArc();
    TearDownWebApps();
  }

  arc::mojom::ArcPackageInfoPtr GetWebAppPackage(
      const std::string& package_name,
      const std::string& app_title) {
    auto package = GetArcAppPackage(package_name, app_title);
    package->web_app_info = GetWebAppInfo(app_title);

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
    package->system = false;

    return package;
  }

  arc::mojom::WebAppInfoPtr GetWebAppInfo(const std::string& app_title) {
    return arc::mojom::WebAppInfo::New(app_title, kAppUrl, kAppScope, 100000);
  }

  ApkWebAppService* apk_web_app_service() {
    return ApkWebAppService::Get(browser()->profile());
  }

  // web_app::AppRegistrarObserver overrides.
  void OnWebAppInstalled(const web_app::AppId& web_app_id) override {
    installed_web_app_id_ = web_app_id;
    installed_web_app_name_ =
        provider_->registrar().GetAppShortName(web_app_id);
  }

  void OnWebAppWillBeUninstalled(const web_app::AppId& web_app_id) override {
    uninstalled_web_app_id_ = web_app_id;
  }

  // ArcAppListPrefs::Observer:
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override {
    EXPECT_TRUE(uninstalled);
    removed_package_ = package_name;
  }

  void Reset() {
    removed_package_.clear();
    installed_web_app_id_.clear();
    installed_web_app_name_.clear();
    uninstalled_web_app_id_.clear();
  }

 protected:
  ScopedObserver<web_app::AppRegistrar, web_app::AppRegistrarObserver>
      observer_{this};
  ArcAppListPrefs* arc_app_list_prefs_ = nullptr;
  web_app::WebAppProvider* provider_ = nullptr;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;

  std::string removed_package_;
  web_app::AppId installed_web_app_id_;
  std::string installed_web_app_name_;
  web_app::AppId uninstalled_web_app_id_;
};

class ApkWebAppInstallerDelayedArcStartBrowserTest
    : public ApkWebAppInstallerBrowserTest {
  // Don't start ARC.
  void SetUpOnMainThread() override { SetUpWebApps(); }

  // Don't tear down ARC.
  void TearDownOnMainThread() override { TearDownWebApps(); }
};

class ApkWebAppInstallerWithLauncherControllerBrowserTest
    : public ApkWebAppInstallerBrowserTest {
 public:
  // ApkWebAppInstallerBrowserTest
  void SetUpOnMainThread() override {
    EnableArc();
    SetUpWebApps();
    launcher_controller_ = ChromeLauncherController::instance();
    ASSERT_TRUE(launcher_controller_);
  }

  // ApkWebAppInstallerBrowserTest
  void TearDownOnMainThread() override {
    DisableArc();
    TearDownWebApps();
  }

 protected:
  ChromeLauncherController* launcher_controller_;
};

// Test the full installation and uninstallation flow.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest, InstallAndUninstall) {
  ApkWebAppService* service = apk_web_app_service();
  service->SetArcAppListPrefsForTesting(arc_app_list_prefs_);

  web_app::AppId app_id;
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_EQ(kAppTitle, installed_web_app_name_);
          EXPECT_EQ(web_app_id, installed_web_app_id_);
          EXPECT_EQ(kPackageName, package_name);
          app_id = web_app_id;
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  // Now send an uninstallation call from ARC, which should uninstall the
  // installed web app.
  {
    base::RunLoop run_loop;
    service->SetWebAppUninstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_FALSE(uninstalled_web_app_id_.empty());
          EXPECT_EQ(app_id, uninstalled_web_app_id_);
          // No UninstallPackage happened.
          EXPECT_EQ("", package_name);
          run_loop.Quit();
        }));

    app_instance_->SendPackageUninstalled(kPackageName);
    run_loop.Run();
  }
}

// Test installation via PackageListRefreshed.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest, PackageListRefreshed) {
  ApkWebAppService* service = apk_web_app_service();
  service->SetArcAppListPrefsForTesting(arc_app_list_prefs_);

  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.push_back(GetWebAppPackage(kPackageName, kAppTitle));

  base::RunLoop run_loop;
  service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
      [&](const std::string& package_name, const web_app::AppId& web_app_id) {
        EXPECT_EQ(kAppTitle, installed_web_app_name_);
        EXPECT_EQ(web_app_id, installed_web_app_id_);
        run_loop.Quit();
      }));

  app_instance_->SendRefreshPackageList(std::move(packages));
  run_loop.Run();
}

// Test uninstallation when ARC isn't running.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerDelayedArcStartBrowserTest,
                       DelayedUninstall) {
  ApkWebAppService* service = apk_web_app_service();

  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_EQ(kAppTitle, installed_web_app_name_);
          EXPECT_EQ(web_app_id, installed_web_app_id_);
          EXPECT_EQ(kPackageName, package_name);
          run_loop.Quit();
        }));

    // Install an app from the raw data as if ARC had installed it.
    service->OnDidGetWebAppIcon(kPackageName, GetWebAppInfo(kAppTitle),
                                GetFakeIconBytes());
    run_loop.Run();
  }

  // Uninstall the app on the web apps side. ARC uninstallation should be
  // queued.
  {
    base::RunLoop run_loop;
    provider_->install_finalizer().UninstallExternalWebApp(
        installed_web_app_id_, web_app::ExternalInstallSource::kArc,
        base::BindLambdaForTesting([&](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Start up ARC and set the package to be installed.
  EnableArc();
  app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));

  // Trigger a package refresh, which should call to ARC to remove the package.
  arc_app_list_prefs_->AddObserver(this);
  service->SetArcAppListPrefsForTesting(arc_app_list_prefs_);
  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.push_back(GetWebAppPackage(kPackageName, kAppTitle));
  app_instance_->SendRefreshPackageList(std::move(packages));

  EXPECT_EQ(kPackageName, removed_package_);

  arc_app_list_prefs_->RemoveObserver(this);
  DisableArc();
}

// Test an upgrade that becomes a web app and then stops being a web app.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest,
                       UpgradeToWebAppAndToArcApp) {
  ApkWebAppService* service = apk_web_app_service();
  service->SetArcAppListPrefsForTesting(arc_app_list_prefs_);
  app_instance_->SendPackageAdded(GetArcAppPackage(kPackageName, kAppTitle));

  EXPECT_TRUE(installed_web_app_id_.empty());
  EXPECT_TRUE(uninstalled_web_app_id_.empty());

  // Send a second package added call from ARC, upgrading the package to a web
  // app.
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_TRUE(uninstalled_web_app_id_.empty());
          EXPECT_EQ(kAppTitle, installed_web_app_name_);
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
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_EQ(uninstalled_web_app_id_, installed_web_app_id_);
          run_loop.Quit();
        }));
    app_instance_->SendPackageAdded(GetArcAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  Reset();
  EXPECT_TRUE(installed_web_app_id_.empty());
  EXPECT_TRUE(installed_web_app_name_.empty());

  // Upgrade the package to a web app again and make sure it is installed again.
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_FALSE(installed_web_app_id_.empty());
          EXPECT_EQ(kAppTitle, installed_web_app_name_);
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerWithLauncherControllerBrowserTest,
                       CheckPinStateAfterUpdate) {
  ApkWebAppService* service = apk_web_app_service();
  service->SetArcAppListPrefsForTesting(arc_app_list_prefs_);
  app_instance_->SendPackageAdded(GetArcAppPackage(kPackageName, kAppTitle));
  const std::string arc_app_id =
      ArcAppListPrefs::GetAppId(kPackageName, kAppActivity);

  /// Create an app and add to the package.
  arc::mojom::AppInfo app;
  app.name = kAppTitle;
  app.package_name = kPackageName;
  app.activity = kAppActivity;
  app.sticky = true;
  app_instance_->SendPackageAppListRefreshed(kPackageName, {app});

  EXPECT_TRUE(installed_web_app_id_.empty());
  EXPECT_TRUE(uninstalled_web_app_id_.empty());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id));

  // Pin the app to the shelf.
  launcher_controller_->PinAppWithID(arc_app_id);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id));

  int pin_index = launcher_controller_->PinnedItemIndexByAppID(arc_app_id);

  arc_app_list_prefs_->SetPackagePrefs(kPackageName, kLastAppId,
                                       base::Value(arc_app_id));

  std::string keep_web_app_id;
  // Update ARC app to web app and check that the pinned app has
  // been updated.
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          // Web apps update the launcher asynchronously, so flush the App
          // Service's mojo calls to ensure that happens.
          auto* proxy =
              apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
          proxy->FlushMojoCallsForTesting();
          keep_web_app_id = web_app_id;
          EXPECT_FALSE(installed_web_app_id_.empty());
          EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id));
          EXPECT_TRUE(launcher_controller_->IsAppPinned(keep_web_app_id));
          int new_index =
              launcher_controller_->PinnedItemIndexByAppID(keep_web_app_id);
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
  launcher_controller_->PinAppAtIndex(arc_app_id1, pin_index);
  EXPECT_EQ(pin_index,
            launcher_controller_->PinnedItemIndexByAppID(arc_app_id1));

  // The app that was previously pinned will be shifted one to the right.
  pin_index += 1;
  EXPECT_EQ(pin_index,
            launcher_controller_->PinnedItemIndexByAppID(keep_web_app_id));

  // Update to ARC app and check the pinned app has updated.
  {
    base::RunLoop run_loop;
    service->SetWebAppUninstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_FALSE(uninstalled_web_app_id_.empty());
          EXPECT_FALSE(launcher_controller_->IsAppPinned(web_app_id));
          EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id));
          int new_index =
              launcher_controller_->PinnedItemIndexByAppID(arc_app_id);
          EXPECT_EQ(pin_index, new_index);
          EXPECT_FALSE(launcher_controller_->IsAppPinned(keep_web_app_id));
          run_loop.Quit();
        }));
    app_instance_->SendPackageAdded(GetArcAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }
}

}  // namespace ash
