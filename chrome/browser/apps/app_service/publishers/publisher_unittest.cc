// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation_traits.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
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
#include "chrome/grit/branded_strings.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/capability_access_update.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/base/l10n/l10n_util.h"

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
        /*vpn_provider=*/false, /*web_app_info=*/nullptr, std::nullopt,
        std::move(permissions));
    arc_test.AddPackage(package->Clone());
    arc_test.app_instance()->SendPackageAdded(package->Clone());
  }
}

apps::Permissions MakeFakePermissions() {
  apps::Permissions permissions;
  permissions.push_back(std::make_unique<apps::Permission>(
      apps::PermissionType::kCamera, apps::TriState::kBlock,
      /*is_managed*/ false));
  permissions.push_back(std::make_unique<apps::Permission>(
      apps::PermissionType::kLocation, apps::TriState::kAllow,
      /*is_managed*/ false));
  return permissions;
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
      apps_util::AuthorityView::Encode(url), apps::PatternMatchType::kLiteral));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kAuthority, std::move(values3)));

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
    app_registry_cache_observer_.Observe(cache);
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
    app_registry_cache_observer_.Reset();
  }

  std::vector<std::string> updated_ids() const { return updated_ids_; }
  std::vector<apps::AppType> app_types() const { return app_types_; }

 private:
  std::vector<std::string> updated_ids_;
  std::vector<apps::AppType> app_types_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
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

    auto externally_managed_app_manager =
        std::make_unique<web_app::ExternallyManagedAppManager>(profile());
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

    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(kAppUrl);
    web_app_info->title = base::UTF8ToUTF16(app_name);
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

  void VerifyOptionalBool(std::optional<bool> source,
                          std::optional<bool> target) {
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
                 std::optional<bool> is_platform_app = std::nullopt,
                 std::optional<bool> recommendable = std::nullopt,
                 std::optional<bool> searchable = std::nullopt,
                 std::optional<bool> show_in_launcher = std::nullopt,
                 std::optional<bool> show_in_shelf = std::nullopt,
                 std::optional<bool> show_in_search = std::nullopt,
                 std::optional<bool> show_in_management = std::nullopt,
                 std::optional<bool> handles_intents = std::nullopt,
                 std::optional<bool> allow_uninstall = std::nullopt,
                 std::optional<bool> allow_close = std::nullopt,
                 std::optional<bool> has_badge = std::nullopt,
                 std::optional<bool> paused = std::nullopt,
                 std::optional<bool> allow_window_mode_selection = std::nullopt,
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
    VerifyOptionalBool(allow_close, cache.states_[app_id]->allow_close);
    VerifyOptionalBool(has_badge, cache.states_[app_id]->has_badge);
    VerifyOptionalBool(paused, cache.states_[app_id]->paused);
    VerifyOptionalBool(allow_window_mode_selection,
                       cache.states_[app_id]->allow_window_mode_selection);
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
                              std::optional<bool> accessing_camera,
                              std::optional<bool> accessing_microphone) {
    std::optional<bool> camera;
    std::optional<bool> microphone;
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
              NOTREACHED_IN_MIGRATION();
            }));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
 private:
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
          apps::Permissions(),
          /*is_platform_app=*/false,
          /*recommendable=*/true, /*searchable=*/true,
          /*show_in_launcher=*/true, /*show_in_shelf=*/true,
          /*show_in_search=*/true, /*show_in_management=*/true,
          /*handles_intents=*/true,
          /*allow_uninstall=*/app_info->ready && !app_info->sticky,
          /*allow_close=*/true,
          /*has_badge=*/false, /*paused=*/false,
          /*allow_window_mode_selection=*/std::nullopt);
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
          MakeFakePermissions(),
          /*is_platform_app=*/false,
          /*recommendable=*/true, /*searchable=*/true,
          /*show_in_launcher=*/true, /*show_in_shelf=*/true,
          /*show_in_search=*/true, /*show_in_management=*/true,
          /*handles_intents=*/true,
          /*allow_uninstall=*/app_info->ready && !app_info->sticky,
          /*allow_close=*/true,
          /*has_badge=*/false, /*paused=*/false,
          /*allow_window_mode_selection=*/std::nullopt);

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
                           /*accessing_microphone=*/std::nullopt);
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
                           /*accessing_microphone=*/std::nullopt);
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
              apps::Permissions(),
              /*is_platform_app=*/false, internal_app.recommendable,
              internal_app.searchable, internal_app.show_in_launcher,
              internal_app.searchable, internal_app.searchable,
              /*show_in_management=*/false, internal_app.show_in_launcher,
              /*allow_uninstall=*/false,
              /*allow_close=*/true,
              /*allow_window_mode_selection=*/std::nullopt);
  }
  VerifyAppTypeIsInitialized(AppType::kBuiltIn);
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
            /*allow_close=*/true,
            /*has_badge=*/false, /*paused=*/false,
            /*allow_window_mode_selection=*/std::nullopt);
  VerifyAppTypeIsInitialized(AppType::kChromeApp);

  // Uninstall the Chrome app.
  service_->UninstallExtension(
      store->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  VerifyApp(AppType::kChromeApp, store->id(), store->name(),
            Readiness::kUninstalledByUser, InstallReason::kDefault,
            InstallSource::kChromeWebStore, {}, base::Time(), base::Time(),
            apps::Permissions(),
            /*is_platform_app=*/true,
            /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*allow_close=*/true,
            /*has_badge=*/false, /*paused=*/false,
            /*allow_window_mode_selection=*/std::nullopt);

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
            /*allow_close=*/true,
            /*has_badge=*/false, /*paused=*/false,
            /*allow_window_mode_selection=*/std::nullopt);

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

  InstallReason expected_install_reason = InstallReason::kSync;
  if (base::FeatureList::IsEnabled(
          features::kWebAppDontAddExistingAppsToSync)) {
    expected_install_reason = InstallReason::kUser;
  }

  VerifyApp(AppType::kWeb, app_id, kAppName, Readiness::kReady,
            expected_install_reason, InstallSource::kBrowser, {}, base::Time(),
            base::Time(), apps::Permissions(),
            /*is_platform_app=*/false,
            /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*allow_close=*/true,
            /*has_badge=*/false, /*paused=*/false,
            /*allow_window_mode_selection=*/true, WindowMode::kWindow);
  VerifyIntentFilters(app_id);
  VerifyAppTypeIsInitialized(AppType::kWeb);
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace apps
