// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service_ash.h"

#include <memory>

#include "ash/components/arc/test/fake_app_instance.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class AppInstallServiceAshTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    arc_test_.set_initialize_real_intent_helper_bridge(true);
    arc_test_.SetUp(&profile_);
  }

  void TearDown() override { arc_test_.TearDown(); }

  Profile* profile() { return &profile_; }

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

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcAppTest arc_test_;
  TestingProfile profile_;
};

TEST_F(AppInstallServiceAshTest, LaunchPlayStoreForArcInstallRequest) {
  InstallPlayStore();

  AppServiceProxyFactory::GetForProfile(profile())
      ->AppInstallService()
      .InstallApp(AppInstallSurface::kAppInstallUriUnknown,
                  PackageId(PackageType::kArc, "com.android.chrome"),
                  /*anchor_window=*/std::nullopt, base::DoNothing());

  ASSERT_EQ(intent_helper_instance()->handled_intents().size(), 1ul);
  ASSERT_EQ(
      intent_helper_instance()->handled_intents()[0].activity->package_name,
      arc::kPlayStorePackage);
}

}  // namespace apps
