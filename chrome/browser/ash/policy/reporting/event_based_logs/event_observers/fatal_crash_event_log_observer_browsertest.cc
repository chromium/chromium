// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observers/fatal_crash_event_log_observer.h"

#include <optional>
#include <set>

#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/ash/policy/core/policy_pref_names.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_uploader.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/reporting/util/status.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::cros_healthd::mojom::CrashEventInfo;
using ash::cros_healthd::mojom::CrashUploadInfo;

using ::testing::_;
using ::testing::DoAll;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::WithArg;

namespace {

class MockLogUploader : public policy::EventBasedLogUploader {
 public:
  MockLogUploader() = default;
  ~MockLogUploader() override = default;

  MOCK_METHOD(void,
              UploadEventBasedLogs,
              (std::set<support_tool::DataCollectorType>,
               ash::reporting::TriggerEventType,
               std::optional<std::string>,
               base::OnceCallback<void(reporting::Status)>),
              (override));
};

class FatalCrashEventLogObserverBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 protected:
  FatalCrashEventLogObserverBrowserTest() {
    // The feature needs to be enabled to get notified about crash events.
    scoped_feature_list_.InitAndEnableFeature(
        reporting::kEnableFatalCrashEventsObserver);
    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("token"));
    // Enable reporting policy to receive notification for crash events.
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceCrashReportInfo, true);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kSystemLogUploadEnabled, true);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    PrefService* local_state = g_browser_process->local_state();
    static_cast<PrefRegistrySimple*>(local_state->DeprecatedGetPrefRegistry())
        ->RegisterDictionaryPref(policy::prefs::kEventBasedLogLastUploadTimes);
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
  }

  // healthd emits a crash.
  void EmitFakeCrashEvent() {
    auto crash_event_info = CrashEventInfo::New();
    crash_event_info->local_id = "crash_id";
    crash_event_info->crash_type = CrashEventInfo::CrashType::kKernel;
    crash_event_info->upload_info = CrashUploadInfo::New();
    crash_event_info->upload_info->crash_report_id = "crash_report_id";
    // The default zero time is earlier than the UNIX epoch.
    crash_event_info->upload_info->creation_time = base::Time::UnixEpoch();

    ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
        ash::cros_healthd::mojom::EventCategoryEnum::kCrash,
        ash::cros_healthd::mojom::EventInfo::NewCrashEventInfo(
            std::move(crash_event_info)));
  }

 private:
  policy::DevicePolicyCrosTestHelper test_helper_;
  // Set up device affiliation. No need to set up or log in as an affiliated
  // user, because reporting fatal crash events is controlled by a device
  // policy.
  policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

using MockLogUploaderStrict = testing::StrictMock<MockLogUploader>;

IN_PROC_BROWSER_TEST_F(FatalCrashEventLogObserverBrowserTest,
                       UploadLogsWhenEventObserved) {
  base::RunLoop run_loop;
  std::unique_ptr<MockLogUploaderStrict> mock_uploader =
      std::make_unique<MockLogUploaderStrict>();
  EXPECT_CALL(*mock_uploader,
              UploadEventBasedLogs(
                  _, ash::reporting::TriggerEventType::FATAL_CRASH, _, _))
      .WillOnce(
          DoAll(WithArg<2>([&](std::optional<std::string> upload_id) {
                  // The triggered upload must have an upload ID attached.
                  EXPECT_TRUE(upload_id.has_value());
                  EXPECT_FALSE(upload_id.value().empty());
                }),
                base::test::RunOnceCallback<3>(reporting::Status::StatusOK())));

  policy::FatalCrashEventLogObserver event_observer;
  event_observer.SetLogUploaderForTesting(std::move(mock_uploader));

  EmitFakeCrashEvent();
  // Wait for all tasks to be completed.
  run_loop.RunUntilIdle();
}
