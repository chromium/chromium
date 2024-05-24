// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_service.h"

#include <algorithm>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_preload_service/app_preload_almanac_endpoint.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"
#include "chrome/browser/apps/app_preload_service/proto/app_preload.pb.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFirstLoginFlowStartedKey[] = "first_login_flow_started";
constexpr char kFirstLoginFlowCompletedKey[] = "first_login_flow_completed";

constexpr char kApsStateManager[] = "apps.app_preload_service.state_manager";

const base::Value::Dict& GetStateManager(Profile* profile) {
  return profile->GetPrefs()->GetDict(kApsStateManager);
}

}  // namespace

namespace apps {

class AppPreloadServiceTest : public testing::Test {
 protected:
  AppPreloadServiceTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        startup_check_resetter_(
            AppPreloadService::DisablePreloadsOnStartupForTesting()) {
    scoped_feature_list_.InitWithFeatures(
        {features::kAppPreloadService, kAppPreloadServiceEnableShelfPin}, {});
    AppPreloadServiceFactory::SkipApiKeyCheckForTesting(true);
  }

  void SetUp() override {
    testing::Test::SetUp();

    GetFakeUserManager()->SetIsCurrentUserNew(true);

    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        url_loader_factory_.GetSafeWeakWrapper());
    profile_ = profile_builder.Build();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(GetProfile());
  }

  void TearDown() override {
    AppPreloadServiceFactory::SkipApiKeyCheckForTesting(false);
  }

  Profile* GetProfile() { return profile_.get(); }

  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  network::TestURLLoaderFactory url_loader_factory_;

 private:
  // BrowserTaskEnvironment has to be the first member or test will break.
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  user_manager::ScopedUserManager scoped_user_manager_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  base::AutoReset<bool> startup_check_resetter_;
};

TEST_F(AppPreloadServiceTest, ServiceAccessPerProfile) {
  // We expect the App Preload Service to be available in a normal profile.
  TestingProfile::Builder profile_builder;
  auto profile = profile_builder.Build();
  EXPECT_TRUE(AppPreloadServiceFactory::IsAvailable(profile.get()));
  auto* service = AppPreloadServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);

  // The service is unsupported in incognito.
  TestingProfile::Builder incognito_builder;
  auto* incognito_profile = incognito_builder.BuildIncognito(profile.get());
  EXPECT_FALSE(AppPreloadServiceFactory::IsAvailable(incognito_profile));
  EXPECT_EQ(nullptr,
            AppPreloadServiceFactory::GetForProfile(incognito_profile));

  // We expect the App Preload Service to not be available in either regular or
  // OTR guest profiles.
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  auto guest_profile = guest_builder.Build();
  EXPECT_FALSE(AppPreloadServiceFactory::IsAvailable(guest_profile.get()));
  EXPECT_EQ(nullptr,
            AppPreloadServiceFactory::GetForProfile(guest_profile.get()));

  auto* guest_otr_profile =
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(AppPreloadServiceFactory::IsAvailable(guest_otr_profile));
  EXPECT_EQ(nullptr,
            AppPreloadServiceFactory::GetForProfile(guest_otr_profile));

  // The service is unsupported for managed and supervised accounts.
  TestingProfile::Builder child_builder;
  child_builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> child_profile = child_builder.Build();

  EXPECT_FALSE(AppPreloadServiceFactory::IsAvailable(child_profile.get()));
  EXPECT_EQ(nullptr,
            AppPreloadServiceFactory::GetForProfile(child_profile.get()));

  TestingProfile::Builder managed_builder;
  managed_builder.OverridePolicyConnectorIsManagedForTesting(true);
  std::unique_ptr<TestingProfile> managed_profile = managed_builder.Build();

  EXPECT_FALSE(AppPreloadServiceFactory::IsAvailable(managed_profile.get()));
  EXPECT_EQ(nullptr,
            AppPreloadServiceFactory::GetForProfile(managed_profile.get()));
}

TEST_F(AppPreloadServiceTest, FirstLoginStartedPrefSet) {
  auto* service = AppPreloadService::Get(GetProfile());
  // Start the login flow, but do not wait for it to finish.
  service->StartFirstLoginFlowForTesting(base::DoNothing());

  auto flow_started =
      GetStateManager(GetProfile()).FindBool(kFirstLoginFlowStartedKey);
  auto flow_completed =
      GetStateManager(GetProfile()).FindBool(kFirstLoginFlowCompletedKey);
  // Since we're creating a new profile with no saved state, we expect the state
  // to be "started", but not "completed".
  EXPECT_TRUE(flow_started.has_value() && flow_started.value());
  EXPECT_EQ(flow_completed, std::nullopt);
}

TEST_F(AppPreloadServiceTest, FirstLoginCompletedPrefSetAfterSuccess) {
  // An empty response indicates that the request completed successfully, but
  // there are no apps to install.
  proto::AppPreloadListResponse response;

  url_loader_factory_.AddResponse(
      app_preload_almanac_endpoint::GetServerUrl().spec(),
      response.SerializeAsString());

  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(GetProfile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_TRUE(result.Get());

  // We expect that the key has been set after the first login flow has been
  // completed.
  auto flow_completed =
      GetStateManager(GetProfile()).FindBool(kFirstLoginFlowCompletedKey);
  EXPECT_NE(flow_completed, std::nullopt);
  EXPECT_TRUE(flow_completed.value());
}

TEST_F(AppPreloadServiceTest, FirstLoginExistingUserNotStarted) {
  GetFakeUserManager()->SetIsCurrentUserNew(false);
  TestingProfile existing_user_profile;

  auto* service = AppPreloadService::Get(&existing_user_profile);
  service->StartFirstLoginFlowForTesting(base::DoNothing());

  auto flow_started = GetStateManager(&existing_user_profile)
                          .FindBool(kFirstLoginFlowStartedKey);
  // Existing users should not start the first-login flow.
  EXPECT_FALSE(flow_started.has_value());
}

TEST_F(AppPreloadServiceTest, IgnoreAndroidAppInstall) {
  constexpr char kPackageName[] = "com.peanuttypes";
  constexpr char kActivityName[] = "com.peanuttypes.PeanutTypesActivity";

  proto::AppPreloadListResponse response;
  auto* app = response.add_apps_to_install();
  app->set_name("Peanut Types");
  app->set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);

  url_loader_factory_.AddResponse(
      app_preload_almanac_endpoint::GetServerUrl().spec(),
      response.SerializeAsString());

  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(GetProfile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_TRUE(result.Get());

  // It's hard to assert conclusively that nothing happens in this case, but for
  // now we just assert that the app wasn't added to App Service.
  auto app_id = ArcAppListPrefs::GetAppId(kPackageName, kActivityName);
  bool found = AppServiceProxyFactory::GetForProfile(GetProfile())
                   ->AppRegistryCache()
                   .ForOneApp(app_id, [](const AppUpdate&) {});
  ASSERT_FALSE(found);
}

TEST_F(AppPreloadServiceTest, FirstLoginStartedNotCompletedAfterServerError) {
  url_loader_factory_.AddResponse(
      app_preload_almanac_endpoint::GetServerUrl().spec(), /*content=*/"",
      net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(GetProfile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_FALSE(result.Get());

  auto flow_started =
      GetStateManager(GetProfile()).FindBool(kFirstLoginFlowStartedKey);
  auto flow_completed =
      GetStateManager(GetProfile()).FindBool(kFirstLoginFlowCompletedKey);
  // Since there was an error fetching apps, the flow should be "started" but
  // not "completed".
  EXPECT_EQ(flow_started, true);
  EXPECT_EQ(flow_completed, std::nullopt);
}

TEST_F(AppPreloadServiceTest, GetPinApps) {
  PackageId app1 = *PackageId::FromString("web:https://example.com/app1");
  PackageId app2 = *PackageId::FromString("web:https://example.com/app2");
  PackageId app3 = *PackageId::FromString("web:https://example.com/app3");
  PackageId app4 = *PackageId::FromString("web:https://example.com/app4");

  proto::AppPreloadListResponse response;
  auto add_app = [&](const std::string& package_id) {
    auto* app = response.add_apps_to_install();
    app->set_package_id(package_id);
    app->set_install_reason(
        proto::AppPreloadListResponse::INSTALL_REASON_DEFAULT);
  };
  auto add_shelf_config = [&](const std::string& package_id, uint32_t order) {
    auto* config = response.add_shelf_config();
    config->add_package_id(package_id);
    config->set_order(order);
  };
  add_app(app4.ToString());
  add_app(app2.ToString());
  add_app(app1.ToString());
  add_shelf_config(app3.ToString(), 3);
  add_shelf_config(app2.ToString(), 2);
  add_shelf_config(app1.ToString(), 1);

  url_loader_factory_.AddResponse(
      app_preload_almanac_endpoint::GetServerUrl().spec(),
      response.SerializeAsString());

  auto* service = AppPreloadService::Get(GetProfile());
  service->StartFirstLoginFlowForTesting(base::DoNothing());

  base::test::TestFuture<const std::vector<PackageId>&,
                         const std::vector<PackageId>&>
      result;
  service->GetPinApps(result.GetCallback());

  // Pin apps should ignore app4 since it is not in pin ordering.
  std::vector<apps::PackageId> pin_apps = result.Get<0>();
  EXPECT_EQ(pin_apps.size(), 2u);
  EXPECT_EQ(pin_apps[0], app2);
  EXPECT_EQ(pin_apps[1], app1);

  // Pin order should be sorted.
  std::vector<apps::PackageId> pin_order = result.Get<1>();
  EXPECT_EQ(pin_order.size(), 3u);
  EXPECT_EQ(pin_order[0], app1);
  EXPECT_EQ(pin_order[1], app2);
  EXPECT_EQ(pin_order[2], app3);
}

}  // namespace apps
