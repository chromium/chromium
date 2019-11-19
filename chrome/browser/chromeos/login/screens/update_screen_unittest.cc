// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_screen.h"

#include "base/command_line.h"
#include "base/optional.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/chromeos/login/screens/mock_error_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_update_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
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

class UpdateScreenUnitTest : public testing::Test {
 public:
  UpdateScreenUnitTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}

  // Simulates an update being available (or not).
  // The parameter "update_screen" points to the currently active UpdateScreen.
  // The parameter "available" indicates whether an update is available.
  // The parameter "critical" indicates whether that update is critical.
  void SimulateUpdateAvailable(
      const std::unique_ptr<UpdateScreen>& update_screen,
      bool available,
      bool critical) {
    update_engine::StatusResult update_engine_status;
    update_engine_status.set_current_operation(
        update_engine::Operation::CHECKING_FOR_UPDATE);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(
        update_engine_status);

    if (critical) {
      ASSERT_TRUE(available) << "Does not make sense for an update to be "
                                "critical if one is not even available.";
      update_screen->set_ignore_update_deadlines_for_testing(true);
    }
    update_engine_status.set_current_operation(
        available ? update_engine::Operation::UPDATE_AVAILABLE
                  : update_engine::Operation::IDLE);

    fake_update_engine_client_->NotifyObserversThatStatusChanged(
        update_engine_status);
  }

  // testing::Test:
  void SetUp() override {
    // Configure the browser to use Hands-Off Enrollment.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");

    // Initialize objects needed by UpdateScreen.
    fake_update_engine_client_ = new FakeUpdateEngineClient();
    DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(fake_update_engine_client_));
    NetworkHandler::Initialize();
    mock_network_portal_detector_ = new MockNetworkPortalDetector();
    network_portal_detector::SetNetworkPortalDetector(
        mock_network_portal_detector_);
    mock_error_screen_.reset(new MockErrorScreen(&mock_error_view_));

    // Ensure proper behavior of UpdateScreen's supporting objects.
    EXPECT_CALL(*mock_network_portal_detector_, IsEnabled())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    update_screen_ = std::make_unique<UpdateScreen>(
        &mock_view_, mock_error_screen_.get(),
        base::BindRepeating(&UpdateScreenUnitTest::HandleScreenExit,
                            base::Unretained(this)));
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
    update_screen_.reset();
    mock_error_screen_.reset();
    network_portal_detector::Shutdown();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  // A pointer to the UpdateScreen used in this test.
  std::unique_ptr<UpdateScreen> update_screen_;

  // Accessory objects needed by UpdateScreen.
  MockUpdateView mock_view_;
  MockErrorScreenView mock_error_view_;
  std::unique_ptr<MockErrorScreen> mock_error_screen_;
  MockNetworkPortalDetector* mock_network_portal_detector_;
  FakeUpdateEngineClient* fake_update_engine_client_;

  base::Optional<UpdateScreen::Result> last_screen_result_;

 private:
  void HandleScreenExit(UpdateScreen::Result result) {
    EXPECT_FALSE(last_screen_result_.has_value());
    last_screen_result_ = result;
  }

  // Test versions of core browser infrastructure.
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreenUnitTest);
};

TEST_F(UpdateScreenUnitTest, HandlesNoUpdate) {
  // DUT reaches UpdateScreen.
  update_screen_->Show();

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  // No updates are available.
  SimulateUpdateAvailable(update_screen_, false /* available */,
                          false /* critical */);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());
}

TEST_F(UpdateScreenUnitTest, HandlesNonCriticalUpdate) {
  // DUT reaches UpdateScreen.
  update_screen_->Show();

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  // A non-critical update is available.
  SimulateUpdateAvailable(update_screen_, true /* available */,
                          false /* critical */);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());
}

TEST_F(UpdateScreenUnitTest, HandlesCriticalUpdate) {
  // DUT reaches UpdateScreen.
  update_screen_->Show();

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  // An update is available, and it's critical!
  SimulateUpdateAvailable(update_screen_, true /* available */,
                          true /* critical */);

  EXPECT_FALSE(last_screen_result_.has_value());
}

TEST_F(UpdateScreenUnitTest, HandleCriticalUpdateError) {
  // DUT reaches UpdateScreen.
  update_screen_->Show();

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  // An update is available, and it's critical!
  SimulateUpdateAvailable(update_screen_, true /* available */,
                          true /* critical */);

  EXPECT_FALSE(last_screen_result_.has_value());

  update_engine::StatusResult update_engine_status;
  update_engine_status.set_current_operation(
      update_engine::Operation::REPORTING_ERROR_EVENT);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(
      update_engine_status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_ERROR, last_screen_result_.value());
}

}  // namespace chromeos
