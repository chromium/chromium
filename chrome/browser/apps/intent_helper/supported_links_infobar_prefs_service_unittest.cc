// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/supported_links_infobar_prefs_service.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestAppId1[] = "foo";
const char kTestAppId2[] = "bar";
}  // namespace

namespace apps {

class SupportedLinksInfoBarPrefsServiceTest : public testing::Test {
 public:
  SupportedLinksInfoBarPrefsServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile());

    AppPtr app1 = std::make_unique<apps::App>(apps::AppType::kWeb, kTestAppId1);
    app1->readiness = Readiness::kReady;
    AppPtr app2 = std::make_unique<apps::App>(apps::AppType::kWeb, kTestAppId2);
    app2->readiness = Readiness::kReady;

    std::vector<AppPtr> apps;
    apps.push_back(std::move(app1));
    apps.push_back(std::move(app2));

    proxy->AppRegistryCache().OnApps(std::move(apps), AppType::kWeb,
                                     /*should_notify_initialized=*/false);
  }

  TestingProfile* profile() { return &profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(SupportedLinksInfoBarPrefsServiceTest, IgnoreInfoBar) {
  SupportedLinksInfoBarPrefsService service(profile());

  for (int i = 0; i < 3; i++) {
    ASSERT_FALSE(service.ShouldHideInfoBarForApp(kTestAppId1));
    service.MarkInfoBarIgnored(kTestAppId1);
  }

  ASSERT_TRUE(service.ShouldHideInfoBarForApp(kTestAppId1));
  ASSERT_FALSE(service.ShouldHideInfoBarForApp(kTestAppId2));
}

TEST_F(SupportedLinksInfoBarPrefsServiceTest, DismissInfoBar) {
  SupportedLinksInfoBarPrefsService service(profile());

  service.MarkInfoBarDismissed(kTestAppId1);
  ASSERT_TRUE(service.ShouldHideInfoBarForApp(kTestAppId1));
  ASSERT_FALSE(service.ShouldHideInfoBarForApp(kTestAppId2));
}

TEST_F(SupportedLinksInfoBarPrefsServiceTest, DeletePrefsOnUninstall) {
  SupportedLinksInfoBarPrefsService service(profile());
  service.MarkInfoBarDismissed(kTestAppId1);

  // Uninstall the app.
  AppPtr app = std::make_unique<apps::App>(apps::AppType::kWeb, kTestAppId1);
  app->readiness = Readiness::kUninstalledByUser;

  auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
  std::vector<AppPtr> apps;
  apps.push_back(std::move(app));
  proxy->AppRegistryCache().OnApps(std::move(apps), AppType::kWeb,
                                   /*should_notify_initialized=*/false);

  ASSERT_FALSE(service.ShouldHideInfoBarForApp(kTestAppId1));
}

}  // namespace apps
