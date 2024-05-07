// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_update_observer.h"

#include <sys/types.h>

#include <memory>
#include <vector>

#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

using base::test::TestFuture;
using testing::Invoke;

const char kAppId[] = "testappid";
const char kAppEmail[] = "test@example.com";
const char kAppInstallUrl[] = "https://example.com";
const char kAppLaunchUrl[] = "https://example.com/launch";
const char kAppTitle[] = "app";

void UpdateWebApp(apps::AppServiceProxy* app_service, const apps::AppPtr& app) {
  std::vector<apps::AppPtr> apps;
  apps.push_back(app->Clone());
  app_service->OnApps(std::move(apps), apps::AppType::kWeb,
                      /*should_notify_initialized=*/true);
}

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

class FakeKioskAppManagerObserver : public KioskAppManagerObserver {
 public:
  FakeKioskAppManagerObserver() = default;
  FakeKioskAppManagerObserver(const FakeKioskAppManagerObserver&) = delete;
  FakeKioskAppManagerObserver& operator=(const FakeKioskAppManagerObserver&) =
      delete;
  ~FakeKioskAppManagerObserver() override = default;

  // `KioskAppManagerObserver` implementation:
  void OnKioskAppDataChanged(const std::string& app_id) override {
    change_waiter_.SetValue(app_id);
  }

  void WaitForAppDataChange() { std::ignore = change_waiter_.Take(); }
  bool HasAppDataChange() const { return change_waiter_.IsReady(); }

 private:
  base::test::TestFuture<std::string> change_waiter_;
};

}  // namespace

class WebKioskAppManagerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    account_id_ = AccountId::FromUserEmail(kAppEmail);

    app_service_test_.UninstallAllApps(profile());
    app_service_test_.SetUp(profile());
    app_service_ = apps::AppServiceProxyFactory::GetForProfile(profile());

    // `WebKioskAppUpdateObserver` requires WebAppProvider to be ready before it
    // is created.
    fake_web_app_provider_ = web_app::FakeWebAppProvider::Get(profile());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    app_publisher_ =
        std::make_unique<FakePublisher>(app_service_, apps::AppType::kWeb);

    app_manager_ = std::make_unique<WebKioskAppManager>();

    app_manager()->StartObservingAppUpdate(profile(), account_id());
    app_manager()->AddObserver(&app_manager_observer_);
    app_manager()->AddAppForTesting(account_id(), GURL(kAppInstallUrl));
  }

  void TearDown() override {
    app_manager_->RemoveObserver(&app_manager_observer_);
    app_manager_.reset();
    fake_web_app_provider_->Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  apps::AppPtr CreateTestApp() {
    auto app = std::make_unique<apps::App>(apps::AppType::kWeb, kAppId);
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

  web_app::WebAppProvider* web_app_provider() {
    return web_app::WebAppProvider::GetForTest(profile());
  }

  web_app::WebAppSyncBridge& sync_bridge() {
    return web_app_provider()->sync_bridge_unsafe();
  }

  const WebKioskAppData* app_data() {
    return app_manager_->GetAppByAccountId(account_id_);
  }

  FakeKioskAppManagerObserver& app_manager_observer() {
    return app_manager_observer_;
  }

  // Ensure no app update is received (so nothing changes).
  void ExpectNoAppDataChange() {
    // The only way to check that we did not receive an app update is to flush
    // everything and then check if any calls arrived.
    task_environment()->RunUntilIdle();
    EXPECT_FALSE(app_manager_observer().HasAppDataChange());
  }

  void WaitForAppDataChange() { app_manager_observer().WaitForAppDataChange(); }

 private:
  AccountId account_id_;

  apps::AppServiceTest app_service_test_;
  raw_ptr<apps::AppServiceProxy, DanglingUntriaged> app_service_ = nullptr;

  // A keyed service not owned by this class.
  raw_ptr<web_app::FakeWebAppProvider, DanglingUntriaged>
      fake_web_app_provider_;

  std::unique_ptr<FakePublisher> app_publisher_;

  std::unique_ptr<WebKioskAppManager> app_manager_;

  FakeKioskAppManagerObserver app_manager_observer_;
};

TEST_F(WebKioskAppManagerTest, ShouldUpdateAppInfoWhenReady) {
  apps::AppPtr app = CreateTestApp();
  app->name = kAppTitle;
  app->publisher_id = kAppLaunchUrl;
  app->icon_key = std::nullopt;

  UpdateWebApp(app_service(), app);
  WaitForAppDataChange();

  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInstalled);
  EXPECT_EQ(app_data()->name(), kAppTitle);
  EXPECT_EQ(app_data()->launch_url(), kAppLaunchUrl);
  EXPECT_TRUE(app_data()->icon().isNull());
}

TEST_F(WebKioskAppManagerTest, ShouldUpdateAppInfoOnConsecutiveChanges) {
  apps::AppPtr app = CreateTestApp();
  app->name = "initial-name";

  // Send first update with initial (default) app information.
  UpdateWebApp(app_service(), app);
  WaitForAppDataChange();
  ASSERT_EQ(app_data()->name(), "initial-name");

  app->name = "new-name";
  app->publisher_id = "http://new-launch-url.com";
  UpdateWebApp(app_service(), app);
  WaitForAppDataChange();

  EXPECT_EQ(app_data()->name(), "new-name");
  EXPECT_EQ(app_data()->launch_url(), GURL("http://new-launch-url.com"));
}

TEST_F(WebKioskAppManagerTest, ShouldUpdateAppInfoWithIconWhenReady) {
  // Initial app info without icon.
  apps::AppPtr app = CreateTestApp();
  app->icon_key = std::nullopt;

  UpdateWebApp(app_service(), app);
  WaitForAppDataChange();
  ASSERT_TRUE(app_data()->icon().isNull());

  app->icon_key = apps::IconKey();
  UpdateWebApp(app_service(), app);
  WaitForAppDataChange();

  EXPECT_FALSE(app_data()->icon().isNull());
  EXPECT_EQ(app_data()->icon().width(), kWebKioskIconSize);
  EXPECT_EQ(app_data()->icon().height(), kWebKioskIconSize);
}

TEST_F(WebKioskAppManagerTest, ShouldNotUpdateAppInfoWhenNotReady) {
  apps::AppPtr app = CreateTestApp();
  app->name = "<new-name>";

  app->readiness = apps::Readiness::kUnknown;

  UpdateWebApp(app_service(), app);

  ExpectNoAppDataChange();
  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_NE(app_data()->name(), "<new-name>");
}

TEST_F(WebKioskAppManagerTest, ShouldNotUpdateAppInfoForNonWebApps) {
  apps::AppPtr app = CreateTestApp();
  app->name = "<new-name>";

  app->app_type = apps::AppType::kChromeApp;

  UpdateWebApp(app_service(), app);

  ExpectNoAppDataChange();
  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_NE(app_data()->name(), "<new-name>");
}

TEST_F(WebKioskAppManagerTest, ShouldNotUpdateAppInfoForNonKioskApps) {
  apps::AppPtr app = CreateTestApp();
  app->name = "<new-name>";

  app->install_reason = apps::InstallReason::kPolicy;

  UpdateWebApp(app_service(), app);

  ExpectNoAppDataChange();
  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_NE(app_data()->name(), "<new-name>");
}

TEST_F(WebKioskAppManagerTest, ShouldNotUpdateAppInfoForPlaceholders) {
  // Install app as placeholder.
  auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL(kAppLaunchUrl));
  app_info->scope = GURL(kAppInstallUrl);
  app_info->title = u"placeholder_title";
  app_info->is_placeholder = true;

  web_app::test::InstallWebApp(profile(), std::move(app_info), true,
                               webapps::WebappInstallSource::KIOSK);

  ExpectNoAppDataChange();
  EXPECT_NE(app_data()->name(), "placeholder_title");
}

}  // namespace ash
