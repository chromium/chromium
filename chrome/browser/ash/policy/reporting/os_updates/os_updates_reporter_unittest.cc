// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/os_updates/os_updates_reporter.h"

#include <memory>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper_testing.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/os_events.pb.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::StrEq;

constexpr char kNewVersion[] = "1233.0.0";
constexpr char kLsbRelease[] =
    R"(CHROMEOS_RELEASE_NAME=Chrome OS
CHROMEOS_RELEASE_VERSION=11012.0.2018_08_28_1422
)";

namespace ash::reporting {

struct OsUpdatesReporterTestCase {
  update_engine::Operation operation;
  bool enterprise_rollback;
};

class TestHelper {
 public:
  TestHelper() = default;

  TestHelper(const TestHelper&) = delete;
  TestHelper& operator=(const TestHelper&) = delete;

  ~TestHelper() = default;

  void Init() {
    chromeos::PowerManagerClient::InitializeFake();
    SessionManagerClient::InitializeFake();
  }

  void Shutdown() {
    SessionManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

  std::unique_ptr<::reporting::UserEventReporterHelperTesting>
  GetReporterHelper(bool reporting_enabled,
                    ::reporting::Status status = ::reporting::Status()) {
    record_.Clear();
    report_count_ = 0;
    auto mock_queue = std::unique_ptr<::reporting::MockReportQueue,
                                      base::OnTaskRunnerDeleter>(
        new ::reporting::MockReportQueue(),
        base::OnTaskRunnerDeleter(
            base::SequencedTaskRunner::GetCurrentDefault()));

    ON_CALL(*mock_queue, AddRecord(_, ::reporting::Priority::SECURITY, _))
        .WillByDefault(
            [this, status](std::string_view record_string,
                           ::reporting::Priority event_priority,
                           ::reporting::ReportQueue::EnqueueCallback cb) {
              ++report_count_;
              EXPECT_TRUE(record_.ParseFromArray(record_string.data(),
                                                 record_string.size()));
              std::move(cb).Run(status);
            });

    auto reporter_helper =
        std::make_unique<::reporting::UserEventReporterHelperTesting>(
            reporting_enabled, /*should_report_user=*/true,
            /*is_kiosk_user=*/false, std::move(mock_queue));
    return reporter_helper;
  }

  OsEventsRecord GetRecord() { return record_; }

  int GetReportCount() { return report_count_; }

 private:
  content::BrowserTaskEnvironment task_environment_;

  OsEventsRecord record_;
  int report_count_ = 0;
};

class OsUpdatesReporterTest
    : public ::testing::TestWithParam<OsUpdatesReporterTestCase> {
 protected:
  OsUpdatesReporterTest() {}

  void SetUp() override { test_helper_.Init(); }

  void TearDown() override { test_helper_.Shutdown(); }

  TestHelper test_helper_;
};

TEST_F(OsUpdatesReporterTest, ReportSuccessfulUpdatePolicyEnabled) {
  // Set current OS Version.
  base::test::ScopedChromeOSVersionInfo scoped_version(
      kLsbRelease, /*lsb_release_time=*/base::Time());

  // Set up fake update client.
  ash::FakeUpdateEngineClient* fake_update_engine_client_ =
      ash::UpdateEngineClient::InitializeFakeForTest();

  // Set up reporter.
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true);
  auto reporter = ::reporting::OsUpdatesReporter::CreateForTesting(
      std::move(reporter_helper));

  // Build and send update status.
  update_engine::StatusResult status;
  status.set_new_version(kNewVersion);
  status.set_is_enterprise_rollback(false);
  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  // Verify event.
  const OsEventsRecord& record = test_helper_.GetRecord();
  ASSERT_EQ(test_helper_.GetReportCount(), 1);
  ASSERT_TRUE(record.has_update_event());
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_EQ(record.target_os_version(), kNewVersion);
  EXPECT_EQ(record.os_operation_type(), reporting::OsOperationType::SUCCESS);
}

TEST_F(OsUpdatesReporterTest, ReportSuccessfulUpdatePolicyDisabled) {
  // Set current OS Version.
  base::test::ScopedChromeOSVersionInfo scoped_version(
      kLsbRelease, /*lsb_release_time=*/base::Time());

  // Set up fake update client.
  ash::FakeUpdateEngineClient* fake_update_engine_client_ =
      ash::UpdateEngineClient::InitializeFakeForTest();

  // Set up reporter.
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/false);
  auto reporter = ::reporting::OsUpdatesReporter::CreateForTesting(
      std::move(reporter_helper));

  // Build and send update status.
  update_engine::StatusResult status;
  status.set_new_version(kNewVersion);
  status.set_is_enterprise_rollback(false);
  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  // Verify event.
  ASSERT_EQ(test_helper_.GetReportCount(), 0);
}

TEST_P(OsUpdatesReporterTest, ReportFailedUpdateRollbackPolicyEnabled) {
  const auto test_case = GetParam();
  // Set current OS Version.
  base::test::ScopedChromeOSVersionInfo scoped_version(
      kLsbRelease, /*lsb_release_time=*/base::Time());

  // Set up fake update client.
  ash::FakeUpdateEngineClient* fake_update_engine_client_ =
      ash::UpdateEngineClient::InitializeFakeForTest();

  // Set up reporter.
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true);
  auto reporter = ::reporting::OsUpdatesReporter::CreateForTesting(
      std::move(reporter_helper));

  // Build and send update status.
  update_engine::StatusResult status;
  status.set_new_version(kNewVersion);
  status.set_is_enterprise_rollback(test_case.enterprise_rollback);
  status.set_current_operation(test_case.operation);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  // Verify event.
  const OsEventsRecord& record = test_helper_.GetRecord();
  ASSERT_EQ(test_helper_.GetReportCount(), 1);
  if (test_case.enterprise_rollback) {
    ASSERT_TRUE(record.has_rollback_event());
  } else {
    ASSERT_TRUE(record.has_update_event());
  }
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_EQ(record.target_os_version(), kNewVersion);
  EXPECT_EQ(record.os_operation_type(), reporting::OsOperationType::FAILURE);
}

TEST_P(OsUpdatesReporterTest, ReportFailedUpdateRollbackPolicyDisabled) {
  const auto test_case = GetParam();
  // Set current OS Version.
  base::test::ScopedChromeOSVersionInfo scoped_version(
      kLsbRelease, /*lsb_release_time=*/base::Time());

  // Set up fake update client.
  ash::FakeUpdateEngineClient* fake_update_engine_client_ =
      ash::UpdateEngineClient::InitializeFakeForTest();

  // Set up reporter.
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/false);
  auto reporter = ::reporting::OsUpdatesReporter::CreateForTesting(
      std::move(reporter_helper));

  // Build and send update status.
  update_engine::StatusResult status;
  status.set_new_version(kNewVersion);
  status.set_is_enterprise_rollback(test_case.enterprise_rollback);
  status.set_current_operation(test_case.operation);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  // Verify event.
  ASSERT_EQ(test_helper_.GetReportCount(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OsUpdatesReporterTest,
    ::testing::ValuesIn<OsUpdatesReporterTestCase>(
        {{/*operation=*/update_engine::Operation::ERROR,
          /*enterprise_rollback=*/true},
         {/*operation=*/update_engine::Operation::ERROR,
          /*enterprise_rollback=*/false},
         {/*operation=*/update_engine::Operation::REPORTING_ERROR_EVENT,
          /*enterprise_rollback=*/true},
         {/*operation=*/update_engine::Operation::REPORTING_ERROR_EVENT,
          /*enterprise_rollback=*/false}}));

class PowerwashTest : public ::testing::TestWithParam<bool> {
 protected:
  PowerwashTest() {}

  void SetUp() override {
    test_helper_.Init();
    ash::UpdateEngineClient::InitializeFakeForTest();
    remote_requested_ = GetParam();
  }

  void TearDown() override {
    test_helper_.Shutdown();
    ash::UpdateEngineClient::Shutdown();
  }

  TestHelper test_helper_;
  bool remote_requested_;
};

TEST_P(PowerwashTest, PolicyEnabled) {
  // Set current OS Version.
  base::test::ScopedChromeOSVersionInfo scoped_version(
      kLsbRelease, /*lsb_release_time=*/base::Time());

  // Set up fake session manager.
  ash::SessionManagerClient::InitializeFake();
  FakeSessionManagerClient* session_manager = FakeSessionManagerClient::Get();

  //  Set up reporter.
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true);
  auto reporter = ::reporting::OsUpdatesReporter::CreateForTesting(
      std::move(reporter_helper));

  // Fake a powerwash.
  if (remote_requested_) {
    session_manager->StartRemoteDeviceWipe(enterprise_management::SignedData());
  } else {
    session_manager->StartDeviceWipe(base::DoNothing());
  }

  // Verify event.
  const OsEventsRecord& record = test_helper_.GetRecord();
  ASSERT_EQ(test_helper_.GetReportCount(), 1);
  ASSERT_EQ(session_manager->start_device_wipe_call_count(), 1);
  ASSERT_TRUE(record.has_powerwash_event());
  ASSERT_EQ(record.os_operation_type(), reporting::OsOperationType::INITIATED);
  EXPECT_EQ(record.powerwash_event().remote_request(), remote_requested_);
  EXPECT_TRUE(record.has_event_timestamp_sec());
}

TEST_P(PowerwashTest, PolicyDisabled) {
  // Set current OS Version.
  base::test::ScopedChromeOSVersionInfo scoped_version(
      kLsbRelease, /*lsb_release_time=*/base::Time());

  // Set up fake session manager.
  ash::SessionManagerClient::InitializeFake();
  FakeSessionManagerClient* session_manager = FakeSessionManagerClient::Get();

  //  Set up reporter.
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/false);
  auto reporter = ::reporting::OsUpdatesReporter::CreateForTesting(
      std::move(reporter_helper));

  // Fake a powerwash.
  if (remote_requested_) {
    session_manager->StartRemoteDeviceWipe(enterprise_management::SignedData());
  } else {
    session_manager->StartDeviceWipe(base::DoNothing());
  }

  // Verify that no event was reported.
  ASSERT_EQ(test_helper_.GetReportCount(), 0);
  // Verify that the powerwash is still going to happen.
  ASSERT_EQ(session_manager->start_device_wipe_call_count(), 1);
}

INSTANTIATE_TEST_SUITE_P(All, PowerwashTest, ::testing::Bool());

}  // namespace ash::reporting
