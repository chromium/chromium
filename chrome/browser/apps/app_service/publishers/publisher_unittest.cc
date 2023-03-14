// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_apps.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/externally_managed_app_manager_impl.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps_factory.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/fake_browser_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/capability_access_update.h"
#include "components/user_manager/scoped_user_manager.h"

using ash::standalone_browser::BrowserSupport;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

const base::Time kLastLaunchTime = base::Time::Now();
const base::Time kInstallTime = base::Time::Now();
const char kUrl[] = "https://example.com/";

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
scoped_refptr<extensions::Extension> MakeExtensionApp(
    const std::string& name,
    const std::string& version,
    const std::string& url,
    const std::string& id) {
  std::string err;
  base::Value::Dict value;
  value.Set("name", name);
  value.Set("version", version);
  base::Value::List scripts;
  scripts.Append("script.js");
  value.SetByDottedPath("app.background.scripts", std::move(scripts));
  scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kInternal, value,
      extensions::Extension::WAS_INSTALLED_BY_DEFAULT, id, &err);
  EXPECT_EQ(err, "");
  return app;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kLegacyPackagedAppId[] = "mblemkccghnfkjignlmgngmopopifacf";

scoped_refptr<extensions::Extension> MakeLegacyPackagedApp(
    const std::string& name,
    const std::string& version,
    const std::string& url,
    const std::string& id) {
  std::string err;
  base::Value::Dict value;
  value.Set("name", name);
  value.Set("version", version);
  value.SetByDottedPath("app.launch.local_path", "index.html");
  scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kInternal, value,
      extensions::Extension::WAS_INSTALLED_BY_DEFAULT, id, &err);
  EXPECT_EQ(err, "");
  return app;
}

void AddArcPackage(ArcAppTest& arc_test,
                   const std::vector<arc::mojom::AppInfoPtr>& fake_apps) {
  for (const auto& fake_app : fake_apps) {
    base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
        permissions;
    permissions.emplace(arc::mojom::AppPermission::CAMERA,
                        arc::mojom::PermissionState::New(/*granted=*/false,
                                                         /*managed=*/false));
    permissions.emplace(arc::mojom::AppPermission::LOCATION,
                        arc::mojom::PermissionState::New(/*granted=*/true,
                                                         /*managed=*/false));
    arc::mojom::ArcPackageInfoPtr package = arc::mojom::ArcPackageInfo::New(
        fake_app->package_name, /*package_version=*/1,
        /*last_backup_android_id=*/1,
        /*last_backup_time=*/1, /*sync=*/true, /*system=*/false,
        /*vpn_provider=*/false, /*web_app_info=*/nullptr, absl::nullopt,
        std::move(permissions));
    arc_test.AddPackage(package->Clone());
    arc_test.app_instance()->SendPackageAdded(package->Clone());
  }
}

apps::AppPtr MakeApp(apps::AppType app_type,
                     const std::string& app_id,
                     const std::string& name,
                     apps::Readiness readiness) {
  auto app = std::make_unique<apps::App>(app_type, app_id);
  app->readiness = readiness;
  app->name = name;
  app->short_name = name;
  app->install_reason = apps::InstallReason::kUser;
  app->install_source = apps::InstallSource::kSync;
  app->icon_key = apps::IconKey(
      /*timeline=*/1, apps::IconKey::kInvalidResourceId,
      /*icon_effects=*/0);
  return app;
}

apps::Permissions MakeFakePermissions() {
  apps::Permissions permissions;
  permissions.push_back(std::make_unique<apps::Permission>(
      apps::PermissionType::kCamera,
      std::make_unique<apps::PermissionValue>(false),
      /*is_managed*/ false));
  permissions.push_back(std::make_unique<apps::Permission>(
      apps::PermissionType::kLocation,
      std::make_unique<apps::PermissionValue>(true),
      /*is_managed*/ false));
  return permissions;
}

apps::CapabilityAccessPtr MakeCapabilityAccess(
    const std::string& app_id,
    absl::optional<bool> camera,
    absl::optional<bool> microphone) {
  apps::CapabilityAccessPtr access =
      std::make_unique<apps::CapabilityAccess>(app_id);
  access->camera = std::move(camera);
  access->microphone = std::move(microphone);
  return access;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

apps::IntentFilters CreateIntentFilters() {
  const GURL url(kUrl);
  apps::IntentFilters filters;
  apps::IntentFilterPtr filter = std::make_unique<apps::IntentFilter>();

  apps::ConditionValues values1;
  values1.push_back(std::make_unique<apps::ConditionValue>(
      apps_util::kIntentActionView, apps::PatternMatchType::kLiteral));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kAction, std::move(values1)));

  apps::ConditionValues values2;
  values2.push_back(std::make_unique<apps::ConditionValue>(
      url.scheme(), apps::PatternMatchType::kLiteral));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kScheme, std::move(values2)));

  apps::ConditionValues values3;
  values3.push_back(std::make_unique<apps::ConditionValue>(
      url.host(), apps::PatternMatchType::kLiteral));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kHost, std::move(values3)));

  apps::ConditionValues values4;
  values4.push_back(std::make_unique<apps::ConditionValue>(
      url.path(), apps::PatternMatchType::kPrefix));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kPath, std::move(values4)));

  filters.push_back(std::move(filter));

  return filters;
}

MATCHER(Ready, "App has readiness=\"kReady\"") {
  return arg.readiness == apps::Readiness::kReady;
}

MATCHER_P(ShownInShelf, shown, "App shown on the shelf") {
  return arg.show_in_shelf.has_value() && arg.show_in_shelf == shown;
}

MATCHER_P(ShownInLauncher, shown, "App shown in the launcher") {
  return arg.show_in_launcher.has_value() && arg.show_in_launcher == shown;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
arc::mojom::PrivacyItemPtr CreateArcPrivacyItem(
    arc::mojom::AppPermissionGroup permission,
    const std::string& package_name) {
  arc::mojom::PrivacyItemPtr item = arc::mojom::PrivacyItem::New();
  item->permission_group = permission;
  item->privacy_application = arc::mojom::PrivacyApplication::New();
  item->privacy_application->package_name = package_name;
  return item;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// AppRegistryCacheObserver is used to test the OnAppTypeInitialized and
// OnAppUpdate interfaces for AppRegistryCache::Observer.
class AppRegistryCacheObserver : public apps::AppRegistryCache::Observer {
 public:
  explicit AppRegistryCacheObserver(apps::AppRegistryCache* cache) {
    cache_ = cache;
    Observe(cache);
  }

  ~AppRegistryCacheObserver() override = default;

  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const apps::AppUpdate& update) override {
    updated_ids_.push_back(update.AppId());
  }

  void OnAppTypeInitialized(apps::AppType app_type) override {
    app_types_.push_back(app_type);
  }

  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    Observe(nullptr);
  }

  std::vector<std::string> updated_ids() const { return updated_ids_; }
  std::vector<apps::AppType> app_types() const { return app_types_; }

 private:
  std::vector<std::string> updated_ids_;
  std::vector<apps::AppType> app_types_;
  raw_ptr<apps::AppRegistryCache> cache_ = nullptr;
};

}  // namespace

namespace apps {

class PublisherTest : public extensions::ExtensionServiceTestBase {
 public:
  PublisherTest() = default;
  PublisherTest(const PublisherTest&) = delete;
  PublisherTest& operator=(const PublisherTest&) = delete;

  ~PublisherTest() override = default;

  // ExtensionServiceTestBase:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams());
    service_->Init();
    ConfigureWebAppProvider();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    browser_manager_ = std::make_unique<crosapi::FakeBrowserManager>();
    ash::LoginState::Initialize();
#endif
  }

  void TearDown() override {
    extensions::ExtensionServiceTestBase::TearDown();
    profile_.reset();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::LoginState::Shutdown();
    browser_manager_.reset();
#endif
  }

  void ConfigureWebAppProvider() {
    auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
    url_loader_ = url_loader.get();

    auto externally_managed_app_manager =
        std::make_unique<web_app::ExternallyManagedAppManagerImpl>(profile());
    externally_managed_app_manager->SetUrlLoaderForTesting(
        std::move(url_loader));

    auto* const provider = web_app::FakeWebAppProvider::Get(profile());
    provider->SetExternallyManagedAppManager(
        std::move(externally_managed_app_manager));
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    base::RunLoop().RunUntilIdle();
  }

  std::string CreateWebApp(const std::string& app_name) {
    const GURL kAppUrl(kUrl);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->title = base::UTF8ToUTF16(app_name);
    web_app_info->start_url = kAppUrl;
    web_app_info->scope = kAppUrl;
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;

    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void RemoveArcApp(const std::string& app_id) {
    ArcApps* arc_apps = ArcAppsFactory::GetForProfile(profile());
    ASSERT_TRUE(arc_apps);
    arc_apps->OnAppRemoved(app_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void VerifyOptionalBool(absl::optional<bool> source,
                          absl::optional<bool> target) {
    if (source.has_value()) {
      EXPECT_EQ(source, target);
    }
  }

  const AppPtr& GetApp(const std::string& app_id) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
    return cache.states_[app_id];
  }

  void VerifyNoApp(const std::string& app_id) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();

    ASSERT_EQ(cache.states_.end(), cache.states_.find(app_id));
  }

  void VerifyApp(AppType app_type,
                 const std::string& app_id,
                 const std::string& name,
                 apps::Readiness readiness,
                 InstallReason install_reason,
                 InstallSource install_source,
                 const std::vector<std::string>& additional_search_terms,
                 base::Time last_launch_time,
                 base::Time install_time,
                 const apps::Permissions& permissions,
                 absl::optional<bool> is_platform_app = absl::nullopt,
                 absl::optional<bool> recommendable = absl::nullopt,
                 absl::optional<bool> searchable = absl::nullopt,
                 absl::optional<bool> show_in_launcher = absl::nullopt,
                 absl::optional<bool> show_in_shelf = absl::nullopt,
                 absl::optional<bool> show_in_search = absl::nullopt,
                 absl::optional<bool> show_in_management = absl::nullopt,
                 absl::optional<bool> handles_intents = absl::nullopt,
                 absl::optional<bool> allow_uninstall = absl::nullopt,
                 absl::optional<bool> has_badge = absl::nullopt,
                 absl::optional<bool> paused = absl::nullopt,
                 WindowMode window_mode = WindowMode::kUnknown) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();

    ASSERT_NE(cache.states_.end(), cache.states_.find(app_id));
    EXPECT_EQ(app_type, cache.states_[app_id]->app_type);
    ASSERT_TRUE(cache.states_[app_id]->name.has_value());
    EXPECT_EQ(name, cache.states_[app_id]->name.value());
    EXPECT_EQ(readiness, cache.states_[app_id]->readiness);
    ASSERT_TRUE(cache.states_[app_id]->icon_key.has_value());
    EXPECT_EQ(install_reason, cache.states_[app_id]->install_reason);
    EXPECT_EQ(install_source, cache.states_[app_id]->install_source);
    EXPECT_EQ(additional_search_terms,
              cache.states_[app_id]->additional_search_terms);
    if (!last_launch_time.is_null()) {
      EXPECT_EQ(last_launch_time, cache.states_[app_id]->last_launch_time);
    }
    if (!install_time.is_null()) {
      EXPECT_EQ(install_time, cache.states_[app_id]->install_time);
    }
    if (!permissions.empty()) {
      EXPECT_TRUE(IsEqual(permissions, cache.states_[app_id]->permissions));
    }
    VerifyOptionalBool(is_platform_app, cache.states_[app_id]->is_platform_app);
    VerifyOptionalBool(recommendable, cache.states_[app_id]->recommendable);
    VerifyOptionalBool(searchable, cache.states_[app_id]->searchable);
    VerifyOptionalBool(show_in_launcher,
                       cache.states_[app_id]->show_in_launcher);
    VerifyOptionalBool(show_in_shelf, cache.states_[app_id]->show_in_shelf);
    VerifyOptionalBool(show_in_search, cache.states_[app_id]->show_in_search);
    VerifyOptionalBool(show_in_management,
                       cache.states_[app_id]->show_in_management);
    VerifyOptionalBool(handles_intents, cache.states_[app_id]->handles_intents);
    VerifyOptionalBool(allow_uninstall, cache.states_[app_id]->allow_uninstall);
    VerifyOptionalBool(has_badge, cache.states_[app_id]->has_badge);
    VerifyOptionalBool(paused, cache.states_[app_id]->paused);
    if (window_mode != WindowMode::kUnknown) {
      EXPECT_EQ(window_mode, cache.states_[app_id]->window_mode);
    }
  }

  void VerifyAppIsRemoved(const std::string& app_id) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
    ASSERT_NE(cache.states_.end(), cache.states_.find(app_id));
    EXPECT_EQ(apps::Readiness::kUninstalledByUser,
              cache.states_[app_id]->readiness);
  }

  void VerifyIntentFilters(const std::string& app_id) {
    apps::IntentFilters source = CreateIntentFilters();

    apps::IntentFilters target;
    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->AppRegistryCache()
        .ForOneApp(app_id, [&target](const apps::AppUpdate& update) {
          target = update.IntentFilters();
        });

    EXPECT_EQ(source.size(), target.size());
    for (int i = 0; i < static_cast<int>(source.size()); i++) {
      EXPECT_EQ(*source[i], *target[i]);
    }
  }

  void VerifyAppTypeIsInitialized(AppType app_type) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
    ASSERT_TRUE(cache.IsAppTypeInitialized(app_type));
    ASSERT_TRUE(base::Contains(cache.InitializedAppTypes(), app_type));
  }

  void VerifyCapabilityAccess(const std::string& app_id,
                              absl::optional<bool> accessing_camera,
                              absl::optional<bool> accessing_microphone) {
    absl::optional<bool> camera;
    absl::optional<bool> microphone;
    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->AppCapabilityAccessCache()
        .ForOneApp(app_id, [&camera, &microphone](
                               const apps::CapabilityAccessUpdate& update) {
          camera = update.Camera();
          microphone = update.Microphone();
        });
    EXPECT_EQ(camera, accessing_camera);
    EXPECT_EQ(microphone, accessing_microphone);
  }

  void VerifyNoCapabilityAccess(const std::string& app_id) {
    ASSERT_FALSE(
        apps::AppServiceProxyFactory::GetForProfile(profile())
            ->AppCapabilityAccessCache()
            .ForOneApp(app_id, [](const apps::CapabilityAccessUpdate& update) {
              NOTREACHED();
            }));
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  PromiseAppPtr& GetPromiseApp(const PackageId& package_id) {
    PromiseAppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())
            ->PromiseAppRegistryCache();
    return cache.promise_app_map_.find(package_id)->second;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  raw_ptr<web_app::TestWebAppUrlLoader> url_loader_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<crosapi::FakeBrowserManager> browser_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PublisherTest, ArcAppsOnApps) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  // Install fake apps.
  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());
  AddArcPackage(arc_test, arc_test.fake_apps());

  // Verify ARC apps are added to AppRegistryCache.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile());
  ASSERT_TRUE(prefs);
  for (const auto& app_id : prefs->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      VerifyApp(
          AppType::kArc, app_id, app_info->name, Readiness::kReady,
          app_info->sticky ? InstallReason::kSystem : InstallReason::kUser,
          app_info->sticky ? InstallSource::kSystem : InstallSource::kPlayStore,
          {}, app_info->last_launch_time, app_info->install_time,
          apps::Permissions(), /*is_platform_app=*/false,
          /*recommendable=*/true, /*searchable=*/true,
          /*show_in_launcher=*/true, /*show_in_shelf=*/true,
          /*show_in_search=*/true, /*show_in_management=*/true,
          /*handles_intents=*/true,
          /*allow_uninstall=*/app_info->ready && !app_info->sticky,
          /*has_badge=*/false, /*paused=*/false);
      // Simulate the app is removed.
      RemoveArcApp(app_id);
      VerifyAppIsRemoved(app_id);
    }
  }
  VerifyAppTypeIsInitialized(AppType::kArc);

  // Verify the initialization process again with a new ArcApps object.
  std::unique_ptr<ArcApps> arc_apps = std::make_unique<ArcApps>(
      AppServiceProxyFactory::GetForProfile(profile()));
  ASSERT_TRUE(arc_apps.get());
  arc_apps->Initialize();

  for (const auto& app_id : prefs->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      VerifyApp(
          AppType::kArc, app_id, app_info->name, Readiness::kReady,
          app_info->sticky ? InstallReason::kSystem : InstallReason::kUser,
          app_info->sticky ? InstallSource::kSystem : InstallSource::kPlayStore,
          {}, app_info->last_launch_time, app_info->install_time,
          MakeFakePermissions(), /*is_platform_app=*/false,
          /*recommendable=*/true, /*searchable=*/true,
          /*show_in_launcher=*/true, /*show_in_shelf=*/true,
          /*show_in_search=*/true, /*show_in_management=*/true,
          /*handles_intents=*/true,
          /*allow_uninstall=*/app_info->ready && !app_info->sticky,
          /*has_badge=*/false, /*paused=*/false);

      // Test OnAppLastLaunchTimeUpdated.
      const base::Time before_time = base::Time::Now();
      prefs->SetLastLaunchTime(app_id);
      app_info = prefs->GetApp(app_id);
      EXPECT_GE(app_info->last_launch_time, before_time);
      VerifyApp(
          AppType::kArc, app_id, app_info->name, Readiness::kReady,
          app_info->sticky ? InstallReason::kSystem : InstallReason::kUser,
          app_info->sticky ? InstallSource::kSystem : InstallSource::kPlayStore,
          {}, app_info->last_launch_time, app_info->install_time,
          MakeFakePermissions());
    }
  }

  arc_apps->Shutdown();
}

TEST_F(PublisherTest, ArcApps_CapabilityAccess) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());
  ArcApps* arc_apps = apps::ArcAppsFactory::GetForProfile(profile());
  ASSERT_TRUE(arc_apps);

  const auto& fake_apps = arc_test.fake_apps();
  std::string package_name1 = fake_apps[0]->package_name;
  std::string package_name2 = fake_apps[1]->package_name;

  // Install fake apps.
  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());

  // Set accessing Camera for `package_name1`.
  {
    std::vector<arc::mojom::PrivacyItemPtr> privacy_items;
    privacy_items.push_back(CreateArcPrivacyItem(
        arc::mojom::AppPermissionGroup::CAMERA, package_name1));
    arc_apps->OnPrivacyItemsChanged(std::move(privacy_items));
    VerifyCapabilityAccess(ArcAppTest::GetAppId(*fake_apps[0]),
                           /*accessing_camera=*/true,
                           /*accessing_microphone=*/absl::nullopt);
  }

  // Cancel accessing Camera for `package_name1`.
  {
    std::vector<arc::mojom::PrivacyItemPtr> privacy_items;
    arc_apps->OnPrivacyItemsChanged(std::move(privacy_items));
    VerifyCapabilityAccess(ArcAppTest::GetAppId(*fake_apps[0]),
                           /*accessing_camera=*/false,
                           /*accessing_microphone=*/false);
  }

  // Set accessing Camera and Microphone for `package_name1`, and accessing
  // Camera for `package_name2`.
  {
    std::vector<arc::mojom::PrivacyItemPtr> privacy_items;
    privacy_items.push_back(CreateArcPrivacyItem(
        arc::mojom::AppPermissionGroup::CAMERA, package_name1));
    privacy_items.push_back(CreateArcPrivacyItem(
        arc::mojom::AppPermissionGroup::MICROPHONE, package_name1));
    privacy_items.push_back(CreateArcPrivacyItem(
        arc::mojom::AppPermissionGroup::CAMERA, package_name2));
    arc_apps->OnPrivacyItemsChanged(std::move(privacy_items));
    VerifyCapabilityAccess(ArcAppTest::GetAppId(*fake_apps[0]),
                           /*accessing_camera=*/true,
                           /*accessing_microphone=*/true);
    VerifyCapabilityAccess(ArcAppTest::GetAppId(*fake_apps[1]),
                           /*accessing_camera=*/true,
                           /*accessing_microphone=*/absl::nullopt);
  }

  // Cancel accessing Microphone for `package_name1`.
  {
    std::vector<arc::mojom::PrivacyItemPtr> privacy_items;
    privacy_items.push_back(CreateArcPrivacyItem(
        arc::mojom::AppPermissionGroup::CAMERA, package_name1));
    privacy_items.push_back(CreateArcPrivacyItem(
        arc::mojom::AppPermissionGroup::CAMERA, package_name2));
    arc_apps->OnPrivacyItemsChanged(std::move(privacy_items));
    VerifyCapabilityAccess(ArcAppTest::GetAppId(*fake_apps[0]),
                           /*accessing_camera=*/true,
                           /*accessing_microphone=*/false);
    VerifyCapabilityAccess(ArcAppTest::GetAppId(*fake_apps[1]),
                           /*accessing_camera=*/true,
                           /*accessing_microphone=*/false);
  }

  // Cancel accessing CAMERA for `package_name1` and `package_name2`.
  {
    std::vector<arc::mojom::PrivacyItemPtr> privacy_items;
    arc_apps->OnPrivacyItemsChanged(std::move(privacy_items));
    VerifyCapabilityAccess(ArcAppTest::GetAppId(*fake_apps[0]),
                           /*accessing_camera=*/false,
                           /*accessing_microphone=*/false);
    VerifyCapabilityAccess(ArcAppTest::GetAppId(*fake_apps[1]),
                           /*accessing_camera=*/false,
                           /*accessing_microphone=*/false);
  }

  arc_apps->Shutdown();
}

TEST_F(PublisherTest, BuiltinAppsOnApps) {
  // Verify Builtin apps are added to AppRegistryCache.
  for (const auto& internal_app : app_list::GetInternalAppList(profile())) {
    if ((internal_app.app_id == nullptr) ||
        (internal_app.name_string_resource_id == 0) ||
        (internal_app.icon_resource_id <= 0)) {
      continue;
    }
    std::vector<std::string> additional_search_terms;
    if (internal_app.searchable_string_resource_id != 0) {
      additional_search_terms.push_back(
          l10n_util::GetStringUTF8(internal_app.searchable_string_resource_id));
    }
    VerifyApp(AppType::kBuiltIn, internal_app.app_id,
              l10n_util::GetStringUTF8(internal_app.name_string_resource_id),
              Readiness::kReady, InstallReason::kSystem, InstallSource::kSystem,
              additional_search_terms, base::Time(), base::Time(),
              apps::Permissions(), /*is_platform_app=*/false,
              internal_app.recommendable, internal_app.searchable,
              internal_app.show_in_launcher, internal_app.searchable,
              internal_app.searchable, /*show_in_management=*/false,
              internal_app.show_in_launcher, /*allow_uninstall=*/false);
  }
  VerifyAppTypeIsInitialized(AppType::kBuiltIn);
}

class LegacyPackagedAppLacrosNotPrimaryPublisherTest : public PublisherTest {
 public:
  LegacyPackagedAppLacrosNotPrimaryPublisherTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(ash::features::kLacrosPrimary);
  }

  LegacyPackagedAppLacrosNotPrimaryPublisherTest(
      const LegacyPackagedAppLacrosNotPrimaryPublisherTest&) = delete;
  LegacyPackagedAppLacrosNotPrimaryPublisherTest& operator=(
      const LegacyPackagedAppLacrosNotPrimaryPublisherTest&) = delete;
  ~LegacyPackagedAppLacrosNotPrimaryPublisherTest() override = default;

 private:
  const base::AutoReset<bool> resetter_ =
      BrowserSupport::SetLacrosEnabledForTest(true);
};

TEST_F(LegacyPackagedAppLacrosNotPrimaryPublisherTest,
       LegacyPackagedAppsOnApps) {
  ASSERT_FALSE(crosapi::browser_util::IsLacrosPrimaryBrowser());

  // Re-init AppService to verify the init process.
  AppServiceTest app_service_test;
  app_service_test.SetUp(profile());

  // Install a legacy packaged app.
  scoped_refptr<extensions::Extension> legacy_app =
      MakeLegacyPackagedApp("legacy_app", "0.0", "http://google.com",
                            std::string(kLegacyPackagedAppId));
  ASSERT_TRUE(legacy_app->is_legacy_packaged_app());

  service_->AddExtension(legacy_app.get());

  // Verify the legacy packaged app is published.
  VerifyApp(AppType::kChromeApp, legacy_app->id(), legacy_app->name(),
            Readiness::kReady, InstallReason::kDefault,
            InstallSource::kChromeWebStore, {}, base::Time(), base::Time(),
            apps::Permissions(),
            /*is_platform_app=*/false, /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*has_badge=*/false, /*paused=*/false);
  VerifyAppTypeIsInitialized(AppType::kChromeApp);
}

class LegacyPackagedAppLacrosPrimaryPublisherTest : public PublisherTest {
 public:
  LegacyPackagedAppLacrosPrimaryPublisherTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(ash::features::kLacrosPrimary);
  }

  LegacyPackagedAppLacrosPrimaryPublisherTest(
      const LegacyPackagedAppLacrosNotPrimaryPublisherTest&) = delete;
  LegacyPackagedAppLacrosPrimaryPublisherTest& operator=(
      const LegacyPackagedAppLacrosNotPrimaryPublisherTest&) = delete;
  ~LegacyPackagedAppLacrosPrimaryPublisherTest() override = default;

 private:
  base::AutoReset<bool> set_lacros_enabled_ =
      BrowserSupport::SetLacrosEnabledForTest(true);
};

TEST_F(LegacyPackagedAppLacrosPrimaryPublisherTest, LegacyPackagedAppsOnApps) {
  ASSERT_TRUE(crosapi::browser_util::IsLacrosPrimaryBrowser());

  // Re-init AppService to verify the init process.
  AppServiceTest app_service_test;
  app_service_test.SetUp(profile());

  // Install a legacy packaged app.
  scoped_refptr<extensions::Extension> legacy_app =
      MakeLegacyPackagedApp("legacy_app", "0.0", "http://google.com",
                            std::string(kLegacyPackagedAppId));
  ASSERT_TRUE(legacy_app->is_legacy_packaged_app());

  service_->AddExtension(legacy_app.get());

  // Verify the legacy packaged app is not published.
  VerifyNoApp(legacy_app->id());
}

class StandaloneBrowserPublisherTest : public PublisherTest {
 public:
  StandaloneBrowserPublisherTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {features::kWebAppsCrosapi, ash::features::kLacrosPrimary}, {});
  }

  StandaloneBrowserPublisherTest(const StandaloneBrowserPublisherTest&) =
      delete;
  StandaloneBrowserPublisherTest& operator=(
      const StandaloneBrowserPublisherTest&) = delete;
  ~StandaloneBrowserPublisherTest() override = default;

  // PublisherTest:
  void SetUp() override {
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* fake_user_manager = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    // Login a user. The "email" must match the TestingProfile's
    // GetProfileUserName() so that profile() will be the primary profile.
    const AccountId account_id = AccountId::FromUserEmail("testing_profile");
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);

    PublisherTest::SetUp();
  }

  void ExtensionAppsOnApps() {
    mojo::PendingReceiver<crosapi::mojom::AppController> pending_receiver;
    mojo::PendingRemote<crosapi::mojom::AppController> pending_remote =
        pending_receiver.InitWithNewPipeAndPassRemote();
    StandaloneBrowserExtensionApps* chrome_apps =
        StandaloneBrowserExtensionAppsFactoryForApp::GetForProfile(profile());
    chrome_apps->RegisterAppController(std::move(pending_remote));
    std::vector<AppPtr> apps;
    auto app = MakeApp(AppType::kStandaloneBrowserChromeApp,
                       /*app_id=*/"a",
                       /*name=*/"TestApp", Readiness::kReady);
    app->is_platform_app = true;
    app->recommendable = false;
    app->searchable = false;
    app->show_in_launcher = false;
    app->show_in_shelf = false;
    app->show_in_search = false;
    app->show_in_management = false;
    app->handles_intents = false;
    app->allow_uninstall = false;
    app->has_badge = false;
    app->paused = false;
    apps.push_back(std::move(app));
    chrome_apps->OnApps(std::move(apps));
  }

  void WebAppsCrosapiOnApps() {
    mojo::PendingReceiver<crosapi::mojom::AppController> pending_receiver;
    mojo::PendingRemote<crosapi::mojom::AppController> pending_remote =
        pending_receiver.InitWithNewPipeAndPassRemote();
    WebAppsCrosapi* web_apps_crosapi =
        WebAppsCrosapiFactory::GetForProfile(profile());
    web_apps_crosapi->RegisterAppController(std::move(pending_remote));
    std::vector<AppPtr> apps;
    auto app = MakeApp(AppType::kWeb,
                       /*app_id=*/"a",
                       /*name=*/"TestApp", Readiness::kReady);
    app->additional_search_terms.push_back("TestApp");
    app->last_launch_time = kLastLaunchTime;
    app->install_time = kInstallTime;
    app->permissions = MakeFakePermissions();
    app->recommendable = true;
    app->searchable = true;
    app->show_in_launcher = true;
    app->show_in_shelf = true;
    app->show_in_search = true;
    app->show_in_management = true;
    app->handles_intents = true;
    app->allow_uninstall = true;
    app->has_badge = true;
    app->paused = true;
    app->window_mode = WindowMode::kBrowser;
    apps.push_back(std::move(app));
    web_apps_crosapi->OnApps(std::move(apps));
  }

 private:
  base::AutoReset<bool> set_lacros_enabled_ =
      BrowserSupport::SetLacrosEnabledForTest(true);
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(StandaloneBrowserPublisherTest, StandaloneBrowserAppsOnApps) {
  VerifyApp(AppType::kStandaloneBrowser, app_constants::kLacrosAppId, "Lacros",
            Readiness::kReady, InstallReason::kSystem, InstallSource::kSystem,
            {"chrome"}, base::Time(), base::Time(), apps::Permissions(),
            /*is_platform_app=*/false,
            /*recommendable=*/true, /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/false);
  VerifyAppTypeIsInitialized(AppType::kStandaloneBrowser);
}

TEST_F(StandaloneBrowserPublisherTest, StandaloneBrowserExtensionAppsOnApps) {
  ExtensionAppsOnApps();
  VerifyApp(AppType::kStandaloneBrowserChromeApp, "a", "TestApp",
            Readiness::kReady, InstallReason::kUser, InstallSource::kSync, {},
            base::Time(), base::Time(), apps::Permissions(),
            /*is_platform_app=*/true, /*recommendable=*/false,
            /*searchable=*/false,
            /*show_in_launcher=*/false, /*show_in_shelf=*/false,
            /*show_in_search=*/false, /*show_in_management=*/false,
            /*handles_intents=*/false, /*allow_uninstall=*/false,
            /*has_badge=*/false, /*paused=*/false);
}

// Verify the app is not updated when not register to CrosApi,
TEST_F(StandaloneBrowserPublisherTest,
       StandaloneBrowserExtensionAppsNotUpdated) {
  StandaloneBrowserExtensionApps* chrome_apps =
      StandaloneBrowserExtensionAppsFactoryForApp::GetForProfile(profile());

  AppRegistryCache& cache =
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
  AppRegistryCacheObserver observer(&cache);

  std::vector<AppPtr> apps;
  std::string app_id = "a";
  apps.push_back(MakeApp(AppType::kStandaloneBrowserChromeApp, app_id,
                         /*name=*/"TestApp", Readiness::kReady));
  chrome_apps->OnApps(std::move(apps));

  // Verify no app updated.
  EXPECT_EQ(AppType::kUnknown, cache.GetAppType(app_id));
  EXPECT_TRUE(observer.app_types().empty());
  EXPECT_TRUE(observer.updated_ids().empty());
}

// Verify apps are updated after register to CrosApi,
TEST_F(StandaloneBrowserPublisherTest, StandaloneBrowserExtensionAppsUpdated) {
  StandaloneBrowserExtensionApps* chrome_apps =
      StandaloneBrowserExtensionAppsFactoryForApp::GetForProfile(profile());

  AppRegistryCache& cache =
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
  AppRegistryCacheObserver observer(&cache);

  std::vector<AppPtr> apps1;
  std::string app_id1 = "a";
  std::string app_id2 = "b";
  apps1.push_back(MakeApp(AppType::kStandaloneBrowserChromeApp, app_id1,
                          /*name=*/"TestApp", Readiness::kReady));
  apps1.push_back(MakeApp(AppType::kStandaloneBrowserChromeApp, app_id2,
                          /*name=*/"TestApp", Readiness::kReady));
  chrome_apps->OnApps(std::move(apps1));

  std::vector<AppPtr> apps2;
  std::string app_id3 = "c";
  apps2.push_back(MakeApp(AppType::kStandaloneBrowserChromeApp, app_id3,
                          /*name=*/"TestApp", Readiness::kReady));
  chrome_apps->OnApps(std::move(apps2));

  // Verify no app updated, since Crosapi is not ready yet.
  EXPECT_EQ(AppType::kUnknown, cache.GetAppType(app_id1));
  EXPECT_EQ(AppType::kUnknown, cache.GetAppType(app_id2));
  EXPECT_EQ(AppType::kUnknown, cache.GetAppType(app_id3));
  EXPECT_TRUE(observer.app_types().empty());
  EXPECT_TRUE(observer.updated_ids().empty());

  // Register Crosapi, which should publish apps.
  mojo::PendingReceiver<crosapi::mojom::AppController> pending_receiver1;
  mojo::PendingRemote<crosapi::mojom::AppController> pending_remote1 =
      pending_receiver1.InitWithNewPipeAndPassRemote();
  chrome_apps->RegisterAppController(std::move(pending_remote1));

  EXPECT_EQ(AppType::kStandaloneBrowserChromeApp, cache.GetAppType(app_id1));
  EXPECT_EQ(AppType::kStandaloneBrowserChromeApp, cache.GetAppType(app_id2));
  EXPECT_EQ(AppType::kStandaloneBrowserChromeApp, cache.GetAppType(app_id3));
  ASSERT_EQ(1u, observer.app_types().size());
  EXPECT_EQ(AppType::kStandaloneBrowserChromeApp, observer.app_types()[0]);
  ASSERT_EQ(3u, observer.updated_ids().size());
  EXPECT_EQ(app_id1, observer.updated_ids()[0]);
  EXPECT_EQ(app_id2, observer.updated_ids()[1]);
  EXPECT_EQ(app_id3, observer.updated_ids()[2]);

  // Add more apps after register Crosapi.
  std::vector<AppPtr> apps3;
  std::string app_id4 = "d";
  apps3.push_back(MakeApp(AppType::kStandaloneBrowserChromeApp, app_id4,
                          /*name=*/"TestApp", Readiness::kReady));
  chrome_apps->OnApps(std::move(apps3));

  EXPECT_EQ(AppType::kStandaloneBrowserChromeApp, cache.GetAppType(app_id4));
  ASSERT_EQ(4u, observer.updated_ids().size());
  EXPECT_EQ(app_id4, observer.updated_ids()[3]);

  // Disconnect crosapi.
  chrome_apps->OnControllerDisconnected();

  // Add more apps after Crosapi disconnect.
  std::vector<AppPtr> apps4;
  std::string app_id5 = "e";
  apps4.push_back(MakeApp(AppType::kStandaloneBrowserChromeApp, app_id5,
                          /*name=*/"TestApp", Readiness::kReady));
  std::string app_id6 = "f";
  apps4.push_back(MakeApp(AppType::kStandaloneBrowserChromeApp, app_id6,
                          /*name=*/"TestApp", Readiness::kReady));
  chrome_apps->OnApps(std::move(apps4));

  // Simulate Crosapi reconnect, which should publish apps.
  mojo::PendingReceiver<crosapi::mojom::AppController> pending_receiver2;
  mojo::PendingRemote<crosapi::mojom::AppController> pending_remote2 =
      pending_receiver2.InitWithNewPipeAndPassRemote();
  chrome_apps->RegisterAppController(std::move(pending_remote2));

  EXPECT_EQ(AppType::kStandaloneBrowserChromeApp, cache.GetAppType(app_id5));
  EXPECT_EQ(AppType::kStandaloneBrowserChromeApp, cache.GetAppType(app_id6));
  ASSERT_EQ(1u, observer.app_types().size());
  ASSERT_EQ(6u, observer.updated_ids().size());
  EXPECT_EQ(app_id5, observer.updated_ids()[4]);
  EXPECT_EQ(app_id6, observer.updated_ids()[5]);
}

TEST_F(StandaloneBrowserPublisherTest, WebAppsCrosapiOnApps) {
  WebAppsCrosapiOnApps();
  VerifyApp(AppType::kWeb, "a", "TestApp", Readiness::kReady,
            InstallReason::kUser, InstallSource::kSync, {"TestApp"},
            kLastLaunchTime, kInstallTime, MakeFakePermissions(),
            /*is_platform_app=*/absl::nullopt, /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*has_badge=*/true, /*paused=*/true, WindowMode::kBrowser);
}

// Verify the app is not updated when not register to CrosApi,
TEST_F(StandaloneBrowserPublisherTest, WebAppsCrosapiNotUpdated) {
  WebAppsCrosapi* web_apps_crosapi =
      WebAppsCrosapiFactory::GetForProfile(profile());

  AppRegistryCache& cache =
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
  AppRegistryCacheObserver observer(&cache);

  std::vector<AppPtr> apps;
  std::string app_id = "a";
  apps.push_back(MakeApp(AppType::kWeb, app_id,
                         /*name=*/"TestApp", Readiness::kReady));
  web_apps_crosapi->OnApps(std::move(apps));

  // Verify no app updated.
  EXPECT_EQ(AppType::kUnknown, cache.GetAppType(app_id));
  EXPECT_TRUE(observer.app_types().empty());
  EXPECT_TRUE(observer.updated_ids().empty());
}

// Verify apps are updated after register to CrosApi,
TEST_F(StandaloneBrowserPublisherTest, WebAppsCrosapiUpdated) {
  WebAppsCrosapi* web_apps_crosapi =
      WebAppsCrosapiFactory::GetForProfile(profile());

  AppRegistryCache& cache =
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
  AppRegistryCacheObserver observer(&cache);

  std::string app_id1 = "a";
  std::string app_id2 = "b";
  {
    std::vector<AppPtr> apps1;
    apps1.push_back(MakeApp(AppType::kWeb, app_id1,
                            /*name=*/"TestApp", Readiness::kReady));
    apps1.push_back(MakeApp(AppType::kWeb, app_id2,
                            /*name=*/"TestApp", Readiness::kReady));
    web_apps_crosapi->OnApps(std::move(apps1));

    std::vector<CapabilityAccessPtr> capability_access1;
    capability_access1.push_back(MakeCapabilityAccess(app_id1,
                                                      /*camera=*/absl::nullopt,
                                                      /*microphone=*/true));
    capability_access1.push_back(
        MakeCapabilityAccess(app_id2,
                             /*camera=*/true,
                             /*microphone=*/absl::nullopt));
    web_apps_crosapi->OnCapabilityAccesses(std::move(capability_access1));
  }

  std::string app_id3 = "c";
  {
    std::vector<AppPtr> apps2;
    apps2.push_back(MakeApp(AppType::kWeb, app_id3,
                            /*name=*/"TestApp", Readiness::kReady));
    web_apps_crosapi->OnApps(std::move(apps2));

    std::vector<CapabilityAccessPtr> capability_access2;
    capability_access2.push_back(MakeCapabilityAccess(app_id3,
                                                      /*camera=*/true,
                                                      /*microphone=*/true));
    web_apps_crosapi->OnCapabilityAccesses(std::move(capability_access2));
  }

  // Verify no app updated, since Crosapi is not ready yet.
  EXPECT_EQ(AppType::kUnknown, cache.GetAppType(app_id1));
  EXPECT_EQ(AppType::kUnknown, cache.GetAppType(app_id2));
  EXPECT_EQ(AppType::kUnknown, cache.GetAppType(app_id3));
  EXPECT_TRUE(observer.app_types().empty());
  EXPECT_TRUE(observer.updated_ids().empty());
  VerifyNoCapabilityAccess(app_id1);
  VerifyNoCapabilityAccess(app_id2);
  VerifyNoCapabilityAccess(app_id3);

  // Register Crosapi, which should publish apps.
  mojo::PendingReceiver<crosapi::mojom::AppController> pending_receiver1;
  mojo::PendingRemote<crosapi::mojom::AppController> pending_remote1 =
      pending_receiver1.InitWithNewPipeAndPassRemote();
  web_apps_crosapi->RegisterAppController(std::move(pending_remote1));

  EXPECT_EQ(AppType::kWeb, cache.GetAppType(app_id1));
  EXPECT_EQ(AppType::kWeb, cache.GetAppType(app_id2));
  EXPECT_EQ(AppType::kWeb, cache.GetAppType(app_id3));
  ASSERT_EQ(1u, observer.app_types().size());
  EXPECT_EQ(AppType::kWeb, observer.app_types()[0]);
  ASSERT_EQ(3u, observer.updated_ids().size());
  EXPECT_EQ(app_id1, observer.updated_ids()[0]);
  EXPECT_EQ(app_id2, observer.updated_ids()[1]);
  EXPECT_EQ(app_id3, observer.updated_ids()[2]);
  VerifyCapabilityAccess(app_id1,
                         /*accessing_camera=*/absl::nullopt,
                         /*accessing_microphone=*/true);
  VerifyCapabilityAccess(app_id2,
                         /*accessing_camera=*/true,
                         /*accessing_microphone=*/absl::nullopt);
  VerifyCapabilityAccess(app_id3,
                         /*accessing_camera=*/true,
                         /*accessing_microphone=*/true);

  // Add more apps after register Crosapi.
  std::string app_id4 = "d";
  {
    std::vector<AppPtr> apps3;
    apps3.push_back(MakeApp(AppType::kWeb, app_id4,
                            /*name=*/"TestApp", Readiness::kReady));
    web_apps_crosapi->OnApps(std::move(apps3));

    std::vector<CapabilityAccessPtr> capability_access3;
    capability_access3.push_back(
        MakeCapabilityAccess(app_id4,
                             /*camera=*/true,
                             /*microphone=*/absl::nullopt));
    web_apps_crosapi->OnCapabilityAccesses(std::move(capability_access3));
  }

  EXPECT_EQ(AppType::kWeb, cache.GetAppType(app_id4));
  ASSERT_EQ(4u, observer.updated_ids().size());
  EXPECT_EQ(app_id4, observer.updated_ids()[3]);
  VerifyCapabilityAccess(app_id4,
                         /*accessing_camera=*/true,
                         /*accessing_microphone=*/absl::nullopt);

  // Disconnect crosapi.
  web_apps_crosapi->OnControllerDisconnected();

  // Add more apps after Crosapi disconnect.
  std::string app_id5 = "e";
  std::string app_id6 = "f";
  {
    std::vector<AppPtr> apps4;
    apps4.push_back(MakeApp(AppType::kWeb, app_id5,
                            /*name=*/"TestApp", Readiness::kReady));
    apps4.push_back(MakeApp(AppType::kWeb, app_id6,
                            /*name=*/"TestApp", Readiness::kReady));
    web_apps_crosapi->OnApps(std::move(apps4));
  }

  // Simulate Crosapi reconnect, which should publish apps.
  mojo::PendingReceiver<crosapi::mojom::AppController> pending_receiver2;
  mojo::PendingRemote<crosapi::mojom::AppController> pending_remote2 =
      pending_receiver2.InitWithNewPipeAndPassRemote();
  web_apps_crosapi->RegisterAppController(std::move(pending_remote2));

  EXPECT_EQ(AppType::kWeb, cache.GetAppType(app_id5));
  EXPECT_EQ(AppType::kWeb, cache.GetAppType(app_id6));
  ASSERT_EQ(1u, observer.app_types().size());
  ASSERT_EQ(6u, observer.updated_ids().size());
  EXPECT_EQ(app_id5, observer.updated_ids()[4]);
  EXPECT_EQ(app_id6, observer.updated_ids()[5]);
  VerifyNoCapabilityAccess(app_id5);
  VerifyNoCapabilityAccess(app_id6);
}

// Verify capability access may arrive without app.
TEST_F(StandaloneBrowserPublisherTest, WebAppsCrosapiUpdatedCapability) {
  WebAppsCrosapi* web_apps_crosapi =
      WebAppsCrosapiFactory::GetForProfile(profile());

  std::string app_id1 = "a";
  std::string app_id2 = "b";
  {
    std::vector<CapabilityAccessPtr> capability_access1;
    capability_access1.push_back(MakeCapabilityAccess(app_id1,
                                                      /*camera=*/absl::nullopt,
                                                      /*microphone=*/true));
    capability_access1.push_back(
        MakeCapabilityAccess(app_id2,
                             /*camera=*/true,
                             /*microphone=*/absl::nullopt));
    web_apps_crosapi->OnCapabilityAccesses(std::move(capability_access1));
  }

  // Verify no capability access occurred, since Crosapi is not ready yet.
  VerifyNoCapabilityAccess(app_id1);
  VerifyNoCapabilityAccess(app_id2);

  // Register Crosapi, which should publish capability access.
  mojo::PendingReceiver<crosapi::mojom::AppController> pending_receiver1;
  mojo::PendingRemote<crosapi::mojom::AppController> pending_remote1 =
      pending_receiver1.InitWithNewPipeAndPassRemote();
  web_apps_crosapi->RegisterAppController(std::move(pending_remote1));

  VerifyCapabilityAccess(app_id1,
                         /*accessing_camera=*/absl::nullopt,
                         /*accessing_microphone=*/true);
  VerifyCapabilityAccess(app_id2,
                         /*accessing_camera=*/true,
                         /*accessing_microphone=*/absl::nullopt);

  // Add more capability access after register Crosapi.
  std::string app_id3 = "c";
  {
    std::vector<CapabilityAccessPtr> capability_access2;
    capability_access2.push_back(MakeCapabilityAccess(app_id3,
                                                      /*camera=*/true,
                                                      /*microphone=*/true));
    web_apps_crosapi->OnCapabilityAccesses(std::move(capability_access2));
  }

  VerifyCapabilityAccess(app_id3,
                         /*accessing_camera=*/true,
                         /*accessing_microphone=*/true);

  // Disconnect crosapi.
  web_apps_crosapi->OnControllerDisconnected();
}

// Verify if OnApps was never called, the registration of AppController will not
// initialize the web app type.
TEST_F(StandaloneBrowserPublisherTest, WebAppsNotInitializedIfRegisterFirst) {
  WebAppsCrosapi* web_apps_crosapi =
      WebAppsCrosapiFactory::GetForProfile(profile());

  AppRegistryCache& cache =
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
  AppRegistryCacheObserver observer(&cache);

  // Verify no app updated, since Crosapi is not ready yet.
  EXPECT_TRUE(observer.app_types().empty());
  EXPECT_TRUE(observer.updated_ids().empty());

  // Register Crosapi first, there should be no app updates because OnApps
  // was never called.
  mojo::PendingReceiver<crosapi::mojom::AppController> pending_receiver1;
  mojo::PendingRemote<crosapi::mojom::AppController> pending_remote1 =
      pending_receiver1.InitWithNewPipeAndPassRemote();
  web_apps_crosapi->RegisterAppController(std::move(pending_remote1));

  EXPECT_TRUE(observer.app_types().empty());
  EXPECT_TRUE(observer.updated_ids().empty());

  std::vector<AppPtr> apps1;
  std::string app_id1 = "a";
  std::string app_id2 = "b";
  apps1.push_back(MakeApp(AppType::kWeb, app_id1,
                          /*name=*/"TestApp", Readiness::kReady));
  apps1.push_back(MakeApp(AppType::kWeb, app_id2,
                          /*name=*/"TestApp", Readiness::kReady));
  web_apps_crosapi->OnApps(std::move(apps1));

  std::vector<AppPtr> apps2;
  std::string app_id3 = "c";
  apps2.push_back(MakeApp(AppType::kWeb, app_id3,
                          /*name=*/"TestApp", Readiness::kReady));
  web_apps_crosapi->OnApps(std::move(apps2));

  EXPECT_EQ(AppType::kWeb, cache.GetAppType(app_id1));
  EXPECT_EQ(AppType::kWeb, cache.GetAppType(app_id2));
  EXPECT_EQ(AppType::kWeb, cache.GetAppType(app_id3));
  ASSERT_EQ(1u, observer.app_types().size());
  EXPECT_EQ(AppType::kWeb, observer.app_types()[0]);
  ASSERT_EQ(3u, observer.updated_ids().size());
  EXPECT_EQ(app_id1, observer.updated_ids()[0]);
  EXPECT_EQ(app_id2, observer.updated_ids()[1]);
  EXPECT_EQ(app_id3, observer.updated_ids()[2]);
}

TEST_F(StandaloneBrowserPublisherTest, WebAppsInitializedForEmptyList) {
  WebAppsCrosapi* web_apps_crosapi =
      WebAppsCrosapiFactory::GetForProfile(profile());

  AppRegistryCache& cache =
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
  AppRegistryCacheObserver observer(&cache);

  web_apps_crosapi->OnApps(std::vector<AppPtr>{});
  // Verify no app updated, since Crosapi is not ready yet.
  EXPECT_TRUE(observer.app_types().empty());
  EXPECT_TRUE(observer.updated_ids().empty());

  mojo::PendingReceiver<crosapi::mojom::AppController> pending_receiver1;
  mojo::PendingRemote<crosapi::mojom::AppController> pending_remote1 =
      pending_receiver1.InitWithNewPipeAndPassRemote();
  web_apps_crosapi->RegisterAppController(std::move(pending_remote1));
  ASSERT_EQ(1u, observer.app_types().size());
  EXPECT_EQ(AppType::kWeb, observer.app_types()[0]);
  EXPECT_TRUE(observer.updated_ids().empty());
}

// Check that when Lacros is primary, extension apps are not published to the
// app service.
TEST_F(StandaloneBrowserPublisherTest, ExtensionAppsNotPublished) {
  // Install a "web store" app.
  scoped_refptr<extensions::Extension> store =
      MakeExtensionApp("webstore", "0.0", "http://google.com",
                       std::string(extensions::kWebStoreAppId));
  service_->AddExtension(store.get());

  AppRegistryCache& cache =
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
  EXPECT_EQ(AppType::kUnknown, cache.GetAppType(store->id()));
}

// This framework conveniently sets up everything but borealis.
using NonBorealisPublisherTest = StandaloneBrowserPublisherTest;

TEST_F(NonBorealisPublisherTest, BorealisAppsNotAllowed) {
  EXPECT_THAT(*GetApp(borealis::kInstallerAppId), testing::Not(Ready()));
}

class BorealisPublisherTest : public StandaloneBrowserPublisherTest {
 public:
  BorealisPublisherTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {features::kBorealis, ash::features::kBorealisPermitted}, {});
  }
};

TEST_F(BorealisPublisherTest, BorealisAppsAllowed) {
  EXPECT_THAT(
      *GetApp(borealis::kInstallerAppId),
      testing::AllOf(Ready(), ShownInShelf(true), ShownInLauncher(false)));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(PublisherTest, ExtensionAppsOnApps) {
  // Re-init AppService to verify the init process.
  AppServiceTest app_service_test;
  app_service_test.SetUp(profile());

  // Install a "web store" app.
  scoped_refptr<extensions::Extension> store =
      MakeExtensionApp("webstore", "0.0", "http://google.com",
                       std::string(extensions::kWebStoreAppId));
  service_->AddExtension(store.get());

  VerifyApp(AppType::kChromeApp, store->id(), store->name(), Readiness::kReady,
            InstallReason::kDefault, InstallSource::kChromeWebStore, {},
            base::Time(), base::Time(), apps::Permissions(),
            /*is_platform_app=*/true, /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*has_badge=*/false, /*paused=*/false);
  VerifyAppTypeIsInitialized(AppType::kChromeApp);

  // Uninstall the Chrome app.
  service_->UninstallExtension(
      store->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  VerifyApp(AppType::kChromeApp, store->id(), store->name(),
            Readiness::kUninstalledByUser, InstallReason::kDefault,
            InstallSource::kChromeWebStore, {}, base::Time(), base::Time(),
            apps::Permissions(), /*is_platform_app=*/true,
            /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*has_badge=*/false, /*paused=*/false);

  // Reinstall the Chrome app.
  service_->AddExtension(store.get());
  VerifyApp(AppType::kChromeApp, store->id(), store->name(), Readiness::kReady,
            InstallReason::kDefault, InstallSource::kChromeWebStore, {},
            base::Time(), base::Time(), apps::Permissions(),
            /*is_platform_app=*/true, /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*has_badge=*/false, /*paused=*/false);

  // Test OnExtensionLastLaunchTimeChanged.
  extensions::ExtensionPrefs::Get(profile())->SetLastLaunchTime(
      store->id(), kLastLaunchTime);
  VerifyApp(AppType::kChromeApp, store->id(), store->name(), Readiness::kReady,
            InstallReason::kDefault, InstallSource::kChromeWebStore, {},
            kLastLaunchTime, base::Time(), apps::Permissions(),
            /*is_platform_app=*/true);
}

TEST_F(PublisherTest, WebAppsOnApps) {
  const std::string kAppName = "Web App";
  AppServiceTest app_service_test_;
  app_service_test_.SetUp(profile());
  auto app_id = CreateWebApp(kAppName);

  VerifyApp(AppType::kWeb, app_id, kAppName, Readiness::kReady,
            InstallReason::kSync, InstallSource::kBrowser, {}, base::Time(),
            base::Time(), apps::Permissions(), /*is_platform_app=*/false,
            /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*has_badge=*/false, /*paused=*/false, WindowMode::kWindow);
  VerifyIntentFilters(app_id);
  VerifyAppTypeIsInitialized(AppType::kWeb);
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PublisherTest, ArcPublishPromiseApps) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeature(ash::features::kPromiseIcons);

  ArcAppTest arc_test;
  arc_test.SetUp(profile());
  std::string package_name = "test.package.name";
  PackageId package_id = PackageId(AppType::kArc, package_name);

  // Confirm that there isn't a promise app yet.
  ASSERT_FALSE(GetPromiseApp(package_id));

  // Notify the publisher about a started installation.
  arc_test.app_instance()->SendInstallationStarted(package_name);

  // Verify the ARC promise app is added to PromiseAppRegistryCache.
  PromiseAppPtr& promise_app = GetPromiseApp(package_id);
  ASSERT_TRUE(promise_app);
  ASSERT_EQ(promise_app->package_id, package_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
