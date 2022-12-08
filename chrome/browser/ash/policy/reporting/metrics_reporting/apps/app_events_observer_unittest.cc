// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_events_observer.h"

#include <memory>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kTestAppId[] = "TestApp";

// Test delegate that stubs out `AppEventsObserver` interactions with the
// `AppServiceProxyFactory`.
class TestDelegate : public AppEventsObserver::Delegate {
 public:
  TestDelegate(bool is_app_service_available,
               ::apps::AppPlatformMetrics* app_platform_metrics)
      : is_app_service_available_(is_app_service_available),
        app_platform_metrics_(app_platform_metrics) {}
  TestDelegate(const TestDelegate& other) = delete;
  TestDelegate& operator=(const TestDelegate& other) = delete;
  ~TestDelegate() override = default;

  // AppEventsObserver::Delegate:
  bool IsAppServiceAvailableForProfile(Profile* profile) override {
    return is_app_service_available_;
  }

  // AppEventsObserver::Delegate:
  ::apps::AppPlatformMetrics* GetAppPlatformMetricsForProfile(
      Profile* profile) override {
    return app_platform_metrics_.get();
  }

 private:
  const bool is_app_service_available_;
  const raw_ptr<::apps::AppPlatformMetrics> app_platform_metrics_;
};

// Fake `AppPublisher` used by the test to simulate app launches.
class FakePublisher : public ::apps::AppPublisher {
 public:
  FakePublisher(::apps::AppServiceProxy* proxy, ::apps::AppType app_type)
      : ::apps::AppPublisher(proxy) {
    RegisterPublisher(app_type);
  }

  MOCK_METHOD(void,
              Launch,
              (const std::string& app_id,
               int32_t event_flags,
               ::apps::LaunchSource launch_source,
               ::apps::WindowInfoPtr window_info));

  MOCK_METHOD(void,
              LaunchAppWithParams,
              (::apps::AppLaunchParams && params,
               ::apps::LaunchCallback callback));

  MOCK_METHOD(void,
              LoadIcon,
              (const std::string& app_id,
               const ::apps::IconKey& icon_key,
               ::apps::IconType icon_type,
               int32_t size_hint_in_dip,
               bool allow_placeholder_icon,
               ::apps::LoadIconCallback callback));
};

class AppEventsObserverTest
    : public ::apps::AppPlatformMetricsServiceTestBase,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  void SetUp() override {
    ::apps::AppPlatformMetricsServiceTestBase::SetUp();

    // Pre-install app so it can be used by tests.
    InstallOneApp(kTestAppId, ::apps::AppType::kArc, /*publisher_id=*/"",
                  ::apps::Readiness::kReady, ::apps::InstallSource::kPlayStore);

    // Set up `AppEventsObserver` with relevant test params.
    auto delegate = std::make_unique<TestDelegate>(
        IsAppServiceAvailableForProfile(),
        app_platform_metrics_service()->AppPlatformMetrics());
    app_events_observer_ =
        AppEventsObserver::CreateForTest(profile(), std::move(delegate));
    app_events_observer_->SetReportingEnabled(IsReportingEnabled());
  }

  void TearDown() override {
    app_events_observer_.reset();
    ::apps::AppPlatformMetricsServiceTestBase::TearDown();
  }

  bool IsAppServiceAvailableForProfile() const {
    return ::testing::get<0>(GetParam());
  }

  bool IsReportingEnabled() const { return ::testing::get<1>(GetParam()); }

  std::unique_ptr<AppEventsObserver> app_events_observer_;
};

TEST_P(AppEventsObserverTest, OnAppInstalled) {
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Install new app.
  static constexpr char app_id[] = "TestNewApp";
  InstallOneApp(app_id, ::apps::AppType::kStandaloneBrowser,
                /*publisher_id=*/"", ::apps::Readiness::kReady,
                ::apps::InstallSource::kBrowser);

  if (IsAppServiceAvailableForProfile() && IsReportingEnabled()) {
    // Verify data being reported.
    const MetricData& result = test_event.result();
    ASSERT_TRUE(result.has_event_data());
    EXPECT_THAT(result.event_data().type(), Eq(MetricEventType::APP_INSTALLED));
    ASSERT_TRUE(result.has_telemetry_data());
    ASSERT_TRUE(result.telemetry_data().has_app_telemetry());
    ASSERT_TRUE(result.telemetry_data().app_telemetry().has_app_install_data());

    const AppInstallData& app_install_data =
        result.telemetry_data().app_telemetry().app_install_data();
    EXPECT_THAT(app_install_data.app_id(), StrEq(app_id));
    EXPECT_THAT(
        app_install_data.app_type(),
        Eq(::apps::ApplicationType::APPLICATION_TYPE_STANDALONE_BROWSER));
    EXPECT_THAT(
        app_install_data.app_install_reason(),
        Eq(::apps::ApplicationInstallReason::APPLICATION_INSTALL_REASON_USER));
    EXPECT_THAT(app_install_data.app_install_source(),
                Eq(::apps::ApplicationInstallSource::
                       APPLICATION_INSTALL_SOURCE_BROWSER));
    EXPECT_THAT(
        app_install_data.app_install_time(),
        Eq(::apps::ApplicationInstallTime::APPLICATION_INSTALL_TIME_INIT));
  } else {
    // Should not report any data if app service unavailable or reporting is
    // disabled for profile.
    ASSERT_TRUE(test_event.no_result());
  }
}

TEST_P(AppEventsObserverTest, OnAppLaunched) {
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Simulate app launch for pre-installed app.
  auto* const proxy = ::apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  FakePublisher fake_publisher(proxy, ::apps::AppType::kArc);
  proxy->Launch(kTestAppId, ui::EF_NONE, apps::LaunchSource::kFromCommandLine,
                nullptr);

  if (IsAppServiceAvailableForProfile() && IsReportingEnabled()) {
    // Verify data being reported.
    const MetricData& result = test_event.result();
    ASSERT_TRUE(result.has_event_data());
    EXPECT_THAT(result.event_data().type(), Eq(MetricEventType::APP_LAUNCHED));
    ASSERT_TRUE(result.has_telemetry_data());
    ASSERT_TRUE(result.telemetry_data().has_app_telemetry());
    ASSERT_TRUE(result.telemetry_data().app_telemetry().has_app_launch_data());

    const AppLaunchData& app_launch_data =
        result.telemetry_data().app_telemetry().app_launch_data();
    EXPECT_THAT(app_launch_data.app_id(), StrEq(kTestAppId));
    EXPECT_THAT(app_launch_data.app_type(),
                Eq(::apps::ApplicationType::APPLICATION_TYPE_ARC));
    EXPECT_THAT(app_launch_data.app_launch_source(),
                Eq(::apps::ApplicationLaunchSource::
                       APPLICATION_LAUNCH_SOURCE_COMMAND_LINE));
  } else {
    // Should not report any data if app service unavailable or reporting is
    // disabled for profile.
    ASSERT_TRUE(test_event.no_result());
  }
}

TEST_P(AppEventsObserverTest, OnAppUninstalled) {
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Simulate app uninstall for pre-installed app.
  auto* const proxy = ::apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  FakePublisher fake_publisher(proxy, ::apps::AppType::kArc);
  proxy->UninstallSilently(kTestAppId, ::apps::UninstallSource::kAppList);

  if (IsAppServiceAvailableForProfile() && IsReportingEnabled()) {
    // Verify data being reported.
    const MetricData& result = test_event.result();
    ASSERT_TRUE(result.has_event_data());
    EXPECT_THAT(result.event_data().type(),
                Eq(MetricEventType::APP_UNINSTALLED));
    ASSERT_TRUE(result.has_telemetry_data());
    ASSERT_TRUE(result.telemetry_data().has_app_telemetry());
    ASSERT_TRUE(
        result.telemetry_data().app_telemetry().has_app_uninstall_data());

    const AppUninstallData& app_uninstall_data =
        result.telemetry_data().app_telemetry().app_uninstall_data();
    EXPECT_THAT(app_uninstall_data.app_id(), StrEq(kTestAppId));
    EXPECT_THAT(app_uninstall_data.app_type(),
                Eq(::apps::ApplicationType::APPLICATION_TYPE_ARC));
    EXPECT_THAT(app_uninstall_data.app_uninstall_source(),
                Eq(::apps::ApplicationUninstallSource::
                       APPLICATION_UNINSTALL_SOURCE_APP_LIST));
  } else {
    // Should not report any data if app service unavailable or reporting is
    // disabled for profile.
    ASSERT_TRUE(test_event.no_result());
  }
}

INSTANTIATE_TEST_SUITE_P(
    AppEventsObserverTests,
    AppEventsObserverTest,
    ::testing::Combine(
        ::testing::Bool() /* true - app service available for profile*/,
        ::testing::Bool()) /* true - reporting enabled*/);

}  // namespace
}  // namespace reporting
