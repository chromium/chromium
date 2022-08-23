// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_update_observer.h"

#include <sys/types.h>

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

const char kAppId[] = "testappid";
const char kAppEmail[] = "test@example.com";
const char kAppInstallUrl[] = "https://example.com";
const char kAppLaunchUrl[] = "https://example.com/launch";
const char kAppTitle[] = "app";
const char kAppTitle2[] = "app2";

class FakePublisher final : public apps::AppPublisher {
 public:
  FakePublisher(apps::AppServiceProxy* proxy, apps::AppType app_type)
      : AppPublisher(proxy) {
    RegisterPublisher(app_type);
  }

  MOCK_METHOD4(Launch,
               void(const std::string& app_id,
                    int32_t event_flags,
                    apps::LaunchSource launch_source,
                    apps::WindowInfoPtr window_info));

  MOCK_METHOD2(LaunchAppWithParams,
               void(apps::AppLaunchParams&& params,
                    apps::LaunchCallback callback));

  void LoadIcon(const std::string& app_id,
                const apps::IconKey& icon_key,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                apps::LoadIconCallback callback) override {
    auto icon = std::make_unique<apps::IconValue>();
    icon->icon_type = apps::IconType::kUncompressed;
    icon->uncompressed = gfx::ImageSkia::CreateFrom1xBitmap(
        web_app::CreateSquareIcon(kWebKioskIconSize, SK_ColorWHITE));
    icon->is_placeholder_icon = false;
    std::move(callback).Run(std::move(icon));
  }
};

}  // namespace

class WebKioskAppUpdateObserverTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    account_id_ = AccountId::FromUserEmail(kAppEmail);

    app_service_test_.UninstallAllApps(profile());
    app_service_test_.SetUp(profile());
    app_service_test_.WaitForAppService();
    app_service_ = apps::AppServiceProxyFactory::GetForProfile(profile());

    app_publisher_ =
        std::make_unique<FakePublisher>(app_service_, apps::AppType::kWeb);

    app_manager_ = std::make_unique<WebKioskAppManager>();

    app_update_observer_ =
        std::make_unique<WebKioskAppUpdateObserver>(profile(), account_id_);
  }

  void TearDown() override {
    app_update_observer_.reset();
    app_manager_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  apps::AppPtr CreateTestApp() {
    auto app = std::make_unique<apps::App>(apps::AppType::kWeb, kAppId);
    app->app_id = kAppId;
    app->app_type = apps::AppType::kWeb;
    app->install_reason = apps::InstallReason::kKiosk;
    app->readiness = apps::Readiness::kReady;
    app->name = kAppTitle;
    app->publisher_id = kAppLaunchUrl;
    return app;
  }

  AccountId& account_id() { return account_id_; }

  apps::AppServiceProxy* app_service() { return app_service_; }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

  WebKioskAppManager* app_manager() { return app_manager_.get(); }

  const WebKioskAppData* app_data() {
    return app_manager_->GetAppByAccountId(account_id_);
  }

  WebKioskAppUpdateObserver* app_update_observer() {
    return app_update_observer_.get();
  }

 private:
  AccountId account_id_;

  apps::AppServiceTest app_service_test_;
  apps::AppServiceProxy* app_service_ = nullptr;

  std::unique_ptr<FakePublisher> app_publisher_;

  std::unique_ptr<WebKioskAppManager> app_manager_;

  std::unique_ptr<WebKioskAppUpdateObserver> app_update_observer_;
};

TEST_F(WebKioskAppUpdateObserverTest, ShouldUpdateAppInfoWithIconWhenReady) {
  app_manager()->AddAppForTesting(account_id(), GURL(kAppInstallUrl));
  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_NE(app_data()->name(), kAppTitle);
  EXPECT_NE(app_data()->launch_url(), kAppLaunchUrl);

  // Initial app info without icon.
  {
    base::RunLoop loop;
    std::vector<apps::AppPtr> apps;
    apps.push_back(CreateTestApp());
    app_service()->OnApps(std::move(apps), apps::AppType::kWeb,
                          /*should_notify_initialized=*/true);
    loop.RunUntilIdle();
  }

  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInstalled);
  EXPECT_EQ(app_data()->name(), kAppTitle);
  EXPECT_EQ(app_data()->launch_url(), kAppLaunchUrl);
  EXPECT_TRUE(app_data()->icon().isNull());

  // Update app info.
  {
    base::RunLoop loop;
    std::vector<apps::AppPtr> apps;
    apps.push_back(CreateTestApp());
    apps[0]->name = kAppTitle2;
    app_service()->OnApps(std::move(apps), apps::AppType::kWeb,
                          /*should_notify_initialized=*/true);
    loop.RunUntilIdle();
  }

  EXPECT_EQ(app_data()->name(), kAppTitle2);

  // Update app icon.
  {
    base::RunLoop loop;
    std::vector<apps::AppPtr> apps;
    apps.push_back(CreateTestApp());
    apps[0]->icon_key = apps::IconKey();
    app_service()->OnApps(std::move(apps), apps::AppType::kWeb,
                          /*should_notify_initialized=*/true);
    loop.RunUntilIdle();
  }

  EXPECT_FALSE(app_data()->icon().isNull());
  EXPECT_EQ(app_data()->icon().width(), kWebKioskIconSize);
  EXPECT_EQ(app_data()->icon().height(), kWebKioskIconSize);
}

TEST_F(WebKioskAppUpdateObserverTest, ShouldNotUpdateAppInfoWhenNotReady) {
  app_manager()->AddAppForTesting(account_id(), GURL(kAppInstallUrl));
  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_NE(app_data()->name(), kAppTitle);
  EXPECT_NE(app_data()->launch_url(), kAppLaunchUrl);

  {
    base::RunLoop loop;
    std::vector<apps::AppPtr> apps;
    apps.push_back(CreateTestApp());
    apps[0]->readiness = apps::Readiness::kUnknown;
    app_service()->OnApps(std::move(apps), apps::AppType::kWeb,
                          /*should_notify_initialized=*/true);
    loop.RunUntilIdle();
  }

  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_NE(app_data()->name(), kAppTitle);
  EXPECT_NE(app_data()->launch_url(), kAppLaunchUrl);
}

TEST_F(WebKioskAppUpdateObserverTest, ShouldNotUpdateAppInfoForNonWebApps) {
  app_manager()->AddAppForTesting(account_id(), GURL(kAppInstallUrl));
  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_NE(app_data()->name(), kAppTitle);
  EXPECT_NE(app_data()->launch_url(), kAppLaunchUrl);

  {
    base::RunLoop loop;
    std::vector<apps::AppPtr> apps;
    apps.push_back(CreateTestApp());
    apps[0]->app_type = apps::AppType::kChromeApp;
    app_service()->OnApps(std::move(apps), apps::AppType::kChromeApp,
                          /*should_notify_initialized=*/true);
    loop.RunUntilIdle();
  }

  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_NE(app_data()->name(), kAppTitle);
  EXPECT_NE(app_data()->launch_url(), kAppLaunchUrl);
}

TEST_F(WebKioskAppUpdateObserverTest, ShouldNotUpdateAppInfoForNonKioskApps) {
  app_manager()->AddAppForTesting(account_id(), GURL(kAppInstallUrl));
  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_NE(app_data()->name(), kAppTitle);
  EXPECT_NE(app_data()->launch_url(), kAppLaunchUrl);

  {
    base::RunLoop loop;
    std::vector<apps::AppPtr> apps;
    apps.push_back(CreateTestApp());
    apps[0]->install_reason = apps::InstallReason::kPolicy;
    app_service()->OnApps(std::move(apps), apps::AppType::kWeb,
                          /*should_notify_initialized=*/true);
    loop.RunUntilIdle();
  }

  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_NE(app_data()->name(), kAppTitle);
  EXPECT_NE(app_data()->launch_url(), kAppLaunchUrl);
}

}  // namespace ash
