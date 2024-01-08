// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/version_updater/version_updater.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/version_updater/mock_version_updater_delegate.h"
#include "chrome/browser/ash/login/version_updater/update_time_estimator.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/portal_detector/mock_network_portal_detector.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::AnyNumber;
using testing::Mock;
using testing::Return;

namespace ash {

namespace {

constexpr int kDownloadTimeInSeconds = 50 * 60;
constexpr int kVerifyingTimeInSeconds = 5 * 60;
constexpr int kFinalizingTimeInSeconds = 5 * 60;

MATCHER_P(TimeLeftEq, time_in_seconds, "") {
  return arg.total_time_left == base::Seconds(time_in_seconds);
}

MATCHER_P2(DowloadingTimeLeftEq, can_be_used, time, "") {
  return arg.show_estimated_time_left == can_be_used &&
         arg.estimated_time_left_in_secs == time;
}
}  // anonymous namespace

class VersionUpdaterUnitTest : public testing::Test {
 public:
  VersionUpdaterUnitTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}

  VersionUpdaterUnitTest(const VersionUpdaterUnitTest&) = delete;
  VersionUpdaterUnitTest& operator=(const VersionUpdaterUnitTest&) = delete;

  void SetUpdateEngineStatus(update_engine::Operation operation,
                             bool is_install = false) {
    update_engine::StatusResult status;
    status.set_current_operation(operation);
    status.set_current_operation(operation);
    status.set_is_install(is_install);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  void SetStatusWithChecks(update_engine::Operation operation,
                           bool is_install = false) {
    testing::MockFunction<void(int check_point_name)> check;
    {
      testing::InSequence s;

      if (!is_install || operation == update_engine::Operation::IDLE) {
        EXPECT_CALL(*mock_delegate_, UpdateInfoChanged(_));
      }
      EXPECT_CALL(check, Call(checks_count_));
    }
    SetUpdateEngineStatus(operation, is_install);
    check.Call(checks_count_);
    ++checks_count_;
  }

  // testing::Test:
  void SetUp() override {
    // Initialize objects needed by VersionUpdater.
    fake_update_engine_client_ = UpdateEngineClient::InitializeFakeForTest();

    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();

    mock_network_portal_detector_ =
        std::make_unique<MockNetworkPortalDetector>();
    EXPECT_CALL(*mock_network_portal_detector_, IsEnabled())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));
    network_portal_detector::SetNetworkPortalDetector(
        mock_network_portal_detector_.get());

    mock_delegate_ = std::make_unique<MockVersionUpdaterDelegate>();
    version_updater_ = std::make_unique<VersionUpdater>(mock_delegate_.get());

    checks_count_ = 0;
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);

    version_updater_.reset();
    mock_delegate_.reset();

    network_portal_detector::InitializeForTesting(nullptr);
    network_handler_test_helper_.reset();

    UpdateEngineClient::Shutdown();
  }

  void InstallDLC() {
    SetStatusWithChecks(update_engine::Operation::CHECKING_FOR_UPDATE, true);

    SetStatusWithChecks(update_engine::Operation::UPDATE_AVAILABLE, true);

    SetStatusWithChecks(update_engine::Operation::DOWNLOADING, true);

    SetStatusWithChecks(update_engine::Operation::VERIFYING, true);

    SetStatusWithChecks(update_engine::Operation::FINALIZING, true);

    SetStatusWithChecks(update_engine::Operation::IDLE, true);
  }

  void StartNetworkCheck() {
    EXPECT_CALL(*mock_delegate_, UpdateInfoChanged(_)).Times(1);
    EXPECT_CALL(*mock_delegate_, PrepareForUpdateCheck()).Times(1);
    version_updater_->StartNetworkCheck();
  }

  std::string ConfigureWiFi(const std::string& state) {
    network_handler_test_helper_->ResetDevicesAndServices();
    return network_handler_test_helper_->ConfigureWiFi(state);
  }

 protected:
  std::unique_ptr<VersionUpdater> version_updater_;

  // Accessory objects needed by VersionUpdater.
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<MockVersionUpdaterDelegate> mock_delegate_;
  std::unique_ptr<MockNetworkPortalDetector> mock_network_portal_detector_;
  raw_ptr<FakeUpdateEngineClient, DanglingUntriaged> fake_update_engine_client_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  ScopedTestingLocalState local_state_;

  int checks_count_ = 0;
};

TEST_F(VersionUpdaterUnitTest, HandlesNoUpdate) {
  StartNetworkCheck();
  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  SetStatusWithChecks(update_engine::Operation::CHECKING_FOR_UPDATE);

  // No updates are available.
  EXPECT_CALL(*mock_delegate_,
              FinishExitUpdate(VersionUpdater::Result::UPDATE_NOT_REQUIRED))
      .Times(1);
  SetStatusWithChecks(update_engine::Operation::IDLE);
}

TEST_F(VersionUpdaterUnitTest, HandlesAvailableUpdate) {
  StartNetworkCheck();
  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  SetStatusWithChecks(update_engine::Operation::IDLE);

  SetStatusWithChecks(update_engine::Operation::CHECKING_FOR_UPDATE);

  SetStatusWithChecks(update_engine::Operation::UPDATE_AVAILABLE);

  SetStatusWithChecks(update_engine::Operation::DOWNLOADING);

  SetStatusWithChecks(update_engine::Operation::VERIFYING);

  SetStatusWithChecks(update_engine::Operation::FINALIZING);

  SetStatusWithChecks(update_engine::Operation::UPDATED_NEED_REBOOT);

  EXPECT_EQ(fake_update_engine_client_->reboot_after_update_call_count(), 0);
  version_updater_->RebootAfterUpdate();
  EXPECT_EQ(fake_update_engine_client_->reboot_after_update_call_count(), 1);
}

// Simple time left test case expectation which does not cover using download
// speed estimation.
TEST_F(VersionUpdaterUnitTest, TimeLeftExpectation) {
  StartNetworkCheck();
  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  EXPECT_CALL(*mock_delegate_, UpdateInfoChanged(TimeLeftEq(0)));
  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  Mock::VerifyAndClearExpectations(&mock_delegate_);

  // All time variables here and below in seconds.
  int time_left = kDownloadTimeInSeconds + kVerifyingTimeInSeconds +
                  kFinalizingTimeInSeconds;

  EXPECT_CALL(*mock_delegate_, UpdateInfoChanged(TimeLeftEq(0.0)));
  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);
  Mock::VerifyAndClearExpectations(&mock_delegate_);

  // DOWNLOADING starts.
  EXPECT_CALL(*mock_delegate_, UpdateInfoChanged(TimeLeftEq(time_left)));
  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  Mock::VerifyAndClearExpectations(&mock_delegate_);

  const int time_spent_on_downloading = 50;
  for (int seconds = 0; seconds < time_spent_on_downloading; seconds++) {
    EXPECT_CALL(*mock_delegate_,
                UpdateInfoChanged(TimeLeftEq(time_left - seconds - 1)));
  }
  task_environment_.FastForwardBy(base::Seconds(time_spent_on_downloading));
  Mock::VerifyAndClearExpectations(&mock_delegate_);

  // VERIFYING starts.
  time_left -= kDownloadTimeInSeconds;
  EXPECT_CALL(*mock_delegate_, UpdateInfoChanged(TimeLeftEq(time_left)));
  SetUpdateEngineStatus(update_engine::Operation::VERIFYING);
  Mock::VerifyAndClearExpectations(&mock_delegate_);

  // Spent more than expected:
  const int over_time = 20;
  const int time_spent_on_verifying = kVerifyingTimeInSeconds + over_time;
  for (int seconds = 0; seconds < kVerifyingTimeInSeconds - 1; seconds++) {
    EXPECT_CALL(*mock_delegate_,
                UpdateInfoChanged(TimeLeftEq(time_left - seconds - 1)));
  }
  EXPECT_CALL(
      *mock_delegate_,
      UpdateInfoChanged(TimeLeftEq(time_left - kVerifyingTimeInSeconds)))
      .Times(over_time + 1);
  task_environment_.FastForwardBy(base::Seconds(time_spent_on_verifying));
  Mock::VerifyAndClearExpectations(&mock_delegate_);

  // FINALIZING starts.
  time_left -= kVerifyingTimeInSeconds;
  EXPECT_CALL(*mock_delegate_, UpdateInfoChanged(TimeLeftEq(time_left)));
  SetUpdateEngineStatus(update_engine::Operation::FINALIZING);
  Mock::VerifyAndClearExpectations(&mock_delegate_);

  SetStatusWithChecks(update_engine::Operation::UPDATED_NEED_REBOOT);

  EXPECT_EQ(fake_update_engine_client_->reboot_after_update_call_count(), 0);
  version_updater_->RebootAfterUpdate();
  EXPECT_EQ(fake_update_engine_client_->reboot_after_update_call_count(), 1);
}

TEST_F(VersionUpdaterUnitTest, SimpleTimeLeftExpectationDownloadinStage) {
  StartNetworkCheck();
  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  EXPECT_CALL(*mock_delegate_,
              UpdateInfoChanged(DowloadingTimeLeftEq(false, 0)));
  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  Mock::VerifyAndClearExpectations(&mock_delegate_);

  EXPECT_CALL(*mock_delegate_,
              UpdateInfoChanged(DowloadingTimeLeftEq(false, 0)));
  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);
  Mock::VerifyAndClearExpectations(&mock_delegate_);

  // DOWNLOADING starts.
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::DOWNLOADING);
  status.set_progress(0.0);
  status.set_new_size(1.0);
  EXPECT_CALL(*mock_delegate_,
              UpdateInfoChanged(DowloadingTimeLeftEq(false, 0)));
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  EXPECT_CALL(*mock_delegate_,
              UpdateInfoChanged(DowloadingTimeLeftEq(false, 0)));
  task_environment_.FastForwardBy(base::Seconds(1));

  status.set_progress(0.01);
  EXPECT_CALL(*mock_delegate_,
              UpdateInfoChanged(DowloadingTimeLeftEq(true, 99)));
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  Mock::VerifyAndClearExpectations(&mock_delegate_);
}

TEST_F(VersionUpdaterUnitTest, HandlesCancelUpdateOnUpdateAvailable) {
  StartNetworkCheck();

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  SetStatusWithChecks(update_engine::Operation::IDLE);

  SetStatusWithChecks(update_engine::Operation::CHECKING_FOR_UPDATE);

  SetStatusWithChecks(update_engine::Operation::UPDATE_AVAILABLE);

  EXPECT_CALL(*mock_delegate_,
              FinishExitUpdate(VersionUpdater::Result::UPDATE_NOT_REQUIRED))
      .Times(1);
  version_updater_->StartExitUpdate(
      VersionUpdater::Result::UPDATE_NOT_REQUIRED);
}

TEST_F(VersionUpdaterUnitTest, HandlesCancelUpdateOnDownloading) {
  StartNetworkCheck();

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  SetStatusWithChecks(update_engine::Operation::IDLE);

  SetStatusWithChecks(update_engine::Operation::CHECKING_FOR_UPDATE);

  SetStatusWithChecks(update_engine::Operation::UPDATE_AVAILABLE);

  SetStatusWithChecks(update_engine::Operation::DOWNLOADING);

  EXPECT_CALL(*mock_delegate_,
              FinishExitUpdate(VersionUpdater::Result::UPDATE_NOT_REQUIRED))
      .Times(1);
  version_updater_->StartExitUpdate(
      VersionUpdater::Result::UPDATE_NOT_REQUIRED);
}

TEST_F(VersionUpdaterUnitTest, HandleUpdateError) {
  StartNetworkCheck();

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  SetStatusWithChecks(update_engine::Operation::IDLE);

  SetStatusWithChecks(update_engine::Operation::CHECKING_FOR_UPDATE);

  SetStatusWithChecks(update_engine::Operation::UPDATE_AVAILABLE);

  SetStatusWithChecks(update_engine::Operation::REPORTING_ERROR_EVENT);

  EXPECT_CALL(*mock_delegate_,
              FinishExitUpdate(VersionUpdater::Result::UPDATE_ERROR))
      .Times(1);
  version_updater_->StartExitUpdate(VersionUpdater::Result::UPDATE_ERROR);
}

TEST_F(VersionUpdaterUnitTest, HandlesPortalOnline) {
  EXPECT_CALL(*mock_network_portal_detector_, IsEnabled())
      .WillOnce(Return(true));

  // StartNetworkCheck will call PortalStateChanged with an unknown portal
  // state.
  EXPECT_CALL(*mock_delegate_,
              UpdateErrorMessage(NetworkState::PortalState::kUnknown,
                                 NetworkError::ERROR_STATE_OFFLINE, _))
      .Times(1);
  EXPECT_CALL(*mock_delegate_, ShowErrorMessage()).Times(1);
  EXPECT_CALL(*mock_delegate_, UpdateInfoChanged(_)).Times(2);
  std::string path = ConfigureWiFi(shill::kStateIdle);
  version_updater_->StartNetworkCheck();
  Mock::VerifyAndClearExpectations(&mock_delegate_);

  // Setting the default network state to online will trigger
  // PortalStateChanged with update_info.state == STATE_ERROR and
  // PortalState == kOnline.
  EXPECT_CALL(*mock_delegate_, PrepareForUpdateCheck()).Times(1);
  EXPECT_CALL(*mock_delegate_, UpdateInfoChanged(_)).Times(1);
  network_handler_test_helper_->SetServiceProperty(
      path, shill::kStateProperty, base::Value(shill::kStateOnline));
}

TEST_F(VersionUpdaterUnitTest, HandlesPortalError) {
  EXPECT_CALL(*mock_network_portal_detector_, IsEnabled())
      .WillOnce(Return(true));

  // StartNetworkCheck will call PortalStateChanged with update_info.state with
  // an unknown portal state.
  EXPECT_CALL(*mock_delegate_,
              UpdateErrorMessage(NetworkState::PortalState::kUnknown,
                                 NetworkError::ERROR_STATE_OFFLINE, _))
      .Times(1);
  EXPECT_CALL(*mock_delegate_, ShowErrorMessage()).Times(1);
  EXPECT_CALL(*mock_delegate_, UpdateInfoChanged(_)).Times(2);
  std::string path = ConfigureWiFi(shill::kStateIdle);
  std::string ssid = network_handler_test_helper_->GetServiceStringProperty(
      path, shill::kSSIDProperty);
  version_updater_->StartNetworkCheck();
  Mock::VerifyAndClearExpectations(&mock_delegate_);

  // Setting the default network state to redirect-found will trigger
  // PortalStateChanged with update_info.state == STATE_ERROR and
  // PortalState == kPortal.
  EXPECT_CALL(*mock_delegate_,
              UpdateErrorMessage(NetworkState::PortalState::kPortal,
                                 NetworkError::ERROR_STATE_PORTAL, ssid))
      .Times(1);
  network_handler_test_helper_->SetServiceProperty(
      path, shill::kStateProperty, base::Value(shill::kStateRedirectFound));
}

TEST_F(VersionUpdaterUnitTest, IgnoreInstallStatus) {
  StartNetworkCheck();
  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  InstallDLC();

  // Verify that the DUT retry check for an update after receiving IDLE.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 2);

  // Verify that all non_idle_state ignored for install status
  EXPECT_EQ(version_updater_->get_non_idle_status_received_for_testing(),
            false);
}

TEST_F(VersionUpdaterUnitTest, RetryOnIDLEState) {
  StartNetworkCheck();
  // Verify that the DUT checks for an update.
  // this is the iitial request and not include in the retry update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  // Retry 3 Times
  InstallDLC();
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 2);

  InstallDLC();
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 3);
}

TEST_F(VersionUpdaterUnitTest, ExitOnRetryCheckTimeout) {
  StartNetworkCheck();

  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  EXPECT_CALL(*mock_delegate_,
              FinishExitUpdate(VersionUpdater::Result::UPDATE_CHECK_TIMEOUT))
      .Times(1);
  task_environment_.FastForwardBy(base::Seconds(185));
}

}  // namespace ash
