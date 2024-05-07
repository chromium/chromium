// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_manager.h"

#include <memory>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps.h"
#include "chrome/browser/apps/app_service/webapk/webapk_install_queue.h"
#include "chrome/browser/apps/app_service/webapk/webapk_install_task.h"
#include "chrome/browser/apps/app_service/webapk/webapk_metrics.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAppUrl[] = "https://www.example.com/";
constexpr char kTestAppActionUrl[] = "https://www.example.com/share";
constexpr char kTestManifestUrl[] = "https://www.example.com/manifest.json";
constexpr char kTestShareTextParam[] = "share_text";
constexpr char kTestWebApkPackageName[] = "org.chromium.webapk.some_package";
const std::u16string kTestAppTitle = u"Test App";

std::unique_ptr<web_app::WebAppInstallInfo> BuildDefaultWebAppInfo(
    GURL app_url = GURL(kTestAppUrl)) {
  auto app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  app_info->scope = GURL(kTestAppUrl);
  app_info->title = kTestAppTitle;
  app_info->manifest_url = GURL(kTestManifestUrl);

  apps::ShareTarget target;
  target.action = GURL(kTestAppActionUrl);
  target.method = apps::ShareTarget::Method::kPost;
  target.enctype = apps::ShareTarget::Enctype::kMultipartFormData;
  target.params.text = kTestShareTextParam;
  app_info->share_target = target;

  return app_info;
}

arc::mojom::ArcPackageInfoPtr GetArcPackage(const std::string& package_name) {
  auto package = arc::mojom::ArcPackageInfo::New();
  package->package_name = package_name;
  return package;
}

}  // namespace

class WebApkManagerTest : public apps::AppRegistryCache::Observer,
                          public testing::Test {
 public:
  WebApkManagerTest() = default;

  void SetUp() override {
    testing::Test::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override { arc_test_.TearDown(); }

  void StartWebApkManager() {
    app_service_test_.SetUp(&profile_);
    // This starts the ArcApps publisher, which owns the WebApkManager.
    arc_test_.SetUp(&profile_);
  }

  void AssertNoPendingInstalls() {
    ASSERT_FALSE(webapk_manager()->GetInstallQueueForTest()->PopTaskForTest());
  }

  bool IsAppInstalled(const std::string& app_id) {
    bool installed = false;
    app_service_proxy()->AppRegistryCache().ForOneApp(
        app_id, [&](const apps::AppUpdate& app) {
          installed = app.Readiness() == apps::Readiness::kReady;
        });
    return installed;
  }

  void WaitForAppUninstall(const std::string& app_id) {
    app_registry_cache_observer_.Observe(
        &(app_service_proxy()->AppRegistryCache()));
    app_id_ = app_id;
    base::RunLoop run_loop;
    quit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const apps::AppUpdate& update) override {
    if (app_id_.empty()) {
      return;
    }

    if (app_id_ == update.AppId() &&
        update.Readiness() == apps::Readiness::kUninstalledByUser &&
        !quit_callback_.is_null()) {
      std::move(quit_callback_).Run();
      app_registry_cache_observer_.Reset();
    }
  }

  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    app_registry_cache_observer_.Reset();
  }

  TestingProfile* profile() { return &profile_; }
  apps::AppServiceTest* app_service_test() { return &app_service_test_; }
  apps::WebApkManager* webapk_manager() {
    return apps::ArcApps::Get(profile())->GetWebApkManagerForTesting();
  }
  ArcAppTest* arc_test() { return &arc_test_; }
  apps::AppServiceProxyBase* app_service_proxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  ArcAppTest arc_test_;
  apps::AppServiceTest app_service_test_;
  std::string app_id_;
  base::OnceClosure quit_callback_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

TEST_F(WebApkManagerTest, InstallsWebApkOnStartup) {
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  StartWebApkManager();

  auto install_task =
      webapk_manager()->GetInstallQueueForTest()->PopTaskForTest();
  ASSERT_TRUE(install_task);
  ASSERT_EQ(install_task->app_id(), app_id);
  AssertNoPendingInstalls();
}

TEST_F(WebApkManagerTest, InstallWebApkAfterStartup) {
  StartWebApkManager();
  AssertNoPendingInstalls();

  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  auto install_task =
      webapk_manager()->GetInstallQueueForTest()->PopTaskForTest();
  ASSERT_TRUE(install_task);
  ASSERT_EQ(install_task->app_id(), app_id);
  AssertNoPendingInstalls();
}

// Does not install web apps without a Share Target definition.
TEST_F(WebApkManagerTest, NoShareTarget) {
  auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL(kTestAppUrl));
  app_info->title = kTestAppTitle;
  auto app_id = web_app::test::InstallWebApp(profile(), std::move(app_info));

  StartWebApkManager();

  AssertNoPendingInstalls();
}

// When two eligible apps are available during startup, but one of them already
// has a WebAPK installed, only install a new WebAPK for the other app.
TEST_F(WebApkManagerTest, IgnoresAlreadyInstalledWebApkOnStartup) {
  auto app_info_1 = BuildDefaultWebAppInfo();
  // Change the manifest_id so that the two apps have different IDs.
  auto app_info_2 =
      BuildDefaultWebAppInfo(GURL(base::StrCat({kTestAppUrl, "/app_2"})));

  auto app_id_1 =
      web_app::test::InstallWebApp(profile(), std::move(app_info_1));
  auto app_id_2 =
      web_app::test::InstallWebApp(profile(), std::move(app_info_2));
  apps::webapk_prefs::AddWebApk(profile(), app_id_1,
                                "org.chromium.webapk.some_package");

  StartWebApkManager();

  auto install_task =
      webapk_manager()->GetInstallQueueForTest()->PopTaskForTest();
  ASSERT_TRUE(install_task);
  ASSERT_EQ(install_task->app_id(), app_id_2);
  AssertNoPendingInstalls();
}

TEST_F(WebApkManagerTest, RemovesIneligibleWebApkOnStartup) {
  auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL(kTestAppUrl));
  app_info->title = kTestAppTitle;
  auto app_id = web_app::test::InstallWebApp(profile(), std::move(app_info));

  // Add the app to prefs manually, as if this app was previously installed as a
  // webapk.
  apps::webapk_prefs::AddWebApk(profile(), app_id, kTestWebApkPackageName);

  StartWebApkManager();
  arc_test()->app_instance()->SendRefreshPackageList({});

  // The WebAPK should have been uninstalled, but the app itself is still
  // installed.
  ASSERT_FALSE(apps::webapk_prefs::GetWebApkPackageName(profile(), app_id));
  ASSERT_TRUE(IsAppInstalled(app_id));
}

TEST_F(WebApkManagerTest, RemovesUninstalledAppOnStartup) {
  std::string app_id = "foobar";
  apps::webapk_prefs::AddWebApk(profile(), app_id, kTestWebApkPackageName);
  StartWebApkManager();
  arc_test()->app_instance()->SendRefreshPackageList({});
  ASSERT_FALSE(apps::webapk_prefs::GetWebApkPackageName(profile(), app_id));
}

TEST_F(WebApkManagerTest, RemovesAppUninstalledFromChrome) {
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  apps::webapk_prefs::AddWebApk(profile(), app_id, kTestWebApkPackageName);
  StartWebApkManager();
  arc_test()->app_instance()->SendRefreshPackageList({});

  app_service_proxy()->UninstallSilently(app_id,
                                         apps::UninstallSource::kUnknown);

  // Wait for the async web app uninstallation.
  WaitForAppUninstall(app_id);

  ASSERT_FALSE(apps::webapk_prefs::GetWebApkPackageName(profile(), app_id));
}

TEST_F(WebApkManagerTest, QueuesUpdatedApp) {
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  apps::webapk_prefs::AddWebApk(profile(), app_id, kTestWebApkPackageName);

  StartWebApkManager();

  // Mimic updating an app by reinstalling it with a different WebAppInfo.
  auto updated_app_info = BuildDefaultWebAppInfo();
  updated_app_info->title = u"Some new title";
  auto updated_app_id =
      web_app::test::InstallWebApp(profile(), std::move(updated_app_info),
                                   /*overwrite_existing_manifest_fields=*/true);
  EXPECT_EQ(app_id, updated_app_id);

  auto install_task =
      webapk_manager()->GetInstallQueueForTest()->PopTaskForTest();
  ASSERT_TRUE(install_task);
  ASSERT_EQ(install_task->app_id(), app_id);
  AssertNoPendingInstalls();

  base::flat_set<std::string> updating_apps =
      apps::webapk_prefs::GetUpdateNeededAppIds(profile());
  ASSERT_THAT(updating_apps, testing::ElementsAre(app_id));
}

TEST_F(WebApkManagerTest, QueuesPendingUpdateOnStartup) {
  auto app_info_1 = BuildDefaultWebAppInfo();
  // Change the manifest_id so that the two apps have different IDs.
  auto app_info_2 =
      BuildDefaultWebAppInfo(GURL(base::StrCat({kTestAppUrl, "/app_2"})));

  auto app_id_1 =
      web_app::test::InstallWebApp(profile(), std::move(app_info_1));
  apps::webapk_prefs::AddWebApk(profile(), app_id_1, kTestWebApkPackageName);
  apps::webapk_prefs::SetUpdateNeededForApp(profile(), app_id_1,
                                            /* update_needed= */ true);
  auto app_id_2 =
      web_app::test::InstallWebApp(profile(), std::move(app_info_2));
  apps::webapk_prefs::AddWebApk(profile(), app_id_2, kTestWebApkPackageName);
  apps::webapk_prefs::SetUpdateNeededForApp(profile(), app_id_2,
                                            /* update_needed= */ false);

  StartWebApkManager();

  // App 1 has a pending update, app 2 does not. Only app 1 should be queued.
  auto install_task =
      webapk_manager()->GetInstallQueueForTest()->PopTaskForTest();
  ASSERT_TRUE(install_task);
  ASSERT_EQ(install_task->app_id(), app_id_1);
  AssertNoPendingInstalls();
}

TEST_F(WebApkManagerTest, IgnoresInstallsWhilePlayStoreDisabled) {
  StartWebApkManager();

  arc::SetArcPlayStoreEnabledForProfile(profile(), /*enabled=*/false);

  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  AssertNoPendingInstalls();
}

TEST_F(WebApkManagerTest, IgnoresInstallsWhilePolicyDisabled) {
  StartWebApkManager();
  profile()->GetPrefs()->SetBoolean(
      apps::webapk_prefs::kGeneratedWebApksEnabled, false);

  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  AssertNoPendingInstalls();
}

TEST_F(WebApkManagerTest, RemovesWebApksWhenPolicyDisabled) {
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  apps::webapk_prefs::AddWebApk(profile(), app_id, kTestWebApkPackageName);

  StartWebApkManager();
  arc_test()->app_instance()->SendRefreshPackageList({});

  profile()->GetPrefs()->SetBoolean(
      apps::webapk_prefs::kGeneratedWebApksEnabled, false);

  ASSERT_THAT(apps::webapk_prefs::GetWebApkAppIds(profile()),
              testing::IsEmpty());
}

TEST_F(WebApkManagerTest, RemovesUntrackedInstalledWebApk) {
  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.push_back(GetArcPackage("org.chromium.webapk.package1"));
  packages.push_back(GetArcPackage("org.chromium.webapk.package2"));

  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  apps::webapk_prefs::AddWebApk(profile(), app_id,
                                "org.chromium.webapk.package1");
  StartWebApkManager();

  arc_test()->app_instance()->SendRefreshPackageList(std::move(packages));

  ASSERT_TRUE(ArcAppListPrefs::Get(profile())->GetPackage(
      "org.chromium.webapk.package1"));
  ASSERT_FALSE(ArcAppListPrefs::Get(profile())->GetPackage(
      "org.chromium.webapk.package2"));
}
