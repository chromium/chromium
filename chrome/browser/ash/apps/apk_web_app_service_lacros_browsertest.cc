// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>

#include "ash/components/arc/mojom/app.mojom-forward.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/apps/apk_web_app_service_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

arc::mojom::ArcPackageInfoPtr GetArcAppPackage(const std::string& name) {
  return arc::mojom::ArcPackageInfo::New("org.example." + name,
                                         /*package_version=*/1,
                                         /*last_backup_android_id=*/1,
                                         /*last_backup_time=*/1,
                                         /*sync=*/true,
                                         /*system=*/false);
}

arc::mojom::ArcPackageInfoPtr GetWebAppPackage(const std::string& name) {
  auto package = GetArcAppPackage(name);
  package->web_app_info = arc::mojom::WebAppInfo::New(
      /*title=*/name,
      /*start_url=*/"https://example.org/" + name + "?start",
      /*scope_url=*/"https://example.org/" + name,
      /*theme_color=*/100000,
      /*is_web_only_twa=*/true,
      /*certificate_sha256_fingerprint=*/name + "-sha1");
  return package;
}

}  // namespace

class ApkWebAppServiceLacrosBrowserTest : public InProcessBrowserTest,
                                          public ApkWebAppService::Delegate {
 public:
  ApkWebAppServiceLacrosBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        ash::standalone_browser::GetFeatureRefs(), {});
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnableLacrosForTesting);
    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ApkWebAppServiceLacrosBrowserTest::SetTestingFactory,
                base::Unretained(this)));
  }

  void SetTestingFactory(content::BrowserContext* context) {
    ApkWebAppServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &ApkWebAppServiceLacrosBrowserTest::CreateApkWebAppService,
                     base::Unretained(this)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    // Create a new ash browser window for things that use browser().
    Profile* profile = ProfileManager::GetActiveUserProfile();
    DCHECK(profile);
    chrome::NewEmptyWindow(profile);
    SelectFirstBrowser();
    DCHECK(browser());
    DCHECK_EQ(browser()->profile(), profile);

    arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);
  }

  // `ApkWebAppService::Delegate` implementation, stubs out ARC And Lacros.

  void MaybeInstallWebAppInLacros(const std::string& package_name,
                                  arc::mojom::WebAppInfoPtr web_app_info,
                                  WebAppInstallCallback callback) override {
    if (!lacros_running_) {
      return;
    }

    auto app = std::make_unique<apps::App>(
        apps::AppType::kWeb,
        web_app::GenerateAppId(
            /*manifest_id=*/std::nullopt, GURL(web_app_info->start_url)));
    app->readiness = apps::Readiness::kReady;
    app->publisher_id = web_app_info->start_url;

    lacros_web_apps_[app->app_id] = app->Clone();

    std::vector<apps::AppPtr> apps;
    apps.push_back(app->Clone());
    PublishToAppService(std::move(apps));

    std::move(callback).Run(app->app_id, web_app_info->is_web_only_twa,
                            web_app_info->certificate_sha256_fingerprint,
                            webapps::InstallResultCode::kSuccessNewInstall);
  }

  void MaybeUninstallWebAppInLacros(const webapps::AppId& web_app_id,
                                    WebAppUninstallCallback callback) override {
    if (!lacros_running_) {
      return;
    }

    auto it = lacros_web_apps_.find(web_app_id);

    // Do not publish the uninstall to App Service if the app is marked as
    // also installed by the browser, as the real web app provider removes the
    // ARC install source but keeps the app installed.
    if (it != lacros_web_apps_.end() &&
        !base::Contains(browser_installed_apps_, web_app_id)) {
      auto app = std::move(it->second);
      app->readiness = apps::Readiness::kUninstalledByUser;
      lacros_web_apps_.erase(it);
      std::vector<apps::AppPtr> apps;
      apps.push_back(std::move(app));
      PublishToAppService(std::move(apps));
    }

    std::move(callback).Run(webapps::UninstallResultCode::kAppRemoved);
  }

  void MaybeUninstallPackageInArc(const std::string& package_name) override {
    if (!arc_running_) {
      return;
    }

    GetAppHost().OnPackageRemoved(package_name);
  }

 protected:
  // Test ApkWebAppService factory.
  std::unique_ptr<KeyedService> CreateApkWebAppService(
      content::BrowserContext* context) {
    Profile* profile = static_cast<Profile*>(context);
    return std::make_unique<ApkWebAppService>(profile, this);
  }

  ApkWebAppService& GetApkWebAppService() {
    auto* service = ApkWebAppService::Get(browser()->profile());
    DCHECK(service);
    return *service;
  }

  apps::AppRegistryCache& GetAppRegistryCache() {
    auto* proxy =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
    DCHECK(proxy);
    return proxy->AppRegistryCache();
  }

  ArcAppListPrefs& GetArcAppListPrefs() {
    auto* prefs = ArcAppListPrefs::Get(browser()->profile());
    DCHECK(prefs);
    return *prefs;
  }

  arc::mojom::AppHost& GetAppHost() { return GetArcAppListPrefs(); }

  template <typename... PackageT>
  void StartArc(mojo::StructPtr<PackageT>... initial_packages) {
    // Can't use initializer list with unique_ptr since it always copies.
    arc::mojom::ArcPackageInfoPtr array[] = {std::move(initial_packages)...};
    StartArc({std::make_move_iterator(std::begin(array)),
              std::make_move_iterator(std::end(array))});
  }

  void StartArc(std::vector<arc::mojom::ArcPackageInfoPtr> initial_packages) {
    DCHECK(!arc_running_);
    arc_running_ = true;
    // Trigger a package refresh.
    arc::mojom::AppHost* app_host = ArcAppListPrefs::Get(browser()->profile());
    app_host->OnPackageListRefreshed(std::move(initial_packages));
  }

  void StopArc() {
    DCHECK(arc_running_);
    arc_running_ = false;
  }

  void StartLacros(std::vector<apps::AppPtr> initial_apps) {
    DCHECK(!lacros_running_);
    lacros_running_ = true;
    // Publish initial apps.
    PublishToAppService(std::move(initial_apps));
    crosapi::WebAppServiceAsh::Observer& observer = GetApkWebAppService();
    observer.OnWebAppProviderBridgeConnected();
  }

  void StartLacros() { StartLacros(GetLacrosWebApps()); }

  void StopLacros() {
    DCHECK(lacros_running_);
    lacros_running_ = false;
  }

  std::vector<apps::AppPtr> GetLacrosWebApps() {
    std::vector<apps::AppPtr> apps;
    for (const auto& [app_id, app] : lacros_web_apps_) {
      apps.push_back(app->Clone());
    }
    return apps;
  }

  void PublishToAppService(std::vector<apps::AppPtr> apps) {
    auto* proxy =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
    // Emulate what |apps::WebAppsCrosapi| does, need to publish apps for
    // |apps::AppRegistryCache| to get the update.
    proxy->OnApps(std::move(apps), apps::AppType::kWeb,
                  !published_initial_apps_);
    published_initial_apps_ = true;
  }

  // Check that the app is installed in the app registry.
  bool IsWebAppInstalled(const std::string& start_url) {
    bool in_app_service = false;
    GetAppRegistryCache().ForEachApp([&](const apps::AppUpdate& update) {
      if (update.PublisherId() == start_url &&
          update.Readiness() == apps::Readiness::kReady) {
        in_app_service = true;
      }
    });
    return in_app_service;
  }

  void SetAppInstalledInBrowser(const std::string& app_id) {
    browser_installed_apps_.emplace(app_id);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  base::CallbackListSubscription dependency_manager_subscription_;

  bool arc_running_ = false;
  bool lacros_running_ = false;
  bool published_initial_apps_ = false;

  std::map<std::string, apps::AppPtr> lacros_web_apps_;
  std::set<std::string> browser_installed_apps_;
};

IN_PROC_BROWSER_TEST_F(ApkWebAppServiceLacrosBrowserTest, InstallAndUninstall) {
  auto& service = GetApkWebAppService();

  // Start with one web app and one Android app in ARC.
  StartLacros();
  StartArc(GetWebAppPackage("a"), GetArcAppPackage("b"));

  // App "a" installed.
  std::optional<std::string> app_id_a =
      service.GetWebAppIdForPackageName("org.example.a");
  ASSERT_NE(app_id_a, std::nullopt);
  EXPECT_TRUE(service.IsWebOnlyTwa(*app_id_a));
  EXPECT_EQ(service.GetCertificateSha256Fingerprint(*app_id_a), "a-sha1");
  EXPECT_TRUE(IsWebAppInstalled("https://example.org/a?start"));
  // App "b" not installed.
  ASSERT_EQ(service.GetWebAppIdForPackageName("org.example.b"), std::nullopt);

  // Incrementally install a web app in ARC.
  GetAppHost().OnPackageAdded(GetWebAppPackage("c"));

  // App "c" installed.
  std::optional<std::string> app_id_c =
      service.GetWebAppIdForPackageName("org.example.c");
  ASSERT_NE(app_id_c, std::nullopt);
  EXPECT_TRUE(service.IsWebOnlyTwa(*app_id_c));
  EXPECT_EQ(service.GetCertificateSha256Fingerprint(*app_id_c), "c-sha1");
  EXPECT_TRUE(IsWebAppInstalled("https://example.org/c?start"));

  // Incrementally uninstall a web app in ARC.
  GetAppHost().OnPackageRemoved("org.example.a");

  // App "a" uninstalled.
  ASSERT_EQ(service.GetWebAppIdForPackageName("org.example.a"), std::nullopt);
  EXPECT_FALSE(IsWebAppInstalled("https://example.org/a?start"));

  // Uninstall an app by removing it from the initial refresh list.
  StopArc();
  StartArc({});

  // App "c" uninstalled.
  ASSERT_EQ(service.GetWebAppIdForPackageName("org.example.c"), std::nullopt);
  EXPECT_FALSE(IsWebAppInstalled("https://example.org/c?start"));
}

IN_PROC_BROWSER_TEST_F(ApkWebAppServiceLacrosBrowserTest, UpdateAppType) {
  auto& service = GetApkWebAppService();
  auto* shelf_model = ShelfModel::Get();

  // Start with one web app in ARC.
  StartLacros();
  StartArc(GetWebAppPackage("a"));

  // App "a" is installed.
  std::optional<std::string> app_id_a =
      service.GetWebAppIdForPackageName("org.example.a");
  ASSERT_NE(app_id_a, std::nullopt);
  EXPECT_TRUE(IsWebAppInstalled("https://example.org/a?start"));

  // Pin the app to the shelf.
  PinAppWithIDToShelf(*app_id_a);
  EXPECT_TRUE(shelf_model->IsAppPinned(*app_id_a));
  int pin_index = shelf_model->ItemIndexByID(ShelfID(*app_id_a));

  // Replace with Android app.
  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.push_back(arc::mojom::AppInfo::New("Title", "org.example.a",
                                          "org.example.a.activity",
                                          /*sticky=*/true));
  GetAppHost().OnPackageAppListRefreshed("org.example.a", std::move(apps));
  GetAppHost().OnPackageAdded(GetArcAppPackage("a"));

  // App "a" is uninstalled.
  ASSERT_EQ(service.GetWebAppIdForPackageName("org.example.a"), std::nullopt);
  EXPECT_FALSE(IsWebAppInstalled("https://example.org/a?start"));
  // Android app is still pinned.
  auto arc_app_id = GetArcAppListPrefs().GetAppIdByPackageName("org.example.a");
  EXPECT_TRUE(shelf_model->IsAppPinned(arc_app_id));
  EXPECT_EQ(shelf_model->ItemIndexByID(ShelfID(arc_app_id)), pin_index);

  // Move pin to the left, and then reinstall the web app.
  ASSERT_TRUE(shelf_model->Swap(pin_index, /*with_next=*/false));
  pin_index--;
  GetAppHost().OnPackageAdded(GetWebAppPackage("a"));

  // App "a" is installed and has the updated pin index.
  app_id_a = service.GetWebAppIdForPackageName("org.example.a");
  ASSERT_NE(app_id_a, std::nullopt);
  EXPECT_TRUE(IsWebAppInstalled("https://example.org/a?start"));
  EXPECT_TRUE(shelf_model->IsAppPinned(*app_id_a));
  EXPECT_EQ(shelf_model->ItemIndexByID(ShelfID(*app_id_a)), pin_index);
}

IN_PROC_BROWSER_TEST_F(ApkWebAppServiceLacrosBrowserTest,
                       DelayedLacrosInstallUninstall) {
  auto& service = GetApkWebAppService();

  // Start ARC only with one web app.
  StartArc(GetWebAppPackage("a"));

  // App "a" won't be installed because Lacros isn't running.
  EXPECT_FALSE(IsWebAppInstalled("https://example.org/a?start"));
  ASSERT_EQ(service.GetWebAppIdForPackageName("org.example.a"), std::nullopt);

  // Start Lacros, app "a" should now be installed.
  StartLacros();
  EXPECT_TRUE(IsWebAppInstalled("https://example.org/a?start"));
  ASSERT_NE(service.GetWebAppIdForPackageName("org.example.a"), std::nullopt);

  // Stop Lacros and install another app incrementally.
  StopLacros();
  GetAppHost().OnPackageAdded(GetWebAppPackage("b"));

  // App "b" won't be installed because Lacros isn't running.
  EXPECT_FALSE(IsWebAppInstalled("https://example.org/b?start"));
  ASSERT_EQ(service.GetWebAppIdForPackageName("org.example.b"), std::nullopt);

  // Start Lacros, app "b" should now be installed.
  StartLacros();
  EXPECT_TRUE(IsWebAppInstalled("https://example.org/b?start"));
  ASSERT_NE(service.GetWebAppIdForPackageName("org.example.b"), std::nullopt);

  // Stop Lacros and uninstall app "a".
  StopLacros();
  GetAppHost().OnPackageRemoved("org.example.a");

  // App "a" should still be installed because Lacros isn't running.
  EXPECT_TRUE(IsWebAppInstalled("https://example.org/a?start"));
  ASSERT_NE(service.GetWebAppIdForPackageName("org.example.a"), std::nullopt);

  // Start Lacros again, app "a" should now be removed.
  StartLacros();
  EXPECT_FALSE(IsWebAppInstalled("https://example.org/a?start"));
  ASSERT_EQ(service.GetWebAppIdForPackageName("org.example.a"), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(ApkWebAppServiceLacrosBrowserTest,
                       UninstallWebAppThenStartArc) {
  auto& service = GetApkWebAppService();

  // Start with one web app in ARC.
  StartLacros();
  StartArc(GetWebAppPackage("a"));

  // App "a" is installed.
  std::optional<std::string> app_id_a =
      service.GetWebAppIdForPackageName("org.example.a");
  ASSERT_NE(app_id_a, std::nullopt);
  EXPECT_TRUE(IsWebAppInstalled("https://example.org/a?start"));

  // Stop ARC and uninstall app "a" from the browser side.
  StopArc();
  MaybeUninstallWebAppInLacros(*app_id_a, base::DoNothing());

  // Prefs should still be there, but the web app is uninstalled.
  EXPECT_NE(GetArcAppListPrefs().GetPackage("org.example.a"), nullptr);
  ASSERT_NE(service.GetWebAppIdForPackageName("org.example.a"), std::nullopt);
  EXPECT_FALSE(IsWebAppInstalled("https://example.org/a?start"));

  // Restart ARC with the same packages, will trigger ARC app uninstallation.
  StartArc(GetWebAppPackage("a"));
  EXPECT_EQ(GetArcAppListPrefs().GetPackage("org.example.a"), nullptr);
  EXPECT_EQ(service.GetWebAppIdForPackageName("org.example.a"), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(ApkWebAppServiceLacrosBrowserTest,
                       RemoveWebAppWhenArcDisabled) {
  auto& service = GetApkWebAppService();

  StartLacros();
  StartArc(GetWebAppPackage("a"));

  // App "a" is installed.
  std::optional<std::string> app_id_a =
      service.GetWebAppIdForPackageName("org.example.a");
  ASSERT_NE(app_id_a, std::nullopt);
  EXPECT_TRUE(IsWebAppInstalled("https://example.org/a?start"));

  // Disable ARC through settings.
  base::test::TestFuture<const std::string&, const webapps::AppId&>
      uninstalled_future;
  service.SetWebAppUninstalledCallbackForTesting(
      uninstalled_future.GetCallback());
  arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), false);
  StopArc();

  ASSERT_TRUE(uninstalled_future.Wait());

  // Web app should be uninstalled.
  EXPECT_EQ(service.GetWebAppIdForPackageName("org.example.a"), std::nullopt);
  EXPECT_FALSE(IsWebAppInstalled("https://example.org/a?start"));
}

IN_PROC_BROWSER_TEST_F(ApkWebAppServiceLacrosBrowserTest,
                       InstallAndUninstallArcOverUserInstall) {
  auto& service = GetApkWebAppService();

  StartLacros();
  StartArc(GetWebAppPackage("a"));

  // App "a" should be installed.
  std::optional<std::string> app_id_a =
      service.GetWebAppIdForPackageName("org.example.a");
  EXPECT_TRUE(service.IsWebOnlyTwa(*app_id_a));
  EXPECT_TRUE(IsWebAppInstalled("https://example.org/a?start"));

  // Mark the app as also installed by the browser, so that uninstalling the ARC
  // package doesn't remove the app.
  SetAppInstalledInBrowser(*app_id_a);

  // Uninstall the web app from ARC.
  GetAppHost().OnPackageRemoved("org.example.a");

  // Web app should be removed from ApkWebAppService, but is still installed.
  EXPECT_EQ(service.GetWebAppIdForPackageName("org.example.a"), std::nullopt);
  EXPECT_TRUE(IsWebAppInstalled("https://example.org/a?start"));
}

}  // namespace ash
