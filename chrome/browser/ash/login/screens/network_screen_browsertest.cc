// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/mock_network_state_helper.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/views/controls/button/button.h"

namespace ash {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::ReturnRef;
using ::views::Button;

constexpr char kCancelButton[] = "cancelButton";
constexpr char kLoadingDialog[] = "loadingDialog";
constexpr char kQuickStartButton[] = "quickStart";
constexpr char kNextButton[] = "nextButton";
constexpr test::UIPath kCancelButtonLoadingDialog = {
    QuickStartView::kScreenId.name, kLoadingDialog, kCancelButton};
constexpr test::UIPath kQuickStartNetworkButtonPath = {
    NetworkScreenView::kScreenId.name /*"network-selection"*/,
    kQuickStartButton};
constexpr test::UIPath kNextNetworkButtonPath = {
    NetworkScreenView::kScreenId.name /*"network-selection"*/, kNextButton};

class NetworkScreenTest : public OobeBaseTest {
 public:
  NetworkScreenTest() {
    needs_network_screen_skip_check_ = true;
  }

  NetworkScreenTest(const NetworkScreenTest&) = delete;
  NetworkScreenTest& operator=(const NetworkScreenTest&) = delete;

  ~NetworkScreenTest() override = default;

  void SetUpOnMainThread() override {
    network_screen_ = static_cast<NetworkScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            NetworkScreenView::kScreenId));
    network_screen_->set_exit_callback_for_testing(base::BindRepeating(
        &NetworkScreenTest::HandleScreenExit, base::Unretained(this)));
    ASSERT_TRUE(network_screen_->view_ != nullptr);

    mock_network_state_helper_ = new login::MockNetworkStateHelper();
    SetDefaultNetworkStateHelperExpectations();
    network_screen_->SetNetworkStateHelperForTest(mock_network_state_helper_);
    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowNetworkScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        NetworkScreenView::kScreenId);
  }

  void EmulateContinueButtonExit(NetworkScreen* network_screen) {
    EXPECT_CALL(*network_state_helper(), IsConnected()).WillOnce(Return(true));
    network_screen->OnContinueButtonClicked();
    base::RunLoop().RunUntilIdle();

    CheckResult(NetworkScreen::Result::CONNECTED);
  }

  void SetDefaultNetworkStateHelperExpectations() {
    EXPECT_CALL(*network_state_helper(), GetCurrentNetworkName())
        .Times(AnyNumber())
        .WillRepeatedly((Return(std::u16string())));
    EXPECT_CALL(*network_state_helper(), IsConnected())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
    EXPECT_CALL(*network_state_helper(), IsConnecting())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
    EXPECT_CALL(*network_state_helper(), IsConnectedToEthernet())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void CheckResult(NetworkScreen::Result result) {
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(last_screen_result_.value(), result);
  }

  login::MockNetworkStateHelper* network_state_helper() {
    return mock_network_state_helper_;
  }

  NetworkScreen* network_screen() { return network_screen_; }

  void EnableQuickStartFeature() {
    feature_list_.Reset();
    feature_list_.InitAndEnableFeature(features::kOobeQuickStart);
  }

  base::HistogramTester histogram_tester_;

 private:
  void HandleScreenExit(NetworkScreen::Result result) {
    screen_exited_ = true;
    last_screen_result_ = result;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  raw_ptr<login::MockNetworkStateHelper, DanglingUntriaged | ExperimentalAsh>
      mock_network_state_helper_;
  raw_ptr<NetworkScreen, DanglingUntriaged | ExperimentalAsh> network_screen_;
  bool screen_exited_ = false;
  base::test::ScopedFeatureList feature_list_;
  absl::optional<NetworkScreen::Result> last_screen_result_;
  base::RepeatingClosure screen_exit_callback_;
};

class NetworkScreenQuickStartEnabled : public NetworkScreenTest {
 public:
  NetworkScreenQuickStartEnabled() {
    this->EnableQuickStartFeature();
    connection_broker_factory_.set_initial_feature_support_status(
        quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
            kUndetermined);
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
    quick_start::TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
        &connection_broker_factory_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    quick_start::TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
        nullptr);
    OobeBaseTest::TearDownInProcessBrowserTestFixture();
  }

  void EnterQuickStartFlowFromNetworkScreen() {
    // Open network screen
    ShowNetworkScreen();
    WaitForScreenShown();
    test::OobeJS().ExpectHiddenPath(kQuickStartNetworkButtonPath);

    connection_broker_factory_.instances().front()->set_feature_support_status(
        quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
            kSupported);

    // Check that QuickStart button is visible since QuickStart feature is
    // enabled
    test::OobeJS()
        .CreateVisibilityWaiter(/*visibility=*/true,
                                kQuickStartNetworkButtonPath)
        ->Wait();

    test::OobeJS().ClickOnPath(kQuickStartNetworkButtonPath);

    WaitForScreenExit();

    CheckResult(NetworkScreen::Result::QUICK_START);
  }

  quick_start::FakeTargetDeviceConnectionBroker::Factory
      connection_broker_factory_;
};

IN_PROC_BROWSER_TEST_F(NetworkScreenQuickStartEnabled,
                       QuickStartButtonNotShownByDefault) {
  // Open network screen
  ShowNetworkScreen();
  WaitForScreenShown();

  test::OobeJS().CreateVisibilityWaiter(true, kNextNetworkButtonPath)->Wait();

  // Check that QuickStart button is hidden since QuickStart feature is not
  // enabled
  test::OobeJS().ExpectHiddenPath(kQuickStartNetworkButtonPath);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenQuickStartEnabled,
                       QuickStartButtonFunctionalWhenFeatureEnabled) {
  EnterQuickStartFlowFromNetworkScreen();
}

IN_PROC_BROWSER_TEST_F(NetworkScreenQuickStartEnabled,
                       ClickingCancelReturnsToNetwork) {
  EnterQuickStartFlowFromNetworkScreen();

  // Cancel button must be present.
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kCancelButtonLoadingDialog)
      ->Wait();
  test::OobeJS().ClickOnPath(kCancelButtonLoadingDialog);
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, CanConnect) {
  ShowNetworkScreen();
  EXPECT_CALL(*network_state_helper(), IsConnecting()).WillOnce((Return(true)));
  // EXPECT_FALSE(view_->IsContinueEnabled());
  network_screen()->UpdateStatus();

  EXPECT_CALL(*network_state_helper(), IsConnected())
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  // TODO(nkostylev): Add integration with WebUI view http://crosbug.com/22570
  // EXPECT_FALSE(view_->IsContinueEnabled());
  // EXPECT_FALSE(view_->IsConnecting());
  network_screen()->UpdateStatus();

  // EXPECT_TRUE(view_->IsContinueEnabled());
  EmulateContinueButtonExit(network_screen());
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Timeout) {
  ShowNetworkScreen();
  EXPECT_CALL(*network_state_helper(), IsConnecting()).WillOnce((Return(true)));
  // EXPECT_FALSE(view_->IsContinueEnabled());
  network_screen()->UpdateStatus();

  EXPECT_CALL(*network_state_helper(), IsConnected())
      .Times(AnyNumber())
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

// The network screen should be skipped if the device can connect and it's using
// zero-touch hands-off enrollment.
IN_PROC_BROWSER_TEST_F(NetworkScreenTest, HandsOffCanConnect_Skipped) {
  // Configure the UI to use Hands-Off Enrollment flow. This cannot be done in
  // the `SetUpCommandLine` method, because the welcome screen would also be
  // skipped, causing the network screen to be shown before we could set up this
  // test class properly.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");

  ShowNetworkScreen();

  EXPECT_CALL(*network_state_helper(), IsConnecting()).WillOnce((Return(true)));

  network_screen()->UpdateStatus();

  EXPECT_CALL(*network_state_helper(), IsConnected())
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  network_screen()->UpdateStatus();

  WaitForScreenExit();
  CheckResult(NetworkScreen::Result::CONNECTED);
}

// The network screen should NOT be skipped if the connection times out, even if
// it's using zero-touch hands-off enrollment.
IN_PROC_BROWSER_TEST_F(NetworkScreenTest, HandsOffTimeout_NotSkipped) {
  // Configure the UI to use Hands-Off Enrollment flow. This cannot be done in
  // the `SetUpCommandLine` method, because the welcome screen would also be
  // skipped, causing the network screen to be shown before we could set up this
  // test class properly.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");

  ShowNetworkScreen();

  EXPECT_CALL(*network_state_helper(), IsConnecting()).WillOnce((Return(true)));

  network_screen()->UpdateStatus();

  EXPECT_CALL(*network_state_helper(), IsConnected())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  network_screen()->OnConnectionTimeout();
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, EthernetConnection_Skipped) {
  EXPECT_CALL(*network_state_helper(), IsConnected())
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*network_state_helper(), IsConnectedToEthernet())
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)));

  ShowNetworkScreen();
  WaitForScreenExit();

  CheckResult(NetworkScreen::Result::NOT_APPLICABLE);

  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Network-selection.Connected", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Network-selection.OfflineDemoSetup",
      0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Network-selection.Back", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTime.Network-selection", 0);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.StepShownStatus.Network-selection"),
      ElementsAre(base::Bucket(
          static_cast<int>(OobeMetricsHelper::ScreenShownStatus::kSkipped),
          1)));
  // Showing screen again to test skip doesn't work now.
  ShowNetworkScreen();
  WaitForScreenShown();
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, DelayedEthernetConnection_Skipped) {
  ShowNetworkScreen();

  EXPECT_CALL(*network_state_helper(), IsConnecting()).WillOnce((Return(true)));

  network_screen()->UpdateStatus();

  EXPECT_CALL(*network_state_helper(), IsConnected())
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*network_state_helper(), IsConnectedToEthernet())
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)));

  network_screen()->UpdateStatus();
  WaitForScreenExit();

  CheckResult(NetworkScreen::Result::CONNECTED);

  // Showing screen again to test skip doesn't work now.
  ShowNetworkScreen();
  WaitForScreenShown();
}

}  // namespace ash
