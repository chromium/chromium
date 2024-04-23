// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/services/app_service/public/cpp/app.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using ::testing::_;

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0);

constexpr apps::AppType kTestAppType = apps::AppType::kChromeApp;
constexpr char kTestAppId[] = "abcdefghabcdefghabcdefghabcdefgh";

class FakePublisher final : public apps::AppPublisher {
 public:
  FakePublisher(apps::AppServiceProxy* proxy, apps::AppType app_type)
      : AppPublisher(proxy) {
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    RegisterPublisher(app_type);
#endif
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
    app_service_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void UpdateAppState(const char* app_id, apps::InstanceState state) {
    apps::InstanceParams params(app_id, /*window=*/nullptr);
    params.state = std::make_pair(state, base::Time::Now());
    app_service_->InstanceRegistry().CreateOrUpdateInstance(std::move(params));
  }
#endif

  void UpdateAppReadiness(apps::Readiness readiness) {
    std::vector<apps::AppPtr> apps;
    auto app = std::make_unique<apps::App>(kTestAppType, kTestAppId);
    app->app_id = kTestAppId;
    app->app_type = kTestAppType;
    app->readiness = readiness;
    apps.push_back(std::move(app));
    app_service_->OnApps(std::move(apps), kTestAppType,
                         /*should_notify_initialized=*/false);
  }

  apps::AppServiceTest app_service_test_;
  raw_ptr<apps::AppServiceProxy> app_service_ = nullptr;

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(KioskAppServiceLauncherTest, ShouldInvokeVisibleCallback) {
  base::MockOnceCallback<void()> visible_callback;

  launcher_->CheckAndMaybeLaunchApp(kTestAppId, base::DoNothing(),
                                    visible_callback.Get());

  EXPECT_CALL(visible_callback, Run()).Times(1);

  UpdateAppState(kTestAppId, apps::InstanceState::kVisible);
  base::RunLoop().RunUntilIdle();
}

TEST_F(KioskAppServiceLauncherTest,
       ShouldNotInvokeVisibleCallbackForOtherAppStates) {
  base::MockOnceCallback<void()> visible_callback;

  launcher_->CheckAndMaybeLaunchApp(kTestAppId, base::DoNothing(),
                                    visible_callback.Get());

  EXPECT_NO_CALLS(visible_callback, Run);
  UpdateAppState(kTestAppId, apps::InstanceState::kHidden);

  base::RunLoop().RunUntilIdle();
}

TEST_F(KioskAppServiceLauncherTest,
       ShouldNotInvokeVisibleCallbackForOtherApps) {
  base::MockOnceCallback<void()> visible_callback;

  launcher_->CheckAndMaybeLaunchApp(kTestAppId, base::DoNothing(),
                                    visible_callback.Get());

  EXPECT_NO_CALLS(visible_callback, Run);
  UpdateAppState("AnotherAppId", apps::InstanceState::kVisible);

  base::RunLoop().RunUntilIdle();
}

TEST_F(KioskAppServiceLauncherTest,
       ShouldInvokeLaunchedCallbackFirstIfAppBecomesVisible) {
  ::testing::InSequence enforce_call_order;

  base::MockOnceCallback<void()> visible_callback;
  base::MockOnceCallback<void(bool)> launched_callback;

  launcher_->CheckAndMaybeLaunchApp(kTestAppId, launched_callback.Get(),
                                    visible_callback.Get());

  EXPECT_CALL(launched_callback, Run(true)).Times(1);
  EXPECT_CALL(visible_callback, Run).Times(1);

  UpdateAppState(kTestAppId, apps::InstanceState::kVisible);

  base::RunLoop().RunUntilIdle();
}

TEST_F(KioskAppServiceLauncherTest,
       ShouldNotInvokeLaunchedCallbackTwiceIfAppBecomesVisible) {
  base::MockOnceCallback<void()> visible_callback;
  base::MockOnceCallback<void(bool)> launched_callback;

  launcher_->CheckAndMaybeLaunchApp(kTestAppId, launched_callback.Get(),
                                    visible_callback.Get());

  // Launched callback is invoked when the app is ready...
  EXPECT_CALL(launched_callback, Run(true)).Times(1);
  UpdateAppReadiness(apps::Readiness::kReady);
  base::RunLoop().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&launched_callback);

  // So it should not be invoked a second time when the app becomes visible.
  EXPECT_NO_CALLS(launched_callback, Run);
  EXPECT_CALL(visible_callback, Run).Times(1);

  UpdateAppState(kTestAppId, apps::InstanceState::kVisible);
  base::RunLoop().RunUntilIdle();
}
#endif

}  // namespace chromeos
