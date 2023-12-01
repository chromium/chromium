// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/policy/remote_commands/user_session_type_test_util.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/util/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using ::testing::WithArg;

namespace policy {

using test::TestSessionType;

namespace em = enterprise_management;

namespace {

// SessionInfo is the parameter type for
// ParametrizedFetchSupportPacketTest tests. It includes the different session
// types that can exist on ChromeOS devices and if the PII is allowed to be kept
// in the collected logs. If PII is not allowed, FETCH_SUPPORT_PACKET command
// job will attach a note to the command result payload.
struct SessionInfo {
  // Print for easier debugging:
  // https://github.com/google/googletest/blob/main/docs/advanced.md#teaching-googletest-how-to-print-your-values
  friend void PrintTo(const SessionInfo& session_info, std::ostream* os) {
    *os << "{session_type="
        << test::SessionTypeToString(session_info.session_type)
        << ", pii_allowed=" << session_info.pii_allowed << "}";
  }

  TestSessionType session_type;
  bool pii_allowed;
};

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 123456;
// The age of command in milliseconds.
constexpr int64 kCommandAge = 60000;

constexpr char kExpectedUploadParametersFormatter[] =
    R"({"Command-ID":"%ld","File-Type":"support_file","Filename":"%s"}
application/json)";

// Return a valid command payload with at least one data collector requested.
// The returned payload doesn't contain any PII request. The returned payload
// will be as following.
// {"supportPacketDetails":{
//     "issueCaseId": "issue_case_id",
//     "issueDescription": "issue description",
//     "requestedDataCollectors":
//     [support_tool::DataCollectorType::CHROMEOS_SYSTEM_LOGS(17)],
//     "requestedPiiTypes": [],
//     "requesterMetadata": "obfuscated123"
//   }
// }
base::Value::Dict GetCommandPayloadDict() {
  base::Value::Dict support_packet_details;
  support_packet_details.Set("issueCaseId", "issue_case_id");
  support_packet_details.Set("issueDescription", "issue description");
  support_packet_details.Set("requesterMetadata", "obfuscated123");
  support_packet_details.Set(
      "requestedDataCollectors",
      base::Value::List().Append(
          support_tool::DataCollectorType::CHROMEOS_SYSTEM_LOGS));
  support_packet_details.Set("requestedPiiTypes", base::Value::List());
  return base::Value::Dict().Set("supportPacketDetails",
                                 std::move(support_packet_details));
}

em::RemoteCommand GenerateCommandProto(std::string payload) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_FETCH_SUPPORT_PACKET);
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(kCommandAge);
  command_proto.set_payload(payload);
  return command_proto;
}

}  // namespace

class DeviceCommandFetchSupportPacketTest : public ash::DeviceSettingsTestBase {
 public:
  DeviceCommandFetchSupportPacketTest()
      : ash::DeviceSettingsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  DeviceCommandFetchSupportPacketTest(
      const DeviceCommandFetchSupportPacketTest&) = delete;
  DeviceCommandFetchSupportPacketTest& operator=(
      const DeviceCommandFetchSupportPacketTest&) = delete;

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ash::DebugDaemonClient::InitializeFake();
    // Set serial number for testing.
    statistics_provider_.SetMachineStatistic("serial_number", "000000");
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);
    cros_settings_helper_.ReplaceDeviceSettingsProviderWithStub();

    arc_kiosk_app_manager_ = std::make_unique<ash::ArcKioskAppManager>();
    web_kiosk_app_manager_ = std::make_unique<ash::WebKioskAppManager>();
    kiosk_chrome_app_manager_ = std::make_unique<ash::KioskChromeAppManager>();
  }

  void TearDown() override {
    kiosk_chrome_app_manager_.reset();
    web_kiosk_app_manager_.reset();
    arc_kiosk_app_manager_.reset();

    ash::DebugDaemonClient::Shutdown();
    if (!temp_dir_.IsValid()) {
      return;
    }
    EXPECT_TRUE(temp_dir_.Delete());
    DeviceSettingsTestBase::TearDown();
  }

  void SetLogUploadEnabledPolicy(bool enabled) {
    cros_settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, enabled);
  }

  void StartSessionOfType(TestSessionType session_type) {
    // `user_manager_` is inherited from ash::DeviceSettingsTestBase.
    StartSessionOfTypeWithProfile(session_type, *user_manager_,
                                  profile_manager_);
  }

  void InitAndRunCommandJob(DeviceCommandFetchSupportPacketJob& in_job,
                            const em::RemoteCommand& command) {
    EXPECT_TRUE(in_job.Init(base::TimeTicks::Now(), command, em::SignedData()));

    base::test::TestFuture<void> job_finished_future;
    bool success = in_job.Run(base::Time::Now(), base::TimeTicks::Now(),
                              job_finished_future.GetCallback());
    EXPECT_TRUE(success);
    ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  }

  // TODO(b/313897897): We can directly use FakeReportQueue instead.
  void CaptureUpcomingEventOnReportQueue(
      DeviceCommandFetchSupportPacketJob& in_job,
      ash::reporting::LogUploadEvent& upcoming_event) {
    std::unique_ptr<reporting::MockReportQueueStrict> mock_report_queue =
        std::make_unique<reporting::MockReportQueueStrict>();
    EXPECT_CALL(*mock_report_queue.get(), AddRecord)
        .WillOnce(testing::WithArgs<0, 2>(
            [&upcoming_event](
                std::string serialized_record,
                reporting::ReportQueue::EnqueueCallback callback) {
              // Parse the enqueued event from serialized record proto.
              ASSERT_TRUE(upcoming_event.ParseFromString(serialized_record));
              std::move(callback).Run(reporting::Status::StatusOK());
            }));
    in_job.SetReportQueueForTesting(std::move(mock_report_queue));
  }

 protected:
  // App manager instances for testing kiosk sessions.
  std::unique_ptr<ash::ArcKioskAppManager> arc_kiosk_app_manager_;
  std::unique_ptr<ash::WebKioskAppManager> web_kiosk_app_manager_;
  std::unique_ptr<ash::KioskChromeAppManager> kiosk_chrome_app_manager_;

  ash::system::FakeStatisticsProvider statistics_provider_;
  ash::ScopedCrosSettingsTestHelper cros_settings_helper_;
  base::ScopedTempDir temp_dir_;
  base::HistogramTester histogram_tester_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
};

TEST_F(DeviceCommandFetchSupportPacketTest,
       FailIfPayloadContainsEmptyDataCollectors) {
  DeviceCommandFetchSupportPacketJob job;
  // Wrong payload with empty data collectors list.
  base::Value::Dict command_payload = GetCommandPayloadDict();
  command_payload.SetByDottedPath(
      "supportPacketDetails.requestedDataCollectors", base::Value::List());
  std::string wrong_payload;
  ASSERT_TRUE(
      base::JSONWriter::Write(std::move(command_payload), &wrong_payload));

  // Shouldn't be able to initialize with wrong payload.
  EXPECT_FALSE(job.Init(base::TimeTicks::Now(),
                        GenerateCommandProto(wrong_payload), em::SignedData()));
  histogram_tester_.ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kFailedOnWrongCommandPayload, 1);
}

TEST_F(DeviceCommandFetchSupportPacketTest, FailWhenLogUploadDisabled) {
  StartSessionOfType(TestSessionType::kNoSession);
  SetLogUploadEnabledPolicy(/*enabled=*/false);

  DeviceCommandFetchSupportPacketJob job;

  job.SetTargetDirForTesting(temp_dir_.GetPath());

  std::string payload;
  ASSERT_TRUE(
      base::JSONWriter::Write(std::move(GetCommandPayloadDict()), &payload));

  InitAndRunCommandJob(job, GenerateCommandProto(payload));

  EXPECT_EQ(job.status(), RemoteCommandJob::FAILED);
  // Expect result payload when the command fails because of not being
  // supported on the device.
  EXPECT_THAT(
      *job.GetResultPayload(),
      IsJson(base::Value::Dict().Set(
          "result", enterprise_management::FetchSupportPacketResultCode::
                        FAILURE_COMMAND_NOT_ENABLED)));

  histogram_tester_.ExpectUniqueSample(kFetchSupportPacketFailureHistogramName,
                                       EnterpriseFetchSupportPacketFailureType::
                                           kFailedOnCommandEnabledForUserCheck,
                                       1);
}

// Fixture for tests parameterized over the possible user session
// types(`TestSessionType`) and PII policies.
class DeviceCommandFetchSupportPacketTestParameterized
    : public DeviceCommandFetchSupportPacketTest,
      public ::testing::WithParamInterface<SessionInfo> {};

TEST_P(DeviceCommandFetchSupportPacketTestParameterized,
       SuccessfulCommandRequestWithoutPii) {
  const SessionInfo& session_info = GetParam();
  StartSessionOfType(session_info.session_type);
  SetLogUploadEnabledPolicy(/*enabled=*/true);

  DeviceCommandFetchSupportPacketJob job;

  job.SetTargetDirForTesting(temp_dir_.GetPath());

  ash::reporting::LogUploadEvent enqueued_event;
  CaptureUpcomingEventOnReportQueue(job, enqueued_event);

  std::string payload;
  ASSERT_TRUE(
      base::JSONWriter::Write(std::move(GetCommandPayloadDict()), &payload));
  InitAndRunCommandJob(job, GenerateCommandProto(payload));

  EXPECT_EQ(job.status(), RemoteCommandJob::ACKED);

  // The result payload should contain the success result code.
  EXPECT_THAT(
      *job.GetResultPayload(),
      IsJson(base::Value::Dict().Set(
          "result", enterprise_management::FetchSupportPacketResultCode::
                        FETCH_SUPPORT_PACKET_RESULT_SUCCESS)));

  base::FilePath exported_file = job.GetExportedFilepathForTesting();

  // Check the contents of LogUploadEvent that the job enqueued.
  std::string expected_upload_parameters =
      base::StringPrintf(kExpectedUploadParametersFormatter, kUniqueID,
                         exported_file.BaseName().value().c_str());
  EXPECT_EQ(
      expected_upload_parameters,
      *enqueued_event.mutable_upload_settings()->mutable_upload_parameters());
  EXPECT_EQ(exported_file.value(),
            *enqueued_event.mutable_upload_settings()->mutable_origin_path());
  EXPECT_TRUE(enqueued_event.has_command_id());
  EXPECT_EQ(enqueued_event.command_id(), kUniqueID);

  int64_t file_size;
  ASSERT_TRUE(base::GetFileSize(exported_file, &file_size));
  EXPECT_GT(file_size, 0);

  histogram_tester_.ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kNoFailure, 1);
}

TEST_P(DeviceCommandFetchSupportPacketTestParameterized,
       SuccessfulCommandRequestWithPii) {
  const SessionInfo& session_info = GetParam();
  StartSessionOfType(session_info.session_type);
  SetLogUploadEnabledPolicy(/*enabled=*/true);

  DeviceCommandFetchSupportPacketJob job;

  job.SetTargetDirForTesting(temp_dir_.GetPath());

  ash::reporting::LogUploadEvent enqueued_event;
  CaptureUpcomingEventOnReportQueue(job, enqueued_event);

  // Add a requested PII type to the command payload.
  base::Value::Dict command_payload_dict = GetCommandPayloadDict();
  command_payload_dict.SetByDottedPath(
      "supportPacketDetails.requestedPiiTypes",
      base::Value::List().Append(support_tool::PiiType::EMAIL));
  std::string payload;
  ASSERT_TRUE(
      base::JSONWriter::Write(std::move(command_payload_dict), &payload));
  InitAndRunCommandJob(job, GenerateCommandProto(payload));

  EXPECT_EQ(job.status(), RemoteCommandJob::ACKED);

  // The result payload should contain the success result code.
  base::Value::Dict expected_payload;
  expected_payload.Set("result",
                       enterprise_management::FetchSupportPacketResultCode::
                           FETCH_SUPPORT_PACKET_RESULT_SUCCESS);
  if (!session_info.pii_allowed) {
    // A note will be added to the result payload when requested PII is
    // not included in the collected logs.
    expected_payload.Set(
        "notes", base::Value::List().Append(
                     enterprise_management::FetchSupportPacketResultNote::
                         WARNING_PII_NOT_ALLOWED));
  }
  EXPECT_THAT(*job.GetResultPayload(), IsJson(std::move(expected_payload)));

  base::FilePath exported_file = job.GetExportedFilepathForTesting();

  // Check the contents of LogUploadEvent that the job enqueued.
  std::string expected_upload_parameters =
      base::StringPrintf(kExpectedUploadParametersFormatter, kUniqueID,
                         exported_file.BaseName().value().c_str());
  EXPECT_EQ(
      expected_upload_parameters,
      *enqueued_event.mutable_upload_settings()->mutable_upload_parameters());
  EXPECT_EQ(exported_file.value(),
            *enqueued_event.mutable_upload_settings()->mutable_origin_path());
  EXPECT_TRUE(enqueued_event.has_command_id());
  EXPECT_EQ(enqueued_event.command_id(), kUniqueID);

  int64_t file_size;
  ASSERT_TRUE(base::GetFileSize(exported_file, &file_size));
  EXPECT_GT(file_size, 0);

  histogram_tester_.ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kNoFailure, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceCommandFetchSupportPacketTestParameterized,
    ::testing::Values(
        SessionInfo{TestSessionType::kManuallyLaunchedArcKioskSession,
                    /*pii_allowed=*/true},
        SessionInfo{TestSessionType::kManuallyLaunchedWebKioskSession,
                    /*pii_allowed=*/true},
        SessionInfo{TestSessionType::kManuallyLaunchedKioskSession,
                    /*pii_allowed=*/true},
        SessionInfo{TestSessionType::kAutoLaunchedArcKioskSession,
                    /*pii_allowed=*/true},
        SessionInfo{TestSessionType::kAutoLaunchedWebKioskSession,
                    /*pii_allowed=*/true},
        SessionInfo{TestSessionType::kAutoLaunchedKioskSession,
                    /*pii_allowed=*/true},
        SessionInfo{TestSessionType::kAffiliatedUserSession,
                    /*pii_allowed=*/true},
        SessionInfo{TestSessionType::kManagedGuestSession,
                    /*pii_allowed=*/false},
        SessionInfo{TestSessionType::kGuestSession,
                    /*pii_allowed=*/false},
        SessionInfo{TestSessionType::kUnaffiliatedUserSession,
                    /*pii_allowed=*/false},
        SessionInfo{TestSessionType::kNoSession,
                    /*pii_allowed=*/false}));

}  // namespace policy
