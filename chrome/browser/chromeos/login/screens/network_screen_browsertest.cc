// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/network_screen.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/mock_network_state_helper.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/views/controls/button/button.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnRef;
using views::Button;

namespace chromeos {

class DummyButtonListener : public views::ButtonListener {
 public:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {}
};

class NetworkScreenTest : public InProcessBrowserTest {
 public:
  NetworkScreenTest() = default;
  ~NetworkScreenTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendArg(switches::kLoginManager);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ShowLoginWizard(NetworkScreenView::kScreenId);
    network_screen_ = NetworkScreen::Get(
        WizardController::default_controller()->screen_manager());
    ASSERT_EQ(WizardController::default_controller()->current_screen(),
              network_screen_);
    network_screen_->set_exit_callback_for_testing(base::BindRepeating(
        &NetworkScreenTest::HandleScreenExit, base::Unretained(this)));
    ASSERT_TRUE(network_screen_->view_ != nullptr);

    mock_network_state_helper_ = new login::MockNetworkStateHelper;
    SetDefaultNetworkStateHelperExpectations();
    network_screen_->SetNetworkStateHelperForTest(mock_network_state_helper_);
  }

  void EmulateContinueButtonExit(NetworkScreen* network_screen) {
    EXPECT_CALL(*network_state_helper(), IsConnected()).WillOnce(Return(true));
    network_screen->OnContinueButtonClicked();
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(NetworkScreen::Result::CONNECTED, last_screen_result_.value());
  }

  void SetDefaultNetworkStateHelperExpectations() {
    EXPECT_CALL(*network_state_helper(), GetCurrentNetworkName())
        .Times(AnyNumber())
        .WillRepeatedly((Return(base::string16())));
    EXPECT_CALL(*network_state_helper(), IsConnected())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
    EXPECT_CALL(*network_state_helper(), IsConnecting())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
  }

  login::MockNetworkStateHelper* network_state_helper() {
    return mock_network_state_helper_;
  }
  NetworkScreen* network_screen() { return network_screen_; }

 private:
  void HandleScreenExit(NetworkScreen::Result result) {
    EXPECT_FALSE(last_screen_result_.has_value());
    last_screen_result_ = result;
  }

  login::MockNetworkStateHelper* mock_network_state_helper_;
  NetworkScreen* network_screen_;
  base::Optional<NetworkScreen::Result> last_screen_result_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreenTest);
};

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, CanConnect) {
  EXPECT_CALL(*network_state_helper(), IsConnecting()).WillOnce((Return(true)));
  // EXPECT_FALSE(view_->IsContinueEnabled());
  network_screen()->UpdateStatus();

  EXPECT_CALL(*network_state_helper(), IsConnected())
      .Times(2)
      .WillRepeatedly(Return(true));
  // TODO(nkostylev): Add integration with WebUI view http://crosbug.com/22570
  // EXPECT_FALSE(view_->IsContinueEnabled());
  // EXPECT_FALSE(view_->IsConnecting());
  network_screen()->UpdateStatus();

  // EXPECT_TRUE(view_->IsContinueEnabled());
  EmulateContinueButtonExit(network_screen());
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Timeout) {
  EXPECT_CALL(*network_state_helper(), IsConnecting()).WillOnce((Return(true)));
  // EXPECT_FALSE(view_->IsContinueEnabled());
  network_screen()->UpdateStatus();

  EXPECT_CALL(*network_state_helper(), IsConnected())
      .Times(2)
      .WillRepeatedly(Return(false));
  // TODO(nkostylev): Add integration with WebUI view http://crosbug.com/22570
  // EXPECT_FALSE(view_->IsContinueEnabled());
  // EXPECT_FALSE(view_->IsConnecting());
  network_screen()->OnConnectionTimeout();

  // Close infobubble with error message - it makes the test stable.
  // EXPECT_FALSE(view_->IsContinueEnabled());
  // EXPECT_FALSE(view_->IsConnecting());
  // view_->ClearErrors();
}

}  // namespace chromeos
