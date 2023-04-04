// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "url/gurl.h"

using ::testing::Eq;
using ::testing::StrEq;

namespace reporting {
namespace {

// Test DM token used to associate reported events.
constexpr char kDMToken[] = "token";

// Standalone webapp start URL.
constexpr char kWebAppUrl[] = "https://test.example.com";

// Assert event data in a record with relevant DM token and returns the
// underlying `MetricData` object.
const MetricData AssertEvent(Priority priority, const Record& record) {
  EXPECT_THAT(priority, Eq(Priority::SLOW_BATCH));
  EXPECT_THAT(record.destination(), Eq(Destination::EVENT_METRIC));

  MetricData record_data;
  EXPECT_TRUE(record_data.ParseFromString(record.data()));
  EXPECT_TRUE(record_data.has_timestamp_ms());
  EXPECT_TRUE(record_data.has_event_data());
  EXPECT_TRUE(record_data.has_telemetry_data());
  EXPECT_TRUE(record_data.telemetry_data().has_app_telemetry());
  EXPECT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kDMToken));
  return record_data;
}

// Returns true if the record includes the specified metric event type. False
// otherwise.
bool IsMetricEventOfType(MetricEventType metric_event_type,
                         const Record& record) {
  MetricData record_data;
  return record_data.ParseFromString(record.data()) &&
         record_data.has_event_data() &&
         (record_data.event_data().type() == metric_event_type);
}

// Browser test that validates events collected and reported by the
// `AppEventsObserver`. Inheriting from `DevicePolicyCrosBrowserTest` enables
// use of `AffiliationMixin` for setting up profile/device affiliation. Only
// available in Ash.
class AppEventsObserverBrowserTest
    : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  AppEventsObserverBrowserTest() {
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    scoped_feature_list_.InitAndEnableFeature(kEnableAppMetricsReporting);
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidTokenForTesting(kDMToken));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ::policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    SetPolicyEnabled(true);
    if (content::IsPreTest()) {
      // Preliminary setup - set up affiliated user.
      ::policy::AffiliationTestHelper::PreLoginUser(
          affiliation_mixin_.account_id());
      return;
    }

    // Login as affiliated user otherwise.
    ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  }

  // Helper that installs a standalone webapp with the specified start url.
  ::web_app::AppId InstallStandaloneWebApp(const GURL& start_url) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = ::blink::mojom::DisplayMode::kStandalone;
    web_app_info->user_display_mode =
        ::web_app::mojom::UserDisplayMode::kStandalone;
    return ::web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  // Helper that uninstalls the standalone webapp with the specified app id.
  void UninstallStandaloneWebApp(const ::web_app::AppId& app_id) {
    ::apps::AppServiceProxyFactory::GetForProfile(profile())->UninstallSilently(
        app_id, ::apps::UninstallSource::kAppList);
  }

  void SetPolicyEnabled(bool is_enabled) {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceAppInfo, is_enabled);
  }

  Profile* profile() const {
    return ash::ProfileHelper::Get()->GetProfileByAccountId(
        affiliation_mixin_.account_id());
  }

  ::policy::DevicePolicyCrosTestHelper test_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ::ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};

  base::test::ScopedFeatureList scoped_feature_list_;
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, PRE_ReportInstalledApp) {
  // Dummy case that sets up the affiliated user through SetUpOnMainThread
  // PRE-condition.
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, ReportInstalledApp) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableAppMetricsReporting));
  ::chromeos::MissiveClientTestObserver missive_observer(base::BindRepeating(
      &IsMetricEventOfType, MetricEventType::APP_INSTALLED));
  const auto& app_id = InstallStandaloneWebApp(GURL(kWebAppUrl));
  const auto& [priority, record] = missive_observer.GetNextEnqueuedRecord();
  const auto& metric_data = AssertEvent(priority, record);
  ASSERT_TRUE(
      metric_data.telemetry_data().app_telemetry().has_app_install_data());
  const auto& app_install_data =
      metric_data.telemetry_data().app_telemetry().app_install_data();
  EXPECT_THAT(app_install_data.app_id(), StrEq(app_id));
  EXPECT_THAT(app_install_data.app_type(),
              Eq(::apps::ApplicationType::APPLICATION_TYPE_WEB));
  EXPECT_THAT(
      app_install_data.app_install_source(),
      Eq(::apps::ApplicationInstallSource::APPLICATION_INSTALL_SOURCE_BROWSER));
  EXPECT_THAT(
      app_install_data.app_install_reason(),
      Eq(::apps::ApplicationInstallReason::APPLICATION_INSTALL_REASON_SYNC));
  EXPECT_THAT(
      app_install_data.app_install_time(),
      Eq(::apps::ApplicationInstallTime::APPLICATION_INSTALL_TIME_RUNNING));
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, PRE_ReportLaunchedApp) {
  // Dummy case that sets up the affiliated user through SetUpOnMainThread
  // PRE-condition.
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, ReportLaunchedApp) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableAppMetricsReporting));
  const auto& app_id = InstallStandaloneWebApp(GURL(kWebAppUrl));
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsMetricEventOfType, MetricEventType::APP_LAUNCHED));
  ::web_app::LaunchWebAppBrowser(profile(), app_id);
  const auto& [priority, record] = missive_observer.GetNextEnqueuedRecord();
  const auto& metric_data = AssertEvent(priority, record);
  ASSERT_TRUE(
      metric_data.telemetry_data().app_telemetry().has_app_launch_data());
  const auto& app_launch_data =
      metric_data.telemetry_data().app_telemetry().app_launch_data();
  EXPECT_THAT(
      app_launch_data.app_launch_source(),
      Eq(::apps::ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_TEST));
  EXPECT_THAT(app_launch_data.app_id(), StrEq(app_id));
  EXPECT_THAT(app_launch_data.app_type(),
              Eq(::apps::ApplicationType::APPLICATION_TYPE_WEB));
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, PRE_ReportUninstalledApp) {
  // Dummy case that sets up the affiliated user through SetUpOnMainThread
  // PRE-condition.
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, ReportUninstalledApp) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableAppMetricsReporting));
  const auto& app_id = InstallStandaloneWebApp(GURL(kWebAppUrl));
  ::chromeos::MissiveClientTestObserver missive_observer(base::BindRepeating(
      &IsMetricEventOfType, MetricEventType::APP_UNINSTALLED));
  UninstallStandaloneWebApp(app_id);
  const auto& [priority, record] = missive_observer.GetNextEnqueuedRecord();
  const auto& metric_data = AssertEvent(priority, record);
  ASSERT_TRUE(
      metric_data.telemetry_data().app_telemetry().has_app_uninstall_data());
  const auto& app_uninstall_data =
      metric_data.telemetry_data().app_telemetry().app_uninstall_data();
  EXPECT_THAT(app_uninstall_data.app_uninstall_source(),
              Eq(::apps::ApplicationUninstallSource::
                     APPLICATION_UNINSTALL_SOURCE_APP_LIST));
  EXPECT_THAT(app_uninstall_data.app_id(), StrEq(app_id));
  EXPECT_THAT(app_uninstall_data.app_type(),
              Eq(::apps::ApplicationType::APPLICATION_TYPE_WEB));
}

}  // namespace
}  // namespace reporting
