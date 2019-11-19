// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_required_screen.h"

#include "base/command_line.h"
#include "base/optional.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/chromeos/login/screens/mock_error_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/ui/ash/test_login_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/fake_update_required_screen_handler.h"
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

class UpdateRequiredScreenUnitTest : public testing::Test {
 public:
  UpdateRequiredScreenUnitTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUpdateEngineStatus(update_engine::Operation operation) {
    update_engine::StatusResult status;
    status.set_current_operation(operation);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  // testing::Test:
  void SetUp() override {
    // Configure the browser to use Hands-Off Enrollment.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");

    // Initialize objects needed by |UpdateRequiredScreen|.
    fake_view_ = std::make_unique<FakeUpdateRequiredScreenHandler>();
    fake_update_engine_client_ = new FakeUpdateEngineClient();
    DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(fake_update_engine_client_));
    NetworkHandler::Initialize();
    mock_network_portal_detector_ = new MockNetworkPortalDetector();
    network_portal_detector::SetNetworkPortalDetector(
        mock_network_portal_detector_);
    mock_error_screen_.reset(new MockErrorScreen(mock_error_view_.get()));

    // Ensure proper behavior of |UpdateRequiredScreen|'s supporting objects.
    EXPECT_CALL(*mock_network_portal_detector_, IsEnabled())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    update_required_screen_ = std::make_unique<UpdateRequiredScreen>(
        fake_view_.get(), mock_error_screen_.get());

    update_required_screen_->GetVersionUpdaterForTesting()
        ->set_wait_for_reboot_time_for_testing(base::TimeDelta::FromSeconds(0));
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);

    update_required_screen_.reset();
    mock_error_view_.reset();
    mock_error_screen_.reset();

    network_portal_detector::Shutdown();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  // A pointer to the |UpdateRequiredScreen| used in this test.
  std::unique_ptr<UpdateRequiredScreen> update_required_screen_;

  // Accessory objects needed by |UpdateRequiredScreen|.
  TestLoginScreen test_login_screen_;
  std::unique_ptr<FakeUpdateRequiredScreenHandler> fake_view_;
  std::unique_ptr<MockErrorScreenView> mock_error_view_;
  std::unique_ptr<MockErrorScreen> mock_error_screen_;
  // Will be deleted in |network_portal_detector::Shutdown()|.
  MockNetworkPortalDetector* mock_network_portal_detector_;
  // Will be deleted in |DBusThreadManager::Shutdown()|.
  FakeUpdateEngineClient* fake_update_engine_client_;

 private:
  // Test versions of core browser infrastructure.
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;

  DISALLOW_COPY_AND_ASSIGN(UpdateRequiredScreenUnitTest);
};

namespace {
constexpr char kUserActionUpdateButtonClicked[] = "update";
constexpr char kUserActionAcceptUpdateOverCellular[] = "update-accept-cellular";
}  // namespace

TEST_F(UpdateRequiredScreenUnitTest, HandlesNoUpdate) {
  // DUT reaches |UpdateRequiredScreen|.
  update_required_screen_->Show();
  EXPECT_EQ(fake_view_->ui_state(),
            UpdateRequiredView::UPDATE_REQUIRED_MESSAGE);
  update_required_screen_->OnUserAction(kUserActionUpdateButtonClicked);

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  // No updates are available.
  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  SetUpdateEngineStatus(update_engine::Operation::IDLE);

  EXPECT_EQ(fake_view_->ui_state(), UpdateRequiredView::UPDATE_ERROR);
}

TEST_F(UpdateRequiredScreenUnitTest, HandlesUpdateExists) {
  // DUT reaches |UpdateRequiredScreen|.
  update_required_screen_->Show();
  EXPECT_EQ(fake_view_->ui_state(),
            UpdateRequiredView::UPDATE_REQUIRED_MESSAGE);
  update_required_screen_->OnUserAction(kUserActionUpdateButtonClicked);

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);

  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);

  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  EXPECT_EQ(fake_view_->ui_state(), UpdateRequiredView::UPDATE_PROCESS);

  SetUpdateEngineStatus(update_engine::Operation::VERIFYING);

  SetUpdateEngineStatus(update_engine::Operation::FINALIZING);

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_GE(fake_update_engine_client_->reboot_after_update_call_count(), 1);

  EXPECT_EQ(fake_view_->ui_state(),
            UpdateRequiredView::UPDATE_COMPLETED_NEED_REBOOT);
}

TEST_F(UpdateRequiredScreenUnitTest, HandlesCellularPermissionNeeded) {
  // DUT reaches |UpdateRequiredScreen|.
  update_required_screen_->Show();
  EXPECT_EQ(fake_view_->ui_state(),
            UpdateRequiredView::UPDATE_REQUIRED_MESSAGE);
  update_required_screen_->OnUserAction(kUserActionUpdateButtonClicked);

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);

  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);

  SetUpdateEngineStatus(update_engine::Operation::NEED_PERMISSION_TO_UPDATE);

  update_required_screen_->OnUserAction(kUserActionAcceptUpdateOverCellular);

  EXPECT_GE(
      fake_update_engine_client_->update_over_cellular_permission_count() +
          fake_update_engine_client_
              ->update_over_cellular_one_time_permission_count(),
      1);

  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  EXPECT_EQ(fake_view_->ui_state(), UpdateRequiredView::UPDATE_PROCESS);

  SetUpdateEngineStatus(update_engine::Operation::VERIFYING);

  SetUpdateEngineStatus(update_engine::Operation::FINALIZING);

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_GE(fake_update_engine_client_->reboot_after_update_call_count(), 1);

  EXPECT_EQ(fake_view_->ui_state(),
            UpdateRequiredView::UPDATE_COMPLETED_NEED_REBOOT);
}

}  // namespace chromeos
