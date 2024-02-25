// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/network_screen.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/ash/login/mock_network_state_helper.h"
#include "chrome/browser/ash/login/screens/mock_network_screen.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

class NetworkScreenUnitTest : public testing::Test {
 public:
  NetworkScreenUnitTest() = default;

  NetworkScreenUnitTest(const NetworkScreenUnitTest&) = delete;
  NetworkScreenUnitTest& operator=(const NetworkScreenUnitTest&) = delete;

  ~NetworkScreenUnitTest() override = default;

  // testing::Test:
  void SetUp() override {
    // Create the NetworkScreen we will use for testing.
    network_screen_ = std::make_unique<NetworkScreen>(
        std::move(mock_view_),
        base::BindRepeating(&NetworkScreenUnitTest::HandleScreenExit,
                            base::Unretained(this)));
    mock_network_state_helper_ = new login::MockNetworkStateHelper();
    network_screen_->SetNetworkStateHelperForTest(mock_network_state_helper_);
    EXPECT_CALL(*mock_network_state_helper_, IsConnectedToEthernet())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
    network_screen_.reset();
  }

 protected:
  // A pointer to the NetworkScreen.
  std::unique_ptr<NetworkScreen> network_screen_;

  raw_ptr<login::MockNetworkStateHelper, DanglingUntriaged>
      mock_network_state_helper_ = nullptr;
  std::optional<NetworkScreen::Result> last_screen_result_;

 private:
  void HandleScreenExit(NetworkScreen::Result screen_result) {
    EXPECT_FALSE(last_screen_result_.has_value());
    last_screen_result_ = screen_result;
  }

  // Test versions of core browser infrastructure.
  content::BrowserTaskEnvironment task_environment_;

  // More accessory objects needed by NetworkScreen.
  base::WeakPtr<NetworkScreenView> mock_view_;
};

TEST_F(NetworkScreenUnitTest, ContinuesOnUserAction) {
  // Simulate a network connection.
  EXPECT_CALL(*mock_network_state_helper_, IsConnected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)));
  network_screen_->UpdateStatus();
  network_screen_->OnContinueButtonClicked();

  EXPECT_EQ(last_screen_result_, NetworkScreen::Result::CONNECTED);
}

}  // namespace ash
