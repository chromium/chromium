// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/network_screen.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/chromeos/login/mock_network_state_helper.h"
#include "chrome/browser/chromeos/login/screens/mock_network_screen.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace chromeos {

class NetworkScreenUnitTest : public testing::Test {
 public:
  NetworkScreenUnitTest() = default;
  ~NetworkScreenUnitTest() override = default;

  // testing::Test:
  void SetUp() override {
    // Initialize the thread manager.
    DBusThreadManager::Initialize();

    // Configure the browser to use Hands-Off Enrollment.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");

    // Create the NetworkScreen we will use for testing.
    network_screen_ = std::make_unique<NetworkScreen>(
        &mock_view_,
        base::BindRepeating(&NetworkScreenUnitTest::HandleScreenExit,
                            base::Unretained(this)));
    mock_network_state_helper_ = new login::MockNetworkStateHelper();
    network_screen_->SetNetworkStateHelperForTest(mock_network_state_helper_);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
    network_screen_.reset();
    DBusThreadManager::Shutdown();
  }

 protected:
  // A pointer to the NetworkScreen.
  std::unique_ptr<NetworkScreen> network_screen_;

  login::MockNetworkStateHelper* mock_network_state_helper_ = nullptr;
  base::Optional<NetworkScreen::Result> last_screen_result_;

 private:
  void HandleScreenExit(NetworkScreen::Result screen_result) {
    EXPECT_FALSE(last_screen_result_.has_value());
    last_screen_result_ = screen_result;
  }

  // Test versions of core browser infrastructure.
  content::BrowserTaskEnvironment task_environment_;

  // More accessory objects needed by NetworkScreen.
  MockNetworkScreenView mock_view_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreenUnitTest);
};

TEST_F(NetworkScreenUnitTest, ContinuesAutomatically) {
  // Simulate a network connection.
  EXPECT_CALL(*mock_network_state_helper_, IsConnected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)));
  network_screen_->UpdateStatus();

  // Check that we continued once
  EXPECT_EQ(1, network_screen_->continue_attempts_);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(NetworkScreen::Result::CONNECTED, last_screen_result_.value());
}

TEST_F(NetworkScreenUnitTest, ContinuesOnlyOnce) {
  // Connect to network "net0".
  EXPECT_CALL(*mock_network_state_helper_, GetCurrentNetworkName())
      .Times(AnyNumber())
      .WillRepeatedly(Return(base::ASCIIToUTF16("net0")));
  EXPECT_CALL(*mock_network_state_helper_, IsConnected())
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  // Stop waiting for net0.
  network_screen_->StopWaitingForConnection(base::ASCIIToUTF16("net0"));

  // Check that we have continued exactly once.
  ASSERT_EQ(1, network_screen_->continue_attempts_);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(NetworkScreen::Result::CONNECTED, last_screen_result_.value());

  // Stop waiting for another network, net1.
  network_screen_->StopWaitingForConnection(base::ASCIIToUTF16("net1"));

  // Check that we have still continued only once.
  EXPECT_EQ(1, network_screen_->continue_attempts_);
}

}  // namespace chromeos
