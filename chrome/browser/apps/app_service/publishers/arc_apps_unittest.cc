// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/arc_apps.h"

#include <functional>
#include <memory>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/intent.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate_map.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/intent_helper/intent_filter.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace {

const char kTestPackageName[] = "com.example.this";
const apps::PackageId kTestPackageId(apps::PackageType::kArc,
                                     "com.example.this");

std::vector<arc::IntentFilter> CreateFilterList(
    const std::string& package_name,
    const std::vector<std::string>& authorities) {
  std::vector<arc::IntentFilter::AuthorityEntry> filter_authorities;
  for (const std::string& authority : authorities) {
    filter_authorities.emplace_back(authority, 0);
  }
  std::vector<arc::IntentFilter::PatternMatcher> patterns;
  patterns.emplace_back("/", arc::mojom::PatternType::PATTERN_PREFIX);

  auto filter = arc::IntentFilter(package_name, {arc::kIntentActionView},
                                  std::move(filter_authorities),
                                  std::move(patterns), {"https"}, {});
  std::vector<arc::IntentFilter> filters;
  filters.push_back(std::move(filter));
  return filters;
}

apps::IntentFilters CreateIntentFilters(
    const std::vector<std::string>& authorities) {
  apps::IntentFilters filters;
  apps::IntentFilterPtr filter = std::make_unique<apps::IntentFilter>();

  apps::ConditionValues values1;
  values1.push_back(std::make_unique<apps::ConditionValue>(
      apps_util::kIntentActionView, apps::PatternMatchType::kLiteral));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kAction, std::move(values1)));

  apps::ConditionValues values2;
  values2.push_back(std::make_unique<apps::ConditionValue>(
      "https", apps::PatternMatchType::kLiteral));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kScheme, std::move(values2)));

  apps::ConditionValues values;
  for (const std::string& authority : authorities) {
    values.push_back(std::make_unique<apps::ConditionValue>(
        authority, apps::PatternMatchType::kLiteral));
  }
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kAuthority, std::move(values)));

  apps::ConditionValues values3;
  values3.push_back(std::make_unique<apps::ConditionValue>(
      "/", apps::PatternMatchType::kPrefix));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kPath, std::move(values3)));

  filters.push_back(std::move(filter));

  return filters;
}

// Returns a FileSystemURL, encoded as a GURL, that points to a file in the
// Downloads directory.
GURL FileInDownloads(Profile* profile, base::FilePath file) {
  url::Origin origin = file_manager::util::GetFilesAppOrigin();
  std::string mount_point_name =
      file_manager::util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  mount_points->RegisterFileSystem(
      mount_point_name, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      file_manager::util::GetDownloadsFolderForProfile(profile));
  return mount_points
      ->CreateExternalFileSystemURL(blink::StorageKey::CreateFirstParty(origin),
                                    mount_point_name, file)
      .ToGURL();
}

std::vector<arc::mojom::AppInfoPtr> GetArcSettingsAppInfo() {
  std::vector<arc::mojom::AppInfoPtr> apps;
  arc::mojom::AppInfoPtr app(arc::mojom::AppInfo::New());
  app->name = "settings";
  app->package_name = "com.android.settings";
  app->activity = "com.android.settings.Settings";
  app->sticky = false;
  apps.push_back(std::move(app));
  return apps;
}

}  // namespace

class ArcAppsPublisherTest : public testing::Test {
 public:
  ArcAppsPublisherTest()
      : local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())) {}
  void SetUp() override {
    testing::Test::SetUp();

    profile_ = MakeProfile();

    // Do not destroy the ArcServiceManager during TearDown, so that Arc
    // KeyedServices can be correctly destroyed during profile shutdown.
    arc_test_.set_persist_service_manager(true);
    // We will manually start ArcApps after setting up IntentHelper, this allows
    // ArcApps to observe the correct IntentHelper during initialization.
    arc_test_.set_start_app_service_publisher(false);
    // We want to use the real ArcIntentHelper KeyedService so that it's the
    // same object that ArcApps uses.
    arc_test_.set_initialize_real_intent_helper_bridge(true);
    arc_test_.SetUp(profile());

    auto* arc_bridge_service =
        arc_test_.arc_service_manager()->arc_bridge_service();

    intent_helper_ =
        arc::ArcIntentHelperBridge::GetForBrowserContext(profile());
    arc_file_system_bridge_ = std::make_unique<arc::ArcFileSystemBridge>(
        profile(), arc_bridge_service);

    auto web_app_policy_manager =
        std::make_unique<web_app::WebAppPolicyManager>(profile());
    web_app_policy_manager->SetSystemWebAppDelegateMap(&system_apps_);
    auto* provider = web_app::FakeWebAppProvider::Get(profile());
    provider->SetWebAppPolicyManager(std::move(web_app_policy_manager));
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    app_service_test_.SetUp(profile_.get());
    apps::ArcAppsFactory::GetForProfile(profile());
    // Ensure that the PreferredAppsList is fully initialized before running the
    // test.
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    arc_test_.StopArcInstance();
    apps::ArcAppsFactory::GetInstance()->ShutDownForTesting(profile());
    arc_test_.TearDown();
  }

  virtual std::unique_ptr<TestingProfile> MakeProfile() {
    return std::make_unique<TestingProfile>();
  }

  void VerifyIntentFilters(const std::string& app_id,
                           const std::vector<std::string>& authorities) {
    apps::IntentFilters source = CreateIntentFilters(authorities);

    apps::IntentFilters target;
    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->AppRegistryCache()
        .ForOneApp(app_id, [&target](const apps::AppUpdate& update) {
          target = update.IntentFilters();
        });

    EXPECT_EQ(source.size(), target.size());
    for (size_t i = 0; i < source.size(); i++) {
      EXPECT_EQ(*source[i], *target[i]);
    }
  }

  void SetUpFileSystemInstance() {
    auto* arc_bridge_service =
        arc_test()->arc_service_manager()->arc_bridge_service();
    file_system_instance_ = std::make_unique<arc::FakeFileSystemInstance>();
    arc_bridge_service->file_system()->SetInstance(file_system_instance());
    arc::WaitForInstanceReady(arc_bridge_service->file_system());
  }

  TestingProfile* profile() { return profile_.get(); }

  apps::AppServiceProxy* app_service_proxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }

  arc::ArcIntentHelperBridge* intent_helper() { return intent_helper_; }

  arc::FakeIntentHelperInstance* intent_helper_instance() {
    return arc_test_.intent_helper_instance();
  }

  arc::FakeFileSystemInstance* file_system_instance() {
    return file_system_instance_.get();
  }

  ArcAppTest* arc_test() { return &arc_test_; }

  apps::PreferredAppsListHandle& preferred_apps() {
    return apps::AppServiceProxyFactory::GetForProfile(profile())
        ->PreferredAppsList();
  }

  std::vector<arc::mojom::SupportedLinksPackagePtr> CreateSupportedLinks(
      const std::string& package_name) {
    std::vector<arc::mojom::SupportedLinksPackagePtr> result;
    auto link = arc::mojom::SupportedLinksPackage::New();
    link->package_name = package_name;
    result.push_back(std::move(link));

    return result;
  }

 protected:
  std::unique_ptr<ScopedTestingLocalState> local_state_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  ash::SystemWebAppDelegateMap system_apps_;
  ArcAppTest arc_test_;
  std::unique_ptr<TestingProfile> profile_;
  apps::AppServiceTest app_service_test_;
  raw_ptr<arc::ArcIntentHelperBridge, DanglingUntriaged> intent_helper_;
  std::unique_ptr<arc::FakeFileSystemInstance> file_system_instance_;
  std::unique_ptr<arc::ArcFileSystemBridge> arc_file_system_bridge_;
};

// Verifies that a call to set the supported links preference from the ARC
// system doesn't change the setting in app service.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksFromArcSystem) {
  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  VerifyIntentFilters(app_id, {kTestAuthority});
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(package_name), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  ASSERT_EQ(std::nullopt, preferred_apps().FindPreferredAppForUrl(
                              GURL("https://www.example.com/foo")));
}

// Verifies that a call to set the supported links preference from App Service
// syncs the setting to ARC.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksFromAppService) {
  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  VerifyIntentFilters(app_id, {kTestAuthority});

  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->SetSupportedLinksPreference(app_id);

  ASSERT_TRUE(
      intent_helper_instance()->verified_links().find(package_name)->second);
}

// Verifies that the ARC system can still update preferred intent filters for
// apps which are already preferred.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksAllowsUpdates) {
  constexpr char kTestAuthority[] = "www.example.com";
  constexpr char kTestAuthority2[] = "www.newexample.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  VerifyIntentFilters(app_id, {kTestAuthority});

  // Set a user preference for the app.
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->SetSupportedLinksPreference(app_id);

  // Update filters with a new authority added.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name,
      CreateFilterList(package_name, {kTestAuthority, kTestAuthority2}));
  VerifyIntentFilters(app_id, {kTestAuthority, kTestAuthority2});
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(package_name), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  // Verify that the user preference has been extended to the new filter.
  ASSERT_EQ(app_id, preferred_apps().FindPreferredAppForUrl(
                        GURL("https://www.newexample.com/foo")));
}

// Verifies that the user can set an app as preferred through ARC settings.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksAllowsUserChanges) {
  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(package_name), {},
      arc::mojom::SupportedLinkChangeSource::kUserPreference);

  ASSERT_EQ(app_id, preferred_apps().FindPreferredAppForUrl(
                        GURL("https://www.example.com/foo")));
}

// Verifies that the Play Store app can be set as preferred by the system.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksAllowsPlayStoreDefault) {
  constexpr char kTestAuthority[] = "play.google.com";

  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.push_back(arc::mojom::AppInfo::New("Play Store", arc::kPlayStorePackage,
                                          arc::kPlayStoreActivity));
  arc_test()->app_instance()->SendRefreshAppList(apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      arc::kPlayStorePackage,
      CreateFilterList(arc::kPlayStorePackage, {kTestAuthority}));
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(arc::kPlayStorePackage), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  ASSERT_EQ(arc::kPlayStoreAppId, preferred_apps().FindPreferredAppForUrl(
                                      GURL("https://play.google.com/foo")));
}

// Verifies that disabling OS settings by SystemFeaturesDisableList policy will
// disable ARC settings as well. Clearing the policy should re-enable ARC
// settings.
TEST_F(ArcAppsPublisherTest, DisableOSSettingArcSettings) {
  arc_test()->app_instance()->SendRefreshAppList(GetArcSettingsAppInfo());

  // Change SystemFeaturesDisableList policy to disable OS Setting.
  {
    ScopedListPrefUpdate update(
        local_state_->Get(), policy::policy_prefs::kSystemFeaturesDisableList);
    update->Append(static_cast<int>(policy::SystemFeature::kOsSettings));
  }

  // Verify that ARC settings readiness is set to disabled by policy.
  bool found = app_service_proxy()->AppRegistryCache().ForOneApp(
      arc::kSettingsAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(update.Readiness(), apps::Readiness::kDisabledByPolicy);
      });
  ASSERT_TRUE(found);

  // Clear SystemFeaturesDisableList policy.
  {
    ScopedListPrefUpdate update(
        local_state_->Get(), policy::policy_prefs::kSystemFeaturesDisableList);
    update->clear();
  }

  // Verify that ARC settings readiness is set to ready.
  found = false;
  found = app_service_proxy()->AppRegistryCache().ForOneApp(
      arc::kSettingsAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(update.Readiness(), apps::Readiness::kReady);
      });
  ASSERT_TRUE(found);
}

// Verifies that disabling OS settings by SystemFeaturesDisableList policy and
// re-enabling does not remove the local settings block.
TEST_F(ArcAppsPublisherTest, DisableAndBlockOSSettingArcSettings) {
  arc_test()->app_instance()->SendRefreshAppList(GetArcSettingsAppInfo());

  // Change SystemFeaturesDisableList policy to disable OS Setting.
  {
    ScopedListPrefUpdate update(
        local_state_->Get(), policy::policy_prefs::kSystemFeaturesDisableList);
    update->Append(static_cast<int>(policy::SystemFeature::kOsSettings));
  }

  // Verify that ARC settings readiness is set to disabled by policy.
  bool found = app_service_proxy()->AppRegistryCache().ForOneApp(
      arc::kSettingsAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(update.Readiness(), apps::Readiness::kDisabledByPolicy);
      });
  ASSERT_TRUE(found);

  // Blocks the ARC settings app. It stays in kDisabledByPolicy.
  app_service_proxy()->BlockApps({arc::kSettingsAppId});
  found = app_service_proxy()->AppRegistryCache().ForOneApp(
      arc::kSettingsAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(update.Readiness(), apps::Readiness::kDisabledByPolicy);
      });
  ASSERT_TRUE(found);

  // Clear SystemFeaturesDisableList policy.
  {
    ScopedListPrefUpdate update(
        local_state_->Get(), policy::policy_prefs::kSystemFeaturesDisableList);
    update->clear();
  }

  // ARC settings should be in kDisabledByLocalSettings.
  found = app_service_proxy()->AppRegistryCache().ForOneApp(
      arc::kSettingsAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(update.Readiness(),
                  apps::Readiness::kDisabledByLocalSettings);
      });
  ASSERT_TRUE(found);

  // Unblocks the app.
  app_service_proxy()->UnblockApps({arc::kSettingsAppId});

  // Verify that ARC settings readiness is set to ready.
  found = app_service_proxy()->AppRegistryCache().ForOneApp(
      arc::kSettingsAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(update.Readiness(), apps::Readiness::kReady);
      });
  ASSERT_TRUE(found);
}

class ArcAppsPublisherManagedProfileTest : public ArcAppsPublisherTest {
 public:
  std::unique_ptr<TestingProfile> MakeProfile() override {
    TestingProfile::Builder builder;
    builder.OverridePolicyConnectorIsManagedForTesting(true);
    return builder.Build();
  }
};

// Verifies that a call to set the default supported links preference from the
// ARC system changes the app service setting, for a managed profile.
TEST_F(ArcAppsPublisherManagedProfileTest, SetSupportedLinksByDefault) {
  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  VerifyIntentFilters(app_id, {kTestAuthority});
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(package_name), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  ASSERT_EQ(app_id, preferred_apps().FindPreferredAppForUrl(
                        GURL("https://www.example.com/foo")));
}

// Verifies that a call to set the default supported links preference from the
// ARC system is ignored if the policy ArcOpenLinksInBrowserByDefault for
// a managed profile is set to true.
TEST_F(ArcAppsPublisherManagedProfileTest, SetSupportedLinksDisabledByPolicy) {
  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);
  profile()->GetPrefs()->SetBoolean(arc::prefs::kArcOpenLinksInBrowserByDefault,
                                    true);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  VerifyIntentFilters(app_id, {kTestAuthority});
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(package_name), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  ASSERT_EQ(std::nullopt, preferred_apps().FindPreferredAppForUrl(
                              GURL("https://www.example.com/foo")));
}

TEST_F(ArcAppsPublisherManagedProfileTest,
       SetSupportedLinksIgnoresWorkspaceInstall) {
  constexpr char kTestAuthority[] = "drive.google.com";
  std::string package_name = "com.google.android.apps.docs";
  std::string activity_name = base::StrCat({package_name, ".MainActivity"});
  std::string app_id = ArcAppListPrefs::GetAppId(package_name, activity_name);

  arc::mojom::AppInfoPtr app =
      arc::mojom::AppInfo::New("Google Drive", package_name, activity_name);
  std::vector<arc::mojom::AppInfoPtr> app_list;
  app_list.push_back(std::move(app));

  arc_test()->app_instance()->SendRefreshAppList(std::move(app_list));

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  VerifyIntentFilters(app_id, {kTestAuthority});
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(package_name), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  ASSERT_EQ(std::nullopt, preferred_apps().FindPreferredAppForUrl(
                              GURL("https://drive.google.com/foo")));
}

TEST_F(ArcAppsPublisherManagedProfileTest,
       SetSupportedLinksAllowsWorkspaceUserChange) {
  constexpr char kTestAuthority[] = "docs.google.com";
  std::string package_name = "com.google.android.apps.docs.editor.docs";
  std::string activity_name = base::StrCat({package_name, ".MainActivity"});
  std::string app_id = ArcAppListPrefs::GetAppId(package_name, activity_name);

  arc::mojom::AppInfoPtr app =
      arc::mojom::AppInfo::New("Google Docs", package_name, activity_name);
  std::vector<arc::mojom::AppInfoPtr> app_list;
  app_list.push_back(std::move(app));

  arc_test()->app_instance()->SendRefreshAppList(std::move(app_list));
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));

  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(package_name), {},
      arc::mojom::SupportedLinkChangeSource::kUserPreference);

  ASSERT_EQ(app_id, preferred_apps().FindPreferredAppForUrl(
                        GURL("https://docs.google.com/document/")));
}

TEST_F(ArcAppsPublisherManagedProfileTest,
       SetSupportedLinksAllowsWorkspaceUpdate) {
  constexpr char kDriveAuthority[] = "drive.google.com";
  constexpr char kDocsAuthority[] = "docs.google.com";
  std::string package_name = "com.google.android.apps.docs";
  std::string activity_name = base::StrCat({package_name, ".MainActivity"});
  std::string app_id = ArcAppListPrefs::GetAppId(package_name, activity_name);

  arc::mojom::AppInfoPtr app =
      arc::mojom::AppInfo::New("Google Drive", package_name, activity_name);
  std::vector<arc::mojom::AppInfoPtr> app_list;
  app_list.push_back(std::move(app));
  arc_test()->app_instance()->SendRefreshAppList(std::move(app_list));
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kDriveAuthority}));

  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->SetSupportedLinksPreference(app_id);
  ASSERT_EQ(app_id, preferred_apps().FindPreferredAppForUrl(
                        GURL("https://drive.google.com/foo")));

  // Simulate the app being updated to add a new intent filter.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name,
      CreateFilterList(package_name, {kDriveAuthority, kDocsAuthority}));
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(package_name), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  // Verify that the new intent filter is also marked as preferred.
  ASSERT_EQ(app_id, preferred_apps().FindPreferredAppForUrl(
                        GURL("https://docs.google.com/document")));
}

// Verifies that ARC permissions are published to App Service correctly.
TEST_F(ArcAppsPublisherTest, PublishPermission) {
  constexpr char kPackageName[] = "com.test.package";
  constexpr char kActivityName[] = "com.test.package.activity";

  const std::string kAppId =
      ArcAppListPrefs::GetAppId(kPackageName, kActivityName);

  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.push_back(
      arc::mojom::AppInfo::New("Fake app", kPackageName, kActivityName));
  arc_test()->app_instance()->SendRefreshAppList(apps);

  std::vector<arc::mojom::ArcPackageInfoPtr> packages;

  auto package = arc::mojom::ArcPackageInfo::New(
      kPackageName, /*package_version=*/1, /*last_backup_android_id=*/1,
      /*last_backup_time=*/1, /*sync=*/true);

  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions;

  permissions.emplace(arc::mojom::AppPermission::CAMERA,
                      arc::mojom::PermissionState::New(
                          /*granted=*/true, /*managed=*/false,
                          /*details=*/std::nullopt, /*one_time=*/true));
  permissions.emplace(
      arc::mojom::AppPermission::LOCATION,
      arc::mojom::PermissionState::New(/*granted=*/true, /*managed=*/true,
                                       /*details=*/"While in use"));

  package->permission_states = std::move(permissions);
  packages.push_back(std::move(package));

  arc_test()->app_instance()->SendRefreshPackageList(std::move(packages));

  apps::Permissions result;
  bool found = app_service_proxy()->AppRegistryCache().ForOneApp(
      kAppId, [&result](const apps::AppUpdate& update) {
        result = ClonePermissions(update.Permissions());
      });
  ASSERT_TRUE(found);

  EXPECT_EQ(result.size(), 2ul);

  // Sort permissions by permission type.
  base::ranges::sort(result, std::less<>(), &apps::Permission::permission_type);

  EXPECT_EQ(result[0]->permission_type, apps::PermissionType::kCamera);
  EXPECT_EQ(absl::get<apps::TriState>(result[0]->value), apps::TriState::kAsk);
  EXPECT_FALSE(result[0]->is_managed);
  EXPECT_EQ(result[0]->details, std::nullopt);

  EXPECT_EQ(result[1]->permission_type, apps::PermissionType::kLocation);
  EXPECT_TRUE(result[1]->IsPermissionEnabled());
  EXPECT_TRUE(result[1]->is_managed);
  EXPECT_EQ(result[1]->details, "While in use");
}

TEST_F(ArcAppsPublisherTest,
       LaunchAppWithIntent_EditIntent_SendsOpenUrlRequest) {
  SetUpFileSystemInstance();
  auto intent = apps_util::MakeEditIntent(
      FileInDownloads(profile(), base::FilePath("test.txt")), "text/plain");

  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  std::optional<apps::State> result;
  app_service_proxy()->LaunchAppWithIntent(
      app_id, 0, std::move(intent), apps::LaunchSource::kFromFileManager,
      /*window_info=*/nullptr,
      base::BindLambdaForTesting(
          [&result](apps::LaunchResult&& callback_result) {
            result = callback_result.state;
          }));

  ASSERT_EQ(apps::State::kSuccess, result.value_or(apps::State::kFailed));

  ASSERT_EQ(file_system_instance()->handledUrlRequests().size(), 1u);
  auto& url_request = file_system_instance()->handledUrlRequests()[0];
  ASSERT_EQ(url_request->action_type, arc::mojom::ActionType::EDIT);
  ASSERT_EQ(url_request->urls.size(), 1u);
  ASSERT_EQ(url_request->urls[0]->mime_type, "text/plain");
  ASSERT_TRUE(
      base::EndsWith(url_request->urls[0]->content_url.spec(), "test.txt"));
}

TEST_F(ArcAppsPublisherTest,
       LaunchAppWithIntent_EditIntent_NoArcFileSystem_ReturnsFalse) {
  // Do not start up ArcFileSystem, to simulate the intent being sent before ARC
  // starts.
  auto intent = apps_util::MakeEditIntent(
      FileInDownloads(profile(), base::FilePath("test.txt")), "text/plain");

  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  std::optional<apps::State> result;
  app_service_proxy()->LaunchAppWithIntent(
      app_id, 0, std::move(intent), apps::LaunchSource::kFromFileManager,
      /*window_info=*/nullptr,
      base::BindLambdaForTesting(
          [&result](apps::LaunchResult&& callback_result) {
            result = callback_result.state;
          }));

  ASSERT_EQ(apps::State::kFailed, result.value_or(apps::State::kSuccess));
}

TEST_F(
    ArcAppsPublisherTest,
    LaunchAppWithIntent_ViewFileIntent_SendsOpenUrlRequestWithIndividualFileMimeTypes) {
  SetUpFileSystemInstance();

  auto file1 = std::make_unique<apps::IntentFile>(
      FileInDownloads(profile(), base::FilePath("test1.png")));
  file1->mime_type = "image/png";

  auto file2 = std::make_unique<apps::IntentFile>(
      FileInDownloads(profile(), base::FilePath("test2.jpeg")));
  file2->mime_type = "image/jpeg";

  std::vector<apps::IntentFilePtr> files;
  files.push_back(std::move(file1));
  files.push_back(std::move(file2));

  auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                               std::move(files));

  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  std::optional<apps::State> result;
  app_service_proxy()->LaunchAppWithIntent(
      app_id, 0, std::move(intent), apps::LaunchSource::kFromFileManager,
      /*window_info=*/nullptr,
      base::BindLambdaForTesting(
          [&result](apps::LaunchResult&& callback_result) {
            result = callback_result.state;
          }));

  ASSERT_EQ(apps::State::kSuccess, result.value_or(apps::State::kFailed));

  ASSERT_EQ(file_system_instance()->handledUrlRequests().size(), 1u);
  auto& url_request = file_system_instance()->handledUrlRequests()[0];
  ASSERT_EQ(url_request->action_type, arc::mojom::ActionType::VIEW);
  ASSERT_EQ(url_request->urls.size(), 2u);
  ASSERT_EQ(url_request->urls[0]->mime_type, "image/png");
  ASSERT_EQ(url_request->urls[1]->mime_type, "image/jpeg");
  ASSERT_TRUE(
      base::EndsWith(url_request->urls[0]->content_url.spec(), "test1.png"));
  ASSERT_TRUE(
      base::EndsWith(url_request->urls[1]->content_url.spec(), "test2.jpeg"));
}

TEST_F(ArcAppsPublisherTest,
       LaunchAppWithIntent_ShareFileIntent_SendsOpenUrlRequest) {
  SetUpFileSystemInstance();

  std::string mime_type = "image/jpeg";
  std::string file_name = "test.jpeg";

  GURL url = FileInDownloads(profile(), base::FilePath(file_name));
  auto intent = apps_util::MakeShareIntent({url}, {mime_type});

  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  std::optional<apps::State> result;
  app_service_proxy()->LaunchAppWithIntent(
      app_id, 0, std::move(intent), apps::LaunchSource::kFromFileManager,
      /*window_info=*/nullptr,
      base::BindLambdaForTesting(
          [&result](apps::LaunchResult&& callback_result) {
            result = callback_result.state;
          }));

  ASSERT_EQ(apps::State::kSuccess, result.value_or(apps::State::kFailed));

  ASSERT_EQ(file_system_instance()->handledUrlRequests().size(), 1u);
  auto& url_request = file_system_instance()->handledUrlRequests()[0];
  ASSERT_EQ(url_request->action_type, arc::mojom::ActionType::SEND);
  ASSERT_EQ(url_request->urls.size(), 1u);
  ASSERT_EQ(url_request->urls[0]->mime_type, mime_type);
  ASSERT_TRUE(
      base::EndsWith(url_request->urls[0]->content_url.spec(), file_name));
}

TEST_F(ArcAppsPublisherTest, LaunchAppWithIntent_ShareFilesIntent_SendsExtras) {
  SetUpFileSystemInstance();

  constexpr char kTestIntentText[] = "launch text";
  constexpr char kTestIntentTitle[] = "launch title";
  constexpr char kTestExtraKey[] = "extra_key";
  constexpr char kTestExtraValue[] = "extra_value";

  GURL url = FileInDownloads(profile(), base::FilePath("test.jpeg"));
  auto intent = apps_util::MakeShareIntent({url}, {"image/jpeg"},
                                           kTestIntentText, kTestIntentTitle);
  intent->extras = {std::make_pair(kTestExtraKey, kTestExtraValue)};

  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  app_service_proxy()->LaunchAppWithIntent(
      app_id, 0, std::move(intent), apps::LaunchSource::kFromFileManager,
      /*window_info=*/nullptr, base::DoNothing());

  ASSERT_EQ(file_system_instance()->handledUrlRequests().size(), 1u);
  auto& url_request = file_system_instance()->handledUrlRequests()[0];
  ASSERT_EQ(url_request->action_type, arc::mojom::ActionType::SEND);
  ASSERT_EQ(url_request->urls.size(), 1u);
  ASSERT_EQ(url_request->extras.value()[kTestExtraKey], kTestExtraValue);
  ASSERT_EQ(url_request->extras.value()["android.intent.extra.TEXT"],
            kTestIntentText);
  ASSERT_EQ(url_request->extras.value()["android.intent.extra.SUBJECT"],
            kTestIntentTitle);
}

TEST_F(ArcAppsPublisherTest, SetAppLocale_SendsLocaleToArc) {
  // Setup.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(arc::kPerAppLanguage);
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile());
  ASSERT_NE(nullptr, prefs);
  // fake_packages[4] is the test package with localeInfo.
  const std::string& test_package_name =
      arc_test()->fake_apps()[4]->package_name;
  const std::string& app_id =
      prefs->GetAppId(test_package_name, arc_test()->fake_apps()[4]->activity);

  // Setup app.
  std::vector<arc::mojom::AppInfoPtr> test_app_info_list;
  test_app_info_list.push_back(arc_test()->fake_apps()[4]->Clone());
  arc_test()->app_instance()->SendRefreshAppList(test_app_info_list);
  // Setup package.
  // Initially pref will be set with "en" as selectedLocale.
  std::vector<arc::mojom::ArcPackageInfoPtr> test_packages;
  test_packages.push_back(arc_test()->fake_packages()[4]->Clone());
  arc_test()->app_instance()->SendRefreshPackageList(
      ArcAppTest::ClonePackages(test_packages));

  // Run.
  app_service_proxy()->SetAppLocale(app_id, "ja");

  // Assert.
  ASSERT_EQ("ja",
            arc_test()->app_instance()->selected_locale(test_package_name));
  ASSERT_EQ("ja",
            profile()->GetPrefs()->GetString(arc::prefs::kArcLastSetAppLocale));
}

class ArcAppsPublisherPromiseAppTest : public ArcAppsPublisherTest {
 public:
  void SetUp() override {
    ArcAppsPublisherTest::SetUp();
    feature_list_.InitAndEnableFeature(ash::features::kPromiseIcons);
    app_service_proxy()->ReinitializeForTesting(profile());
    service()->SetSkipAlmanacForTesting(true);
  }

  apps::PromiseAppService* service() {
    return app_service_proxy()->PromiseAppService();
  }

  apps::PromiseAppRegistryCache* cache() {
    return app_service_proxy()->PromiseAppRegistryCache();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ArcAppsPublisherPromiseAppTest,
       StartingInstallationRegistersPromiseApp) {
  // Verify that the promise app is not yet registered.
  EXPECT_FALSE(cache()->HasPromiseApp(kTestPackageId));

  arc_test()->app_instance()->SendInstallationStarted(kTestPackageName);

  // Verify that the promise app is now registered.
  EXPECT_TRUE(cache()->HasPromiseApp(kTestPackageId));
}

TEST_F(ArcAppsPublisherPromiseAppTest,
       InstallationProgressChangeUpdatesPromiseApp) {
  float progress_initial = 0.1;
  float progress_next = 0.9;

  // Add a promise app for testing.
  std::unique_ptr<apps::PromiseApp> promise_app =
      std::make_unique<apps::PromiseApp>(kTestPackageId);
  promise_app->progress = progress_initial;
  cache()->OnPromiseApp(std::move(promise_app));

  // Check that the initial progress value is correct.
  const apps::PromiseApp* promise_app_result =
      cache()->GetPromiseApp(kTestPackageId);
  EXPECT_TRUE(promise_app_result);
  EXPECT_TRUE(promise_app_result->progress.has_value());
  EXPECT_EQ(promise_app_result->progress.value(), progress_initial);

  // Send an update and check the progress value.
  arc_test()->app_instance()->SendInstallationProgressChanged(kTestPackageName,
                                                              progress_next);
  promise_app_result = cache()->GetPromiseApp(kTestPackageId);
  EXPECT_TRUE(promise_app_result);
  EXPECT_TRUE(promise_app_result->progress.has_value());
  EXPECT_EQ(promise_app_result->progress.value(), progress_next);
}

TEST_F(ArcAppsPublisherPromiseAppTest, ProgressUpdateChangesPromiseStatus) {
  // Add a promise app for testing.
  std::unique_ptr<apps::PromiseApp> promise_app =
      std::make_unique<apps::PromiseApp>(kTestPackageId);
  promise_app->status = apps::PromiseStatus::kPending;
  cache()->OnPromiseApp(std::move(promise_app));

  // Check that the initial status is kPending.
  const apps::PromiseApp* promise_app_result =
      cache()->GetPromiseApp(kTestPackageId);
  EXPECT_TRUE(promise_app_result);
  EXPECT_EQ(promise_app_result->status, apps::PromiseStatus::kPending);

  // Send a progress update and check the status.
  arc_test()->app_instance()->SendInstallationProgressChanged(kTestPackageName,
                                                              0.2);
  promise_app_result = cache()->GetPromiseApp(kTestPackageId);
  EXPECT_TRUE(promise_app_result);
  EXPECT_EQ(promise_app_result->status, apps::PromiseStatus::kInstalling);
}

TEST_F(ArcAppsPublisherPromiseAppTest, CancelledInstallationRemovesPromiseApp) {
  // Add a promise app to the cache.
  std::unique_ptr<apps::PromiseApp> promise_app =
      std::make_unique<apps::PromiseApp>(kTestPackageId);
  promise_app->status = apps::PromiseStatus::kPending;
  cache()->OnPromiseApp(std::move(promise_app));

  // Check that the promise app exists.
  EXPECT_TRUE(cache()->HasPromiseApp(kTestPackageId));

  // Confirm that the promise app gets removed after a cancelled/ failed
  // installation update.
  arc_test()->app_instance()->SendInstallationFinished(kTestPackageName, false);
  EXPECT_FALSE(cache()->HasPromiseApp(kTestPackageId));
}

TEST_F(ArcAppsPublisherPromiseAppTest,
       SuccessfulInstallationOfNonLaunchablePackageRemovesPromiseApp) {
  // Add a promise app to the cache.
  std::unique_ptr<apps::PromiseApp> promise_app =
      std::make_unique<apps::PromiseApp>(kTestPackageId);
  promise_app->status = apps::PromiseStatus::kPending;
  cache()->OnPromiseApp(std::move(promise_app));

  // Check that the promise app exists.
  EXPECT_TRUE(cache()->HasPromiseApp(kTestPackageId));

  // Confirm that the promise app gets removed after successful installation of
  // a non-launchable package.
  arc_test()->app_instance()->SendInstallationFinished(
      kTestPackageName, /*success=*/true,
      /*is_launchable_app=*/false);
  EXPECT_FALSE(cache()->HasPromiseApp(kTestPackageId));
}

TEST_F(ArcAppsPublisherPromiseAppTest,
       SuccessfulInstallationRemovesPromiseApp) {
  // Add a promise app to the cache.
  std::unique_ptr<apps::PromiseApp> promise_app =
      std::make_unique<apps::PromiseApp>(kTestPackageId);
  promise_app->status = apps::PromiseStatus::kPending;
  cache()->OnPromiseApp(std::move(promise_app));

  // Check that the promise app exists.
  EXPECT_TRUE(cache()->HasPromiseApp(kTestPackageId));

  // Confirm that the promise app gets removed after a successfully completed
  // installation.
  const auto& fake_apps = arc_test()->fake_apps();
  fake_apps[0]->package_name = kTestPackageName;
  std::string app_id =
      ArcAppListPrefs::GetAppId(kTestPackageName, "testActivity");
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Confirm that the promise app gets removed after the installed app gets
  // registered.
  EXPECT_FALSE(cache()->HasPromiseApp(kTestPackageId));
}

TEST_F(ArcAppsPublisherPromiseAppTest, PromiseAppsAreSuppressedForPiArc) {
  // Set ARC version to P, which we should not create promise apps for.
  apps::ArcApps::SetArcVersionForTesting(arc::kArcVersionP);

  // Verify that the promise app is not registered to begin with.
  EXPECT_FALSE(cache()->HasPromiseApp(kTestPackageId));

  // Trigger an installation event notification.
  arc_test()->app_instance()->SendInstallationStarted(kTestPackageName);

  // Verify that the promise app still isn't registered.
  EXPECT_FALSE(cache()->HasPromiseApp(kTestPackageId));
}

TEST_F(ArcAppsPublisherPromiseAppTest, PromiseAppsAreCreatedForRvcArc) {
  // Set ARC version to R, which should allow promise apps to be created.
  apps::ArcApps::SetArcVersionForTesting(arc::kArcVersionR);

  // Verify that the promise app is not registered to begin with.
  EXPECT_FALSE(cache()->HasPromiseApp(kTestPackageId));

  // Trigger an installation event notification.
  arc_test()->app_instance()->SendInstallationStarted(kTestPackageName);

  // Verify that the promise app is registered.
  EXPECT_TRUE(cache()->HasPromiseApp(kTestPackageId));
}

// Verifies that only valid intent filters will be published from ARC.
TEST_F(ArcAppsPublisherTest, OnlyValidFilterIsPublished) {
  const GURL kTestUrl("https://www.example.com");
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  std::vector<arc::IntentFilter::AuthorityEntry> filter_authorities1;
  filter_authorities1.emplace_back(kTestUrl.host(), 0);
  std::vector<arc::IntentFilter::PatternMatcher> patterns;
  patterns.emplace_back(kTestUrl.path(),
                        arc::mojom::PatternType::PATTERN_PREFIX);

  auto filter = arc::IntentFilter(package_name, {arc::kIntentActionView},
                                  std::move(filter_authorities1),
                                  std::move(patterns), {kTestUrl.scheme()}, {});
  std::vector<arc::IntentFilter> filters;
  filters.push_back(std::move(filter));

  std::vector<arc::IntentFilter::AuthorityEntry> filter_authorities2;
  filter_authorities2.emplace_back(kTestUrl.host(), 0);
  int invalid_pattern_type =
      static_cast<int>(arc::mojom::PatternType::kMaxValue) + 1;
  std::vector<arc::IntentFilter::PatternMatcher> invalid_pattern;
  invalid_pattern.emplace_back(
      kTestUrl.path(),
      static_cast<arc::mojom::PatternType>(invalid_pattern_type));

  auto invalid_filter = arc::IntentFilter(
      package_name, {arc::kIntentActionView}, std::move(filter_authorities2),
      std::move(invalid_pattern), {"https"}, {});
  filters.push_back(std::move(invalid_filter));

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(package_name,
                                                    std::move(filters));

  apps::IntentFilters published_filters;
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [&published_filters](const apps::AppUpdate& update) {
        published_filters = update.IntentFilters();
      });
  // Only one valid filter should be published.
  EXPECT_EQ(published_filters.size(), 1u);

  apps::IntentFilterPtr expected_filter =
      apps_util::MakeIntentFilterForUrlScope(kTestUrl,
                                             /*omit_port_for_testing=*/true);
  EXPECT_EQ(*published_filters[0], *expected_filter);
}
