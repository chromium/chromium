// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/shelf_model.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/apps/apk_web_app_installer.h"
#include "chrome/browser/chromeos/apps/apk_web_app_service.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"

namespace {

const char kPackageName[] = "com.google.maps";
const char kAppTitle[] = "Google Maps";
const char kAppUrl[] = "https://www.google.com/maps/";
const char kAppScope[] = "https://www.google.com/";
constexpr char kLastAppId[] = "last_app_id";
const char kAppActivity[] = "test.app.activity";
const char kAppActivity1[] = "test.app1.activity";
const char kPackageName1[] = "com.test.app";

const std::vector<uint8_t> GetFakeIconBytes() {
  auto fake_app_instance =
      std::make_unique<arc::FakeAppInstance>(/*app_host=*/nullptr);
  std::string png_data_as_string;
  EXPECT_TRUE(fake_app_instance->GenerateIconResponse(128, /*app_icon=*/true,
                                                      &png_data_as_string));
  return std::vector<uint8_t>(png_data_as_string.begin(),
                              png_data_as_string.end());
}

}  // namespace

namespace chromeos {

class ApkWebAppInstallerBrowserTest
    : public InProcessBrowserTest,
      public extensions::ExtensionRegistryObserver,
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
  }

  void SetUpOnMainThread() override { EnableArc(); }

  void TearDownOnMainThread() override { DisableArc(); }

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

  // ExtensionRegistryObserver:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override {
    installed_extension_ = extension;
    is_update_installed_ = is_update;
  }

  void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason uninstall_reason) override {
    uninstall_reason_ = uninstall_reason;
    // Make copies of required data: the |extension| object will be destroyed.
    uninstalled_extension_name_ = extension->name();
    uninstalled_extension_id_ = extension->id();
  }

  // ArcAppListPrefs::Observer:
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override {
    EXPECT_TRUE(uninstalled);
    removed_package_ = package_name;
  }

  void Reset() {
    removed_package_ = "";

    installed_extension_ = nullptr;
    is_update_installed_ = base::nullopt;

    uninstalled_extension_id_.clear();
    uninstalled_extension_name_.clear();
    uninstall_reason_ = extensions::UNINSTALL_REASON_FOR_TESTING;
  }

 protected:
  ArcAppListPrefs* arc_app_list_prefs_ = nullptr;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  std::string removed_package_;

  const extensions::Extension* installed_extension_ = nullptr;
  base::Optional<bool> is_update_installed_;

  extensions::ExtensionId uninstalled_extension_id_;
  std::string uninstalled_extension_name_;
  extensions::UninstallReason uninstall_reason_ =
      extensions::UNINSTALL_REASON_FOR_TESTING;
};

class ApkWebAppInstallerDelayedArcStartBrowserTest
    : public ApkWebAppInstallerBrowserTest {
  // Don't start ARC.
  void SetUpOnMainThread() override {}

  // Don't tear down ARC.
  void TearDownOnMainThread() override {}
};

class ApkWebAppInstallerWithLauncherControllerBrowserTest
    : public ApkWebAppInstallerBrowserTest {
 public:
  // ApkWebAppInstallerBrowserTest
  void SetUpOnMainThread() override {
    EnableArc();
    launcher_controller_ = ChromeLauncherController::instance();
    ASSERT_TRUE(launcher_controller_);
  }

  // ApkWebAppInstallerBrowserTest
  void TearDownOnMainThread() override { DisableArc(); }

 protected:
  ChromeLauncherController* launcher_controller_;
};

// Test the full installation and uninstallation flow.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest, InstallAndUninstall) {
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer(this);
  observer.Add(extensions::ExtensionRegistry::Get(browser()->profile()));
  ApkWebAppService* service = apk_web_app_service();
  service->SetArcAppListPrefsForTesting(arc_app_list_prefs_);

  web_app::AppId app_id;
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_TRUE(installed_extension_);
          EXPECT_EQ(kAppTitle, installed_extension_->name());
          EXPECT_FALSE(is_update_installed_.value());

          EXPECT_EQ(web_app_id, installed_extension_->id());
          EXPECT_EQ(kPackageName, package_name);
          app_id = web_app_id;
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  // Now send an uninstallation call from ARC, which should uninstall the
  // installed extension.
  {
    base::RunLoop run_loop;
    service->SetWebAppUninstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_FALSE(uninstalled_extension_id_.empty());
          EXPECT_EQ(kAppTitle, uninstalled_extension_name_);
          EXPECT_EQ(extensions::UNINSTALL_REASON_ARC, uninstall_reason_);

          EXPECT_EQ(app_id, uninstalled_extension_id_);
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
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer(this);
  observer.Add(extensions::ExtensionRegistry::Get(browser()->profile()));
  ApkWebAppService* service = apk_web_app_service();
  service->SetArcAppListPrefsForTesting(arc_app_list_prefs_);

  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.push_back(GetWebAppPackage(kPackageName, kAppTitle));

  base::RunLoop run_loop;
  service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
      [&](const std::string& package_name, const web_app::AppId& web_app_id) {
        EXPECT_TRUE(installed_extension_);
        EXPECT_EQ(kAppTitle, installed_extension_->name());
        EXPECT_FALSE(is_update_installed_.value());
        run_loop.Quit();
      }));

  app_instance_->SendRefreshPackageList(std::move(packages));
  run_loop.Run();
}

// Test uninstallation when ARC isn't running.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerDelayedArcStartBrowserTest,
                       DelayedUninstall) {
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer(this);
  observer.Add(extensions::ExtensionRegistry::Get(browser()->profile()));
  ApkWebAppService* service = apk_web_app_service();

  base::RunLoop run_loop;
  service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
      [&](const std::string& package_name, const web_app::AppId& web_app_id) {
        EXPECT_TRUE(installed_extension_);
        EXPECT_EQ(kAppTitle, installed_extension_->name());
        EXPECT_FALSE(is_update_installed_.value());
        run_loop.Quit();
      }));

  // Install an app from the raw data as if ARC had installed it.
  service->OnDidGetWebAppIcon(kPackageName, GetWebAppInfo(kAppTitle),
                              GetFakeIconBytes());
  run_loop.Run();

  // Uninstall the app on the extensions side. ARC uninstallation should be
  // queued.
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->UninstallExtension(installed_extension_->id(),
                           extensions::UNINSTALL_REASON_USER_INITIATED,
                           /*error=*/nullptr);
  EXPECT_EQ(extensions::UNINSTALL_REASON_USER_INITIATED, uninstall_reason_);

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
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer(this);
  observer.Add(extensions::ExtensionRegistry::Get(browser()->profile()));
  ApkWebAppService* service = apk_web_app_service();
  service->SetArcAppListPrefsForTesting(arc_app_list_prefs_);
  app_instance_->SendPackageAdded(GetArcAppPackage(kPackageName, kAppTitle));

  EXPECT_FALSE(installed_extension_);
  EXPECT_TRUE(uninstalled_extension_id_.empty());

  // Send a second package added call from ARC, upgrading the package to a web
  // app.
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_TRUE(uninstalled_extension_id_.empty());

          EXPECT_TRUE(installed_extension_);
          EXPECT_EQ(kAppTitle, installed_extension_->name());
          EXPECT_FALSE(is_update_installed_.value());
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  // Send an package added call from ARC, upgrading the package to not be a
  // web app. The extension should be uninstalled.
  {
    base::RunLoop run_loop;
    service->SetWebAppUninstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_FALSE(uninstalled_extension_id_.empty());
          EXPECT_EQ(kAppTitle, uninstalled_extension_name_);
          EXPECT_EQ(extensions::UNINSTALL_REASON_ARC, uninstall_reason_);
          run_loop.Quit();
        }));
    app_instance_->SendPackageAdded(GetArcAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }

  Reset();

  // Upgrade the package to a web app again and make sure it is installed again.
  {
    base::RunLoop run_loop;
    service->SetWebAppInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&](const std::string& package_name, const web_app::AppId& web_app_id) {
          EXPECT_TRUE(installed_extension_);
          EXPECT_EQ(kAppTitle, installed_extension_->name());
          EXPECT_FALSE(is_update_installed_.value());
          run_loop.Quit();
        }));

    app_instance_->SendPackageAdded(GetWebAppPackage(kPackageName, kAppTitle));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerWithLauncherControllerBrowserTest,
                       CheckPinStateAfterUpdate) {
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer(this);
  observer.Add(extensions::ExtensionRegistry::Get(browser()->profile()));
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

  EXPECT_FALSE(installed_extension_);
  EXPECT_TRUE(uninstalled_extension_id_.empty());
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
          keep_web_app_id = web_app_id;
          EXPECT_TRUE(installed_extension_);
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
          EXPECT_FALSE(uninstalled_extension_id_.empty());
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

}  // namespace chromeos
