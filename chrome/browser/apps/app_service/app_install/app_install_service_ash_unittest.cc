// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service_ash.h"

#include <memory>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_install/app_install.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install_almanac_endpoint.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class AppInstallServiceAshTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    url_loader_factory_ = std::make_unique<network::TestURLLoaderFactory>();
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        url_loader_factory_->GetSafeWeakWrapper());
    profile_ = profile_builder.Build();

    arc_test_.set_initialize_real_intent_helper_bridge(true);
    arc_test_.SetUp(profile_.get());
  }

  void TearDown() override { arc_test_.TearDown(); }

  Profile* profile() { return profile_.get(); }

  arc::FakeIntentHelperInstance* intent_helper_instance() {
    return arc_test_.intent_helper_instance();
  }

  AppServiceProxy* app_service() {
    return AppServiceProxyFactory::GetForProfile(profile());
  }

  // Installs a fake Play Store app into App Service that handles links to
  // https://play.google.com/.
  void InstallPlayStore() {
    // Push the app definition into ArcAppListPrefs.
    std::vector<arc::mojom::AppInfoPtr> apps;
    apps.push_back(arc::mojom::AppInfo::New(
        "Play Store", arc::kPlayStorePackage, arc::kPlayStoreActivity));
    arc_test_.app_instance()->SendRefreshAppList(apps);

    // Manually poke App Service to add the desired Intent Filters and set the
    // Supported Links setting.
    AppPtr play_store =
        std::make_unique<App>(AppType::kArc, arc::kPlayStoreAppId);
    IntentFilters intent_filters;
    intent_filters.push_back(apps_util::MakeIntentFilterForUrlScope(
        GURL("https://play.google.com/"), /*omit_port_for_testing=*/true));
    play_store->intent_filters = std::move(intent_filters);

    std::vector<apps::AppPtr> deltas;
    deltas.push_back(std::move(play_store));
    app_service()->OnApps(std::move(deltas), AppType::kArc,
                          /*should_notify_initialized=*/false);
    apps_util::SetSupportedLinksPreferenceAndWait(profile(),
                                                  arc::kPlayStoreAppId);
  }

  void SetInstallUrlResponse(PackageId package_id, GURL install_url) {
    proto::AppInstallResponse response;
    proto::AppInstallResponse_AppInstance& instance =
        *response.mutable_app_instance();
    instance.set_package_id(package_id.ToString());
    instance.set_name("Test");
    instance.set_install_url(install_url.spec());
    url_loader_factory_->AddResponse(
        app_install_almanac_endpoint::GetEndpointUrlForTesting().spec(),
        response.SerializeAsString());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcAppTest arc_test_;
  std::unique_ptr<network::TestURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<Profile> profile_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

TEST_F(AppInstallServiceAshTest, LaunchPlayStoreForArcInstallRequest) {
  InstallPlayStore();
  PackageId install_package_id(PackageType::kArc, "com.android.chrome");
  GURL install_url =
      GURL("https://play.google.com/store/apps/details?id=com.android.chrome");
  SetInstallUrlResponse(install_package_id, install_url);

  base::test::TestFuture<void> complete_future;
  AppServiceProxyFactory::GetForProfile(profile())
      ->AppInstallService()
      .InstallApp(AppInstallSurface::kAppInstallUriUnknown, install_package_id,
                  /*anchor_window=*/std::nullopt,
                  complete_future.GetCallback());

  ASSERT_TRUE(complete_future.Wait());
  ASSERT_EQ(intent_helper_instance()->handled_intents().size(), 1ul);
  ASSERT_EQ(
      intent_helper_instance()->handled_intents()[0].activity->package_name,
      arc::kPlayStorePackage);
}

}  // namespace apps
