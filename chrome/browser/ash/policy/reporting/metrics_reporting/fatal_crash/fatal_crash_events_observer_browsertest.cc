// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_save_file_paths_provider.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_test_util.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {
constexpr std::string_view kTestLocalId = "a local ID";
constexpr std::string_view kTestCrashReportId = "a crash report ID";

using ::ash::cros_healthd::FakeCrosHealthd;
using ::ash::cros_healthd::mojom::CrashEventInfo;
using ::ash::cros_healthd::mojom::CrashUploadInfo;
using ::ash::cros_healthd::mojom::EventCategoryEnum;
using ::ash::cros_healthd::mojom::EventInfo;
using ::testing::Eq;

// Filter to determine if a record is a crash event as required by the browser
// tests.
bool IsRecordCrashEvent(const ::reporting::Record& record) {
  MetricData record_data;
  EXPECT_TRUE(record_data.ParseFromString(record.data()));
  return
      // Destination must be EVENT_METRIC.
      record.has_destination() &&
      record.destination() == Destination::EVENT_METRIC &&
      // Event type must be FATAL_CRASH.
      record_data.has_event_data() && record_data.event_data().has_type() &&
      record_data.event_data().type() == MetricEventType::FATAL_CRASH;
}

// Browser test environment for fatal crash events.
class [[maybe_unused]] BrowserTestEnvironment final
    : public FatalCrashEventsObserver::TestEnvironment::SaveFilePathsProvider {
 public:
  BrowserTestEnvironment() = default;
  ~BrowserTestEnvironment() override = default;
  BrowserTestEnvironment(const BrowserTestEnvironment&) = delete;
  BrowserTestEnvironment& operator=(const BrowserTestEnvironment&) = delete;

 private:
  // Reset g_default_save_file_paths_provider_ during the test.
  base::AutoReset<decltype(g_default_save_file_paths_provider_)> auto_reset_{
      &g_default_save_file_paths_provider_, this};
};

class FatalCrashEventsBrowserTest
    : public ::policy::DevicePolicyCrosBrowserTest,
      public ::testing::WithParamInterface</*uploaded=*/bool> {
 protected:
  FatalCrashEventsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kEnableFatalCrashEventsObserver);
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidToken("token"));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  bool is_uploaded() const { return GetParam(); }

  void EnablePolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceCrashReportInfo, true);
  }

  // healthd emits a crash.
  static void EmitCrash(bool is_uploaded) {
    auto crash_event_info = CrashEventInfo::New();
    crash_event_info->local_id = kTestLocalId;
    if (is_uploaded) {
      crash_event_info->upload_info = CrashUploadInfo::New();
      crash_event_info->upload_info->crash_report_id = kTestCrashReportId;
      // The default zero time is earlier than the UNIX epoch.
      crash_event_info->upload_info->creation_time = base::Time::UnixEpoch();
    }

    FakeCrosHealthd::Get()->EmitEventForCategory(
        EventCategoryEnum::kCrash,
        EventInfo::NewCrashEventInfo(std::move(crash_event_info)));
  }

 private:
  BrowserTestEnvironment fatal_crash_events_browser_test_environment_;
  ::policy::DevicePolicyCrosTestHelper test_helper_;
  // Set up device affiliation. No need to set up or log in as an affiliated
  // user, because reporting fatal crash events is controlled by a device
  // policy.
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(FatalCrashEventsBrowserTest,
                       CrashOccursAndPolicyEnabled) {
  EnablePolicy();
  chromeos::MissiveClientTestObserver missive_event_observer(
      base::BindRepeating(IsRecordCrashEvent));

  EmitCrash(is_uploaded());

  const auto [priority, record] =
      missive_event_observer.GetNextEnqueuedRecord();
  EXPECT_THAT(priority, Eq(Priority::IMMEDIATE));
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));

  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));

  // Testing event found successfully. It is sufficient to test,
  // local ID (present in both uploaded and unuploaded crashes) and crash report
  // ID (uploaded crash only) in browser tests. Detailed tests of fields are
  // covered in unit tests.
  ASSERT_TRUE(metric_data.has_telemetry_data());
  ASSERT_TRUE(metric_data.telemetry_data().has_fatal_crash_telemetry());
  const auto& fatal_crash_telemetry =
      metric_data.telemetry_data().fatal_crash_telemetry();

  ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
  EXPECT_EQ(fatal_crash_telemetry.local_id(), kTestLocalId);

  if (is_uploaded()) {
    ASSERT_TRUE(fatal_crash_telemetry.has_crash_report_id());
    EXPECT_EQ(fatal_crash_telemetry.crash_report_id(), kTestCrashReportId);
  }
}

IN_PROC_BROWSER_TEST_P(FatalCrashEventsBrowserTest,
                       CrashOccursAndPolicyDefault) {
  // Not enabling policy here.
  chromeos::MissiveClientTestObserver missive_event_observer(
      base::BindRepeating(IsRecordCrashEvent));
  EmitCrash(is_uploaded());

  ::content::RunAllTasksUntilIdle();
  EXPECT_FALSE(missive_event_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(FatalCrashEventsBrowserTest,
                       CrashNotOccurAndPolicyEnabled) {
  EnablePolicy();
  chromeos::MissiveClientTestObserver missive_event_observer(
      base::BindRepeating(IsRecordCrashEvent));
  // Not emitting crashes here.

  ::content::RunAllTasksUntilIdle();
  EXPECT_FALSE(missive_event_observer.HasNewEnqueuedRecord());
}

INSTANTIATE_TEST_SUITE_P(
    FatalCrashEventsBrowserTests,
    FatalCrashEventsBrowserTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<FatalCrashEventsBrowserTest::ParamType>&
           info) { return info.param ? "uploaded" : "unuploaded"; });
}  // namespace
}  // namespace reporting
