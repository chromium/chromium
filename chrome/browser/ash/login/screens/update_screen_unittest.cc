// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/update_screen.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/ash/login/screens/mock_error_screen.h"
#include "chrome/browser/ash/login/screens/mock_update_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/portal_detector/mock_network_portal_detector.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

class UpdateScreenUnitTest : public testing::Test {
 public:
  UpdateScreenUnitTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}

  UpdateScreenUnitTest(const UpdateScreenUnitTest&) = delete;
  UpdateScreenUnitTest& operator=(const UpdateScreenUnitTest&) = delete;

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
      update_engine_status.set_update_urgency(
          update_engine::UpdateUrgency::CRITICAL);
    }
    update_engine_status.set_current_operation(
        available ? update_engine::Operation::UPDATE_AVAILABLE
                  : update_engine::Operation::IDLE);

    fake_update_engine_client_->NotifyObserversThatStatusChanged(
        update_engine_status);
  }

  // testing::Test:
  void SetUp() override {
    // Initialize objects needed by UpdateScreen.
    wizard_context_ = std::make_unique<WizardContext>();
    chromeos::PowerManagerClient::InitializeFake();
    fake_update_engine_client_ = UpdateEngineClient::InitializeFakeForTest();
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    mock_network_portal_detector_ = new MockNetworkPortalDetector();
    network_portal_detector::SetNetworkPortalDetector(
        mock_network_portal_detector_);
    mock_error_screen_ =
        std::make_unique<MockErrorScreen>(mock_error_view_.AsWeakPtr());

    // Ensure proper behavior of UpdateScreen's supporting objects.
    EXPECT_CALL(*mock_network_portal_detector_, IsEnabled())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    update_screen_ = std::make_unique<UpdateScreen>(
        mock_view_.AsWeakPtr(), mock_error_screen_.get(),
        base::BindRepeating(&UpdateScreenUnitTest::HandleScreenExit,
                            base::Unretained(this)));
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
    update_screen_.reset();
    mock_error_screen_.reset();
    network_portal_detector::Shutdown();
    network_handler_test_helper_.reset();
    chromeos::PowerManagerClient::Shutdown();
    UpdateEngineClient::Shutdown();
  }

  // Fast forwards time by the specified amount.
  void FastForwardTime(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

 protected:
  // A pointer to the UpdateScreen used in this test.
  std::unique_ptr<UpdateScreen> update_screen_;

  // Accessory objects needed by UpdateScreen.
  MockUpdateView mock_view_;
  MockErrorScreenView mock_error_view_;
  std::unique_ptr<MockErrorScreen> mock_error_screen_;
  raw_ptr<MockNetworkPortalDetector, DanglingUntriaged>
      mock_network_portal_detector_;
  raw_ptr<FakeUpdateEngineClient, DanglingUntriaged> fake_update_engine_client_;
  std::unique_ptr<WizardContext> wizard_context_;

  std::optional<UpdateScreen::Result> last_screen_result_;

 private:
  void HandleScreenExit(UpdateScreen::Result result) {
    EXPECT_FALSE(last_screen_result_.has_value());
    last_screen_result_ = result;
  }

  // Test versions of core browser infrastructure.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingLocalState local_state_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
};

TEST_F(UpdateScreenUnitTest, HandlesNoUpdate) {
  // DUT reaches UpdateScreen.
  update_screen_->Show(wizard_context_.get());

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
  update_screen_->Show(wizard_context_.get());

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
  update_screen_->Show(wizard_context_.get());

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  // An update is available, and it's critical!
  SimulateUpdateAvailable(update_screen_, true /* available */,
                          true /* critical */);

  EXPECT_FALSE(last_screen_result_.has_value());
}

TEST_F(UpdateScreenUnitTest, HandleCriticalUpdateError) {
  // DUT reaches UpdateScreen.
  update_screen_->Show(wizard_context_.get());

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  // An update is available, and it's critical!
  SimulateUpdateAvailable(update_screen_, true /* available */,
                          true /* critical */);

  EXPECT_FALSE(last_screen_result_.has_value());

  update_engine::StatusResult update_engine_status;
  update_engine_status.set_current_operation(
      update_engine::Operation::REPORTING_ERROR_EVENT);
  update_engine_status.set_update_urgency(
      update_engine::UpdateUrgency::CRITICAL);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(
      update_engine_status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_ERROR, last_screen_result_.value());
}

TEST_F(UpdateScreenUnitTest, RetryCheckforUpdateElapsed) {
  // DUT reaches UpdateScreen.
  update_screen_->Show(wizard_context_.get());

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  FastForwardTime(base::Seconds(185));

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_CHECK_TIMEOUT,
            last_screen_result_.value());
}

}  // namespace ash
