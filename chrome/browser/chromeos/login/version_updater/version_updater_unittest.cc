// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/version_updater/version_updater.h"

#include <memory>

#include "base/command_line.h"
#include "base/optional.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/version_updater/mock_version_updater_delegate.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/portal_detector/mock_network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace chromeos {

namespace {
constexpr const char kNetworkGuid[] = "test_network";
}  // anonymous namespace

class VersionUpdaterUnitTest : public testing::Test {
 public:
  VersionUpdaterUnitTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUpdateEngineStatus(update_engine::Operation operation) {
    update_engine::StatusResult status;
    status.set_current_operation(operation);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  void SetStatusWithChecks(update_engine::Operation operation) {
    testing::MockFunction<void(int check_point_name)> check;
    {
      testing::InSequence s;

      EXPECT_CALL(*mock_delegate_, UpdateInfoChanged(_));
      EXPECT_CALL(check, Call(checks_count_));
    }

    SetUpdateEngineStatus(operation);
    check.Call(checks_count_);
    ++checks_count_;
  }

  void SetUpMockNetworkPortalDetector() {
    network_portal_detector::SetNetworkPortalDetector(
        mock_network_portal_detector_.get());
  }

  void SetUpFakeNetworkPortalDetector() {
    fake_network_portal_detector_->SetDefaultNetworkForTesting(kNetworkGuid);
    network_portal_detector::SetNetworkPortalDetector(
        fake_network_portal_detector_.get());
  }

  // testing::Test:
  void SetUp() override {
    // Initialize objects needed by VersionUpdater.
    fake_update_engine_client_ = new FakeUpdateEngineClient();
    DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(fake_update_engine_client_));

    NetworkHandler::Initialize();

    // |mock_network_portal_detector_->IsEnabled()| will always return false.
    mock_network_portal_detector_ =
        std::make_unique<MockNetworkPortalDetector>();
    EXPECT_CALL(*mock_network_portal_detector_, IsEnabled())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    // |fake_network_portal_detector_->IsEnabled()| will always return true.
    fake_network_portal_detector_ =
        std::make_unique<NetworkPortalDetectorTestImpl>();

    mock_delegate_ = std::make_unique<MockVersionUpdaterDelegate>();
    version_updater_ = std::make_unique<VersionUpdater>(mock_delegate_.get());

    checks_count_ = 0;
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
    // We need to stop observing |NetworkPortalDetector| before call
    // |DBusThreadManager::Shutdown()|, so destroy |version_updater_| now.
    version_updater_.reset();
    mock_delegate_.reset();

    network_portal_detector::InitializeForTesting(nullptr);
    NetworkHandler::Shutdown();

    // It will delete |fake_update_engine_client_|.
    DBusThreadManager::Shutdown();
  }

 protected:
  std::unique_ptr<VersionUpdater> version_updater_;

  // Accessory objects needed by VersionUpdater.
  std::unique_ptr<MockVersionUpdaterDelegate> mock_delegate_;
  std::unique_ptr<MockNetworkPortalDetector> mock_network_portal_detector_;
  std::unique_ptr<NetworkPortalDetectorTestImpl> fake_network_portal_detector_;
  FakeUpdateEngineClient* fake_update_engine_client_;

 private:
  // Test versions of core browser infrastructure.
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;

  int checks_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterUnitTest);
};

TEST_F(VersionUpdaterUnitTest, HandlesNoUpdate) {
  SetUpMockNetworkPortalDetector();

  EXPECT_CALL(*mock_delegate_, PrepareForUpdateCheck()).Times(1);
  version_updater_->StartNetworkCheck();
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
  SetUpMockNetworkPortalDetector();

  EXPECT_CALL(*mock_delegate_, PrepareForUpdateCheck()).Times(1);
  version_updater_->StartNetworkCheck();
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

TEST_F(VersionUpdaterUnitTest, HandlesCancelUpdateOnUpdateAvailable) {
  SetUpMockNetworkPortalDetector();

  EXPECT_CALL(*mock_delegate_, PrepareForUpdateCheck()).Times(1);
  version_updater_->StartNetworkCheck();

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
  SetUpMockNetworkPortalDetector();

  EXPECT_CALL(*mock_delegate_, PrepareForUpdateCheck()).Times(1);
  version_updater_->StartNetworkCheck();

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
  SetUpMockNetworkPortalDetector();

  EXPECT_CALL(*mock_delegate_, PrepareForUpdateCheck()).Times(1);
  version_updater_->StartNetworkCheck();

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
  SetUpFakeNetworkPortalDetector();

  version_updater_->StartNetworkCheck();

  NetworkPortalDetector::CaptivePortalState state;
  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;

  EXPECT_CALL(*mock_delegate_, PrepareForUpdateCheck()).Times(1);
  fake_network_portal_detector_->SetDetectionResultsForTesting(kNetworkGuid,
                                                               state);
  fake_network_portal_detector_->NotifyObserversForTesting();
}

TEST_F(VersionUpdaterUnitTest, HandlesPortalError) {
  SetUpFakeNetworkPortalDetector();

  version_updater_->StartNetworkCheck();

  NetworkPortalDetector::CaptivePortalState state;
  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;

  // Name of the network is empty because of implementation
  // SetDefaultNetworkForTesting (and it's not easy to fix it).
  EXPECT_CALL(
      *mock_delegate_,
      UpdateErrorMessage(state.status, NetworkError::ERROR_STATE_PORTAL, ""))
      .Times(1);
  EXPECT_CALL(*mock_delegate_, DelayErrorMessage()).Times(1);
  fake_network_portal_detector_->SetDetectionResultsForTesting(kNetworkGuid,
                                                               state);
  fake_network_portal_detector_->NotifyObserversForTesting();
}

}  // namespace chromeos
