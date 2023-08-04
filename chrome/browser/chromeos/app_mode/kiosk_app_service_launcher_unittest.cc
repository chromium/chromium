// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/app_types.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

using ::testing::_;

constexpr apps::AppType kTestAppType = apps::AppType::kChromeApp;
constexpr char kTestAppId[] = "abcdefghabcdefghabcdefghabcdefgh";

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

  void LaunchAppWithParams(apps::AppLaunchParams&& params,
                           apps::LaunchCallback callback) override {
    if (params.app_id == kTestAppId &&
        params.launch_source == apps::LaunchSource::kFromKiosk) {
      std::move(callback).Run(apps::LaunchResult());
    }
  }

  MOCK_METHOD6(LoadIcon,
               void(const std::string& app_id,
                    const apps::IconKey& icon_key,
                    apps::IconType icon_type,
                    int32_t size_hint_in_dip,
                    bool allow_placeholder_icon,
                    apps::LoadIconCallback callback));
};

}  // namespace

class KioskAppServiceLauncherTest : public BrowserWithTestWindowTest {
 public:
  KioskAppServiceLauncherTest() = default;

  // testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    app_service_test_.UninstallAllApps(profile());
    app_service_test_.SetUp(profile());
    app_service_ = apps::AppServiceProxyFactory::GetForProfile(profile());
    publisher_ = std::make_unique<FakePublisher>(app_service_, kTestAppType);
    launcher_ = std::make_unique<KioskAppServiceLauncher>(profile());
  }

  void TearDown() override {
    publisher_.reset();
    launcher_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  void UpdateAppReadiness(apps::Readiness readiness) {
    std::vector<apps::AppPtr> apps;
    auto app = std::make_unique<apps::App>(kTestAppType, kTestAppId);
    app->app_id = kTestAppId;
    app->app_type = kTestAppType;
    app->readiness = readiness;
    apps.push_back(std::move(app));
    app_service_->AppRegistryCache().OnApps(
        std::move(apps), kTestAppType, /*should_notify_initialized=*/false);
  }

  apps::AppServiceTest app_service_test_;
  raw_ptr<apps::AppServiceProxy, ExperimentalAsh> app_service_ = nullptr;

  std::unique_ptr<FakePublisher> publisher_;
  std::unique_ptr<KioskAppServiceLauncher> launcher_;
};

TEST_F(KioskAppServiceLauncherTest, ShouldFailIfAppInInvalidReadiness) {
  base::HistogramTester histogram;

  base::MockCallback<KioskAppServiceLauncher::AppLaunchedCallback>
      launched_callback;

  UpdateAppReadiness(apps::Readiness::kUninstalledByUser);
  EXPECT_CALL(launched_callback, Run(false)).Times(1);
  launcher_->CheckAndMaybeLaunchApp(kTestAppId, launched_callback.Get());

  histogram.ExpectUniqueSample(KioskAppServiceLauncher::kLaunchAppReadinessUMA,
                               apps::Readiness::kUninstalledByUser, 1);
}

TEST_F(KioskAppServiceLauncherTest, ShouldWaitIfAppNotExist) {
  base::HistogramTester histogram;

  base::MockCallback<KioskAppServiceLauncher::AppLaunchedCallback>
      launched_callback;

  EXPECT_CALL(launched_callback, Run(_)).Times(0);
  launcher_->CheckAndMaybeLaunchApp(kTestAppId, launched_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&launched_callback);

  EXPECT_CALL(launched_callback, Run(true)).Times(1);
  UpdateAppReadiness(apps::Readiness::kReady);

  histogram.ExpectUniqueSample(KioskAppServiceLauncher::kLaunchAppReadinessUMA,
                               apps::Readiness::kUnknown, 1);
}

TEST_F(KioskAppServiceLauncherTest, ShouldWaitIfAppNotReady) {
  base::HistogramTester histogram;

  base::MockCallback<KioskAppServiceLauncher::AppLaunchedCallback>
      launched_callback;

  UpdateAppReadiness(apps::Readiness::kUnknown);
  EXPECT_CALL(launched_callback, Run(_)).Times(0);
  launcher_->CheckAndMaybeLaunchApp(kTestAppId, launched_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&launched_callback);

  EXPECT_CALL(launched_callback, Run(true)).Times(1);
  UpdateAppReadiness(apps::Readiness::kReady);

  histogram.ExpectUniqueSample(KioskAppServiceLauncher::kLaunchAppReadinessUMA,
                               apps::Readiness::kUnknown, 1);
}

TEST_F(KioskAppServiceLauncherTest, ShouldLaunchIfAppReady) {
  base::HistogramTester histogram;

  base::MockCallback<KioskAppServiceLauncher::AppLaunchedCallback>
      launched_callback;

  UpdateAppReadiness(apps::Readiness::kReady);
  EXPECT_CALL(launched_callback, Run(true)).Times(1);
  launcher_->CheckAndMaybeLaunchApp(kTestAppId, launched_callback.Get());

  histogram.ExpectUniqueSample(KioskAppServiceLauncher::kLaunchAppReadinessUMA,
                               apps::Readiness::kReady, 1);
}

}  // namespace ash
