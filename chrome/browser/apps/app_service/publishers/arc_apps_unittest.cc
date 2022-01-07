// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/arc_apps.h"

#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/intent_helper/intent_filter.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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
}  // namespace

class ArcAppsPublisherTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    scoped_feature_list_.InitAndDisableFeature(
        features::kDefaultLinkCapturingInBrowser);

    // Do not destroy the ArcServiceManager during TearDown, so that Arc
    // KeyedServices can be correctly destroyed during profile shutdown.
    arc_test_.set_persist_service_manager(true);
    // We will manually start ArcApps after setting up IntentHelper, this allows
    // ArcApps to observe the correct IntentHelper during initialization.
    arc_test_.set_start_app_service_publisher(false);
    arc_test_.SetUp(profile());

    intent_helper_ =
        arc::ArcIntentHelperBridge::GetForBrowserContextForTesting(profile());
    intent_helper_instance_ = std::make_unique<arc::FakeIntentHelperInstance>();
    auto* arc_bridge_service =
        arc_test_.arc_service_manager()->arc_bridge_service();
    arc_bridge_service->intent_helper()->SetInstance(
        intent_helper_instance_.get());

    auto* const provider = web_app::FakeWebAppProvider::Get(&profile_);
    provider->SkipAwaitingExtensionSystem();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    app_service_test_.SetUp(&profile_);
    apps::ArcAppsFactory::GetForProfile(profile());
    app_service_test_.FlushMojoCalls();
  }

  void TearDown() override { arc_test_.TearDown(); }

  void FlushMojoCalls() { app_service_test_.FlushMojoCalls(); }

  TestingProfile* profile() { return &profile_; }

  arc::ArcIntentHelperBridge* intent_helper() { return intent_helper_; }

  arc::FakeIntentHelperInstance* intent_helper_instance() {
    return intent_helper_instance_.get();
  }

  ArcAppTest* arc_test() { return &arc_test_; }

  apps::PreferredAppsListHandle& preferred_apps() {
    return apps::AppServiceProxyFactory::GetForProfile(profile())
        ->PreferredApps();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ArcAppTest arc_test_;
  TestingProfile profile_;
  apps::AppServiceTest app_service_test_;
  arc::ArcIntentHelperBridge* intent_helper_;
  std::unique_ptr<arc::FakeIntentHelperInstance> intent_helper_instance_;
};

// Verifies that a call to set the supported links preference from ARC persists
// the setting in app service.
// Flaky: https://crbug.com/1285361.
TEST_F(ArcAppsPublisherTest, DISABLED_SetSupportedLinksFromArc) {
  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0].package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0].package_name,
                                                 fake_apps[0].activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  std::vector<arc::mojom::SupportedLinksPtr> added_links;
  added_links.emplace_back(base::in_place, package_name,
                           CreateFilterList(package_name, {kTestAuthority}));
  intent_helper()->OnSupportedLinksChanged(
      std::move(added_links), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  FlushMojoCalls();

  ASSERT_EQ(app_id, preferred_apps().FindPreferredAppForUrl(
                        GURL("https://www.example.com/foo")));
}

// Verifies that a call to set the supported links preference from App Service
// syncs the setting to ARC.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksFromAppService) {
  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0].package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0].package_name,
                                                 fake_apps[0].activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  FlushMojoCalls();

  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->SetSupportedLinksPreference(app_id);
  FlushMojoCalls();

  ASSERT_TRUE(
      intent_helper_instance()->verified_links().find(package_name)->second);
}

// Verifies that when the behavior to open links in the browser by default is
// enabled, Android apps are not set to handle links during first install.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksDefaultBrowserBehavior) {
  base::test::ScopedFeatureList scoped_features(
      features::kDefaultLinkCapturingInBrowser);

  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0].package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0].package_name,
                                                 fake_apps[0].activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  std::vector<arc::mojom::SupportedLinksPtr> added_links;
  added_links.emplace_back(base::in_place, package_name,
                           CreateFilterList(package_name, {kTestAuthority}));
  intent_helper()->OnSupportedLinksChanged(
      std::move(added_links), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  FlushMojoCalls();

  ASSERT_EQ(absl::nullopt, preferred_apps().FindPreferredAppForUrl(
                               GURL("https://www.example.com/foo")));
}

// Verifies that when the behavior to open links in the browser by default is
// enabled, apps which are already preferred can still update their filters.
TEST_F(ArcAppsPublisherTest,
       SetSupportedLinksDefaultBrowserBehaviorAllowsUpdates) {
  constexpr char kTestAuthority[] = "www.example.com";
  constexpr char kTestAuthority2[] = "www.newexample.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0].package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0].package_name,
                                                 fake_apps[0].activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  std::vector<arc::mojom::SupportedLinksPtr> added_links;
  added_links.emplace_back(base::in_place, package_name,
                           CreateFilterList(package_name, {kTestAuthority}));
  intent_helper()->OnSupportedLinksChanged(
      std::move(added_links), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);
  FlushMojoCalls();

  base::test::ScopedFeatureList scoped_features(
      features::kDefaultLinkCapturingInBrowser);

  // Update filters with a new authority added.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name,
      CreateFilterList(package_name, {kTestAuthority, kTestAuthority2}));
  std::vector<arc::mojom::SupportedLinksPtr> added_links2;
  added_links2.emplace_back(
      base::in_place, package_name,
      CreateFilterList(package_name, {kTestAuthority, kTestAuthority2}));
  intent_helper()->OnSupportedLinksChanged(
      std::move(added_links2), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);
  FlushMojoCalls();

  ASSERT_EQ(app_id, preferred_apps().FindPreferredAppForUrl(
                        GURL("https://www.newexample.com/foo")));
}

// Verifies that when the behavior to open links in the browser by default is
// enabled, the user can still set an app as preferred through ARC settings.
TEST_F(ArcAppsPublisherTest,
       SetSupportedLinksDefaultBrowserBehaviorAllowsUserChanges) {
  base::test::ScopedFeatureList scoped_features(
      features::kDefaultLinkCapturingInBrowser);

  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0].package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0].package_name,
                                                 fake_apps[0].activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  std::vector<arc::mojom::SupportedLinksPtr> added_links;
  added_links.emplace_back(base::in_place, package_name,
                           CreateFilterList(package_name, {kTestAuthority}));
  intent_helper()->OnSupportedLinksChanged(
      std::move(added_links), {},
      arc::mojom::SupportedLinkChangeSource::kUserPreference);

  FlushMojoCalls();

  ASSERT_EQ(app_id, preferred_apps().FindPreferredAppForUrl(
                        GURL("https://www.example.com/foo")));
}
