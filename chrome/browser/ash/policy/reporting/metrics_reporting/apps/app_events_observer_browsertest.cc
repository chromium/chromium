// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "url/gurl.h"

using ::testing::Contains;
using ::testing::Eq;
using ::testing::SizeIs;
using ::testing::StrEq;

namespace reporting {
namespace {

// Test DM token used to associate reported events.
constexpr char kDMToken[] = "token";

// Standalone webapp start URL.
constexpr char kWebAppUrl[] = "https://test.example.com/";

void AssertRecordData(Priority priority, const Record& record) {
  EXPECT_THAT(priority, Eq(Priority::SLOW_BATCH));
  ASSERT_TRUE(record.has_destination());
  EXPECT_THAT(record.destination(), Eq(Destination::EVENT_METRIC));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kDMToken));
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
}

void AssertMetricData(const MetricData& metric_data) {
  EXPECT_TRUE(metric_data.has_timestamp_ms());
  EXPECT_TRUE(metric_data.has_event_data());
  ASSERT_TRUE(metric_data.has_telemetry_data());
  EXPECT_TRUE(metric_data.telemetry_data().has_app_telemetry());
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
  AppEventsObserverBrowserTest()
      : scoped_feature_list_(kEnableAppEventsObserver) {
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    crypto_home_mixin_.ApplyAuthConfig(
        affiliation_mixin_.account_id(),
        ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidToken(kDMToken));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  // Helper that installs a standalone webapp with the specified start url.
  ::webapps::AppId InstallStandaloneWebApp(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = ::blink::mojom::DisplayMode::kStandalone;
    web_app_info->user_display_mode =
        ::web_app::mojom::UserDisplayMode::kStandalone;
    return ::web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  // Helper that uninstalls the standalone webapp with the specified app id.
  void UninstallStandaloneWebApp(const ::webapps::AppId& app_id) {
    ::apps::AppServiceProxyFactory::GetForProfile(profile())->UninstallSilently(
        app_id, ::apps::UninstallSource::kAppList);
  }

  void SetAllowedAppReportingTypes(const std::vector<std::string>& app_types) {
    base::Value::List allowed_app_types;
    for (const auto& app_type : app_types) {
      allowed_app_types.Append(app_type);
    }
    profile()->GetPrefs()->SetList(::ash::reporting::kReportAppInventory,
                                   std::move(allowed_app_types));
  }

  Profile* profile() const {
    return ash::ProfileHelper::Get()->GetProfileByAccountId(
        affiliation_mixin_.account_id());
  }

  ::policy::DevicePolicyCrosTestHelper test_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ::ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, PRE_ReportInstalledApp) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, ReportInstalledApp) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryPWA});

  ::chromeos::MissiveClientTestObserver missive_observer(base::BindRepeating(
      &IsMetricEventOfType, MetricEventType::APP_INSTALLED));
  const auto app_id = InstallStandaloneWebApp(GURL(kWebAppUrl));
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  AssertMetricData(metric_data);
  ASSERT_TRUE(
      metric_data.telemetry_data().app_telemetry().has_app_install_data());
  const auto& app_install_data =
      metric_data.telemetry_data().app_telemetry().app_install_data();
  EXPECT_THAT(app_install_data.app_id(), StrEq(kWebAppUrl));
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
  EXPECT_THAT(profile()->GetPrefs()->GetList(::ash::reporting::kAppsInstalled),
              Contains(app_id).Times(1));
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest,
                       PRE_PRE_ReportPreinstalledApp) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest,
                       PRE_ReportPreinstalledApp) {
  // Login as affiliated user and install app before closing the session.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  const auto app_id = InstallStandaloneWebApp(GURL(kWebAppUrl));
    ASSERT_THAT(
        profile()->GetPrefs()->GetList(::ash::reporting::kAppsInstalled),
        Contains(app_id).Times(1));
  ::ash::Shell::Get()->session_controller()->RequestSignOut();
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, ReportPreinstalledApp) {
  ::chromeos::MissiveClientTestObserver missive_observer(base::BindRepeating(
      &IsMetricEventOfType, MetricEventType::APP_INSTALLED));
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryPWA});
    ASSERT_THAT(
        profile()->GetPrefs()->GetList(::ash::reporting::kAppsInstalled),
        SizeIs(1));

  const auto app_id = InstallStandaloneWebApp(GURL(kWebAppUrl));
  ::content::RunAllTasksUntilIdle();
  ASSERT_FALSE(missive_observer.HasNewEnqueuedRecord());
    EXPECT_THAT(
        profile()->GetPrefs()->GetList(::ash::reporting::kAppsInstalled),
        Contains(app_id).Times(1));
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, PRE_ReportLaunchedApp) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, ReportLaunchedApp) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryPWA});

  const auto app_id = InstallStandaloneWebApp(GURL(kWebAppUrl));
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsMetricEventOfType, MetricEventType::APP_LAUNCHED));
  ::web_app::LaunchWebAppBrowser(profile(), app_id);
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  AssertMetricData(metric_data);
  ASSERT_TRUE(
      metric_data.telemetry_data().app_telemetry().has_app_launch_data());
  const auto& app_launch_data =
      metric_data.telemetry_data().app_telemetry().app_launch_data();
  EXPECT_THAT(
      app_launch_data.app_launch_source(),
      Eq(::apps::ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_TEST));
  EXPECT_THAT(app_launch_data.app_id(), StrEq(kWebAppUrl));
  EXPECT_THAT(app_launch_data.app_type(),
              Eq(::apps::ApplicationType::APPLICATION_TYPE_WEB));
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, PRE_ReportUninstalledApp) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(AppEventsObserverBrowserTest, ReportUninstalledApp) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryPWA});

  const auto app_id = InstallStandaloneWebApp(GURL(kWebAppUrl));

    ASSERT_THAT(
        profile()->GetPrefs()->GetList(::ash::reporting::kAppsInstalled),
        Contains(app_id).Times(1));

  ::chromeos::MissiveClientTestObserver missive_observer(base::BindRepeating(
      &IsMetricEventOfType, MetricEventType::APP_UNINSTALLED));
  UninstallStandaloneWebApp(app_id);
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  AssertMetricData(metric_data);
  ASSERT_TRUE(
      metric_data.telemetry_data().app_telemetry().has_app_uninstall_data());
  const auto& app_uninstall_data =
      metric_data.telemetry_data().app_telemetry().app_uninstall_data();
  EXPECT_THAT(app_uninstall_data.app_uninstall_source(),
              Eq(::apps::ApplicationUninstallSource::
                     APPLICATION_UNINSTALL_SOURCE_APP_LIST));
  EXPECT_THAT(app_uninstall_data.app_id(), StrEq(kWebAppUrl));
  EXPECT_THAT(app_uninstall_data.app_type(),
              Eq(::apps::ApplicationType::APPLICATION_TYPE_WEB));
    EXPECT_THAT(
        profile()->GetPrefs()->GetList(::ash::reporting::kAppsInstalled),
        Contains(app_id).Times(0));
}

}  // namespace
}  // namespace reporting
