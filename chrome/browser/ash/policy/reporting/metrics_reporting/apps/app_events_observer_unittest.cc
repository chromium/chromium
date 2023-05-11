// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_events_observer.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_platform_metrics_retriever.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kTestAppId[] = "TestApp";

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

// Mock retriever for the `AppPlatformMetrics` component.
class MockAppPlatformMetricsRetriever : public AppPlatformMetricsRetriever {
 public:
  MockAppPlatformMetricsRetriever() : AppPlatformMetricsRetriever(nullptr) {}
  MockAppPlatformMetricsRetriever(const MockAppPlatformMetricsRetriever&) =
      delete;
  MockAppPlatformMetricsRetriever& operator=(
      const MockAppPlatformMetricsRetriever&) = delete;
  ~MockAppPlatformMetricsRetriever() override = default;

  MOCK_METHOD(void,
              GetAppPlatformMetrics,
              (AppPlatformMetricsCallback callback),
              (override));
};

class AppEventsObserverTest : public ::apps::AppPlatformMetricsServiceTestBase {
 protected:
  void SetUp() override {
    ::apps::AppPlatformMetricsServiceTestBase::SetUp();

    // Pre-install app so it can be used by tests.
    InstallOneApp(kTestAppId, ::apps::AppType::kArc, /*publisher_id=*/"",
                  ::apps::Readiness::kReady, ::apps::InstallSource::kPlayStore);

    // Set up `AppEventsObserver` with relevant test params.
    auto mock_app_platform_metrics_retriever =
        std::make_unique<MockAppPlatformMetricsRetriever>();
    EXPECT_CALL(*mock_app_platform_metrics_retriever, GetAppPlatformMetrics(_))
        .WillOnce([this](AppPlatformMetricsRetriever::AppPlatformMetricsCallback
                             callback) {
          std::move(callback).Run(
              app_platform_metrics_service()->AppPlatformMetrics());
        });
    app_events_observer_ = std::make_unique<AppEventsObserver>(
        std::move(mock_app_platform_metrics_retriever), &reporting_settings_);
  }

  void TearDown() override {
    app_events_observer_.reset();
    ::apps::AppPlatformMetricsServiceTestBase::TearDown();
  }

  void SetAllowedAppReportingTypes(const std::vector<std::string>& app_types) {
    base::Value::List allowed_app_types;
    for (const auto& app_type : app_types) {
      allowed_app_types.Append(app_type);
    }
    reporting_settings_.SetList(::ash::reporting::kReportAppInventory,
                                std::move(allowed_app_types));

    // Simulate policy update.
    bool is_app_reporting_enabled = !app_types.empty();
    app_events_observer_->SetReportingEnabled(is_app_reporting_enabled);
  }

  test::FakeReportingSettings reporting_settings_;
  std::unique_ptr<AppEventsObserver> app_events_observer_;
};

TEST_F(AppEventsObserverTest, OnAppInstalled) {
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryBrowser});
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Install new app.
  static constexpr char app_id[] = "TestNewApp";
  InstallOneApp(app_id, ::apps::AppType::kStandaloneBrowser,
                /*publisher_id=*/"", ::apps::Readiness::kReady,
                ::apps::InstallSource::kBrowser);

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
  EXPECT_THAT(app_install_data.app_type(),
              Eq(::apps::ApplicationType::APPLICATION_TYPE_STANDALONE_BROWSER));
  EXPECT_THAT(
      app_install_data.app_install_reason(),
      Eq(::apps::ApplicationInstallReason::APPLICATION_INSTALL_REASON_USER));
  EXPECT_THAT(
      app_install_data.app_install_source(),
      Eq(::apps::ApplicationInstallSource::APPLICATION_INSTALL_SOURCE_BROWSER));
  EXPECT_THAT(
      app_install_data.app_install_time(),
      Eq(::apps::ApplicationInstallTime::APPLICATION_INSTALL_TIME_INIT));
}

TEST_F(AppEventsObserverTest, OnAppInstalled_UnsetPolicy) {
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Install new app.
  static constexpr char app_id[] = "TestNewApp";
  InstallOneApp(app_id, ::apps::AppType::kStandaloneBrowser,
                /*publisher_id=*/"", ::apps::Readiness::kReady,
                ::apps::InstallSource::kBrowser);

  // Verify no data is being reported.
  ASSERT_TRUE(test_event.no_result());
}

TEST_F(AppEventsObserverTest, OnAppInstalled_DisallowedAppType) {
  // Set policy to enable reporting for a different app type than the one being
  // tested.
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryAndroidApps});
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Install new app.
  static constexpr char app_id[] = "TestNewApp";
  InstallOneApp(app_id, ::apps::AppType::kStandaloneBrowser,
                /*publisher_id=*/"", ::apps::Readiness::kReady,
                ::apps::InstallSource::kBrowser);

  // Verify no data is being reported.
  ASSERT_TRUE(test_event.no_result());
}

TEST_F(AppEventsObserverTest, OnAppLaunched) {
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryAndroidApps});
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Simulate app launch for pre-installed app.
  auto* const proxy = ::apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  FakePublisher fake_publisher(proxy, ::apps::AppType::kArc);
  proxy->Launch(kTestAppId, ui::EF_NONE, apps::LaunchSource::kFromCommandLine,
                nullptr);

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
}

TEST_F(AppEventsObserverTest, OnAppLaunched_UnsetPolicy) {
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Simulate app launch for pre-installed app.
  auto* const proxy = ::apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  FakePublisher fake_publisher(proxy, ::apps::AppType::kArc);
  proxy->Launch(kTestAppId, ui::EF_NONE, apps::LaunchSource::kFromCommandLine,
                nullptr);

  // Verify no data is being reported.
  ASSERT_TRUE(test_event.no_result());
}

TEST_F(AppEventsObserverTest, OnAppLaunched_DisallowedAppType) {
  // Set policy to enable reporting for a different app type than the one being
  // tested.
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryGames});
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Simulate app launch for pre-installed app.
  auto* const proxy = ::apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  FakePublisher fake_publisher(proxy, ::apps::AppType::kArc);
  proxy->Launch(kTestAppId, ui::EF_NONE, apps::LaunchSource::kFromCommandLine,
                nullptr);

  // Verify no data is being reported.
  ASSERT_TRUE(test_event.no_result());
}

TEST_F(AppEventsObserverTest, OnAppUninstalled) {
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryAndroidApps});
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Simulate app uninstall for pre-installed app.
  auto* const proxy = ::apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  FakePublisher fake_publisher(proxy, ::apps::AppType::kArc);
  proxy->UninstallSilently(kTestAppId, ::apps::UninstallSource::kAppList);

  // Verify data being reported.
  const MetricData& result = test_event.result();
  ASSERT_TRUE(result.has_event_data());
  EXPECT_THAT(result.event_data().type(), Eq(MetricEventType::APP_UNINSTALLED));
  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_app_telemetry());
  ASSERT_TRUE(result.telemetry_data().app_telemetry().has_app_uninstall_data());

  const AppUninstallData& app_uninstall_data =
      result.telemetry_data().app_telemetry().app_uninstall_data();
  EXPECT_THAT(app_uninstall_data.app_id(), StrEq(kTestAppId));
  EXPECT_THAT(app_uninstall_data.app_type(),
              Eq(::apps::ApplicationType::APPLICATION_TYPE_ARC));
  EXPECT_THAT(app_uninstall_data.app_uninstall_source(),
              Eq(::apps::ApplicationUninstallSource::
                     APPLICATION_UNINSTALL_SOURCE_APP_LIST));
}

TEST_F(AppEventsObserverTest, OnAppUninstalled_UnsetPolicy) {
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Simulate app uninstall for pre-installed app.
  auto* const proxy = ::apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  FakePublisher fake_publisher(proxy, ::apps::AppType::kArc);
  proxy->UninstallSilently(kTestAppId, ::apps::UninstallSource::kAppList);

  // Verify no data is being reported.
  ASSERT_TRUE(test_event.no_result());
}

TEST_F(AppEventsObserverTest, OnAppUninstalled_DisallowedAppType) {
  // Set policy to enable reporting for a different app type than the one being
  // tested.
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryGames});
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Simulate app uninstall for pre-installed app.
  auto* const proxy = ::apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  FakePublisher fake_publisher(proxy, ::apps::AppType::kArc);
  proxy->UninstallSilently(kTestAppId, ::apps::UninstallSource::kAppList);

  // Verify no data is being reported.
  ASSERT_TRUE(test_event.no_result());
}

TEST_F(AppEventsObserverTest, OnAppPlatformMetricsDestroyed) {
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryBrowser});
  test::TestEvent<MetricData> test_event;
  app_events_observer_->SetOnEventObservedCallback(test_event.repeating_cb());

  // Reset `AppPlatformMetricsService` to destroy the `AppPlatformMetrics`
  // component.
  ResetAppPlatformMetricsService();

  // Verify observer is unregistered by attempting to install an app and no
  // metric data being reported.
  static constexpr char app_id[] = "TestNewApp";
  InstallOneApp(app_id, ::apps::AppType::kStandaloneBrowser,
                /*publisher_id=*/"", ::apps::Readiness::kReady,
                ::apps::InstallSource::kBrowser);
  ASSERT_TRUE(test_event.no_result());
}

}  // namespace
}  // namespace reporting
