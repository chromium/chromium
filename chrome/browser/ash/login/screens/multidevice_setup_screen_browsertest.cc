// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/multidevice_setup_screen.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/multidevice_setup_screen_handler.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "content/public/test/browser_test.h"

namespace ash {

constexpr test::UIPath kMultideviceSetupPath = {"multidevice-setup-screen",
                                                "multideviceSetup"};

class MultiDeviceSetupScreenTest : public OobeBaseTest {
 public:
  MultiDeviceSetupScreenTest() = default;
  ~MultiDeviceSetupScreenTest() override = default;

  void SetUpOnMainThread() override {
    MultiDeviceSetupScreen* screen = static_cast<MultiDeviceSetupScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            MultiDeviceSetupScreenView::kScreenId));
    screen->AddExitCallbackForTesting(base::BindRepeating(
        &MultiDeviceSetupScreenTest::HandleScreenExit, base::Unretained(this)));

    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    screen->set_multidevice_setup_client_for_testing(
        fake_multidevice_setup_client_.get());
    OobeBaseTest::SetUpOnMainThread();
  }

  void SimulateHostStatusChange() {
    multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
        host_status_with_device = multidevice_setup::MultiDeviceSetupClient::
            GenerateDefaultHostStatusWithDevice();
    host_status_with_device.first =
        multidevice_setup::mojom::HostStatus::kEligibleHostExistsButNoHostSet;
    fake_multidevice_setup_client_->SetHostStatusWithDevice(
        host_status_with_device);
  }

  void ShowMultiDeviceSetupScreen() {
    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    if (!screen_exited_) {
      LoginDisplayHost::default_host()->StartWizard(
          MultiDeviceSetupScreenView::kScreenId);
    }
  }

  void FinishDeviceSetup() {
    const std::string elementJS =
        test::GetOobeElementPath(kMultideviceSetupPath);
    test::OobeJS().Evaluate(
        elementJS + R"(.fire('setup-exited', {didUserCompleteSetup: true});)");
  }

  void CancelDeviceSetup() {
    const std::string elementJS =
        test::GetOobeElementPath(kMultideviceSetupPath);
    test::OobeJS().Evaluate(
        elementJS + R"(.fire('setup-exited', {didUserCompleteSetup: false});)");
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(MultiDeviceSetupScreenView::kScreenId).Wait();
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void CheckUserChoice(bool Accepted) {
    histogram_tester_.ExpectBucketCount<
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice>(
        "MultiDeviceSetup.OOBE.UserChoice",
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice::kAccepted,
        Accepted);
    histogram_tester_.ExpectBucketCount<
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice>(
        "MultiDeviceSetup.OOBE.UserChoice",
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice::kDeclined,
        !Accepted);
  }

  absl::optional<MultiDeviceSetupScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;

 private:
  void HandleScreenExit(MultiDeviceSetupScreen::Result result) {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest, Accepted) {
  SimulateHostStatusChange();
  ShowMultiDeviceSetupScreen();
  WaitForScreenShown();

  FinishDeviceSetup();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), MultiDeviceSetupScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Multidevice-setup-screen.Next", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTime.Multidevice-setup-screen", 1);
  CheckUserChoice(true);
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest, Declined) {
  SimulateHostStatusChange();
  ShowMultiDeviceSetupScreen();
  WaitForScreenShown();

  CancelDeviceSetup();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), MultiDeviceSetupScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Multidevice-setup-screen.Next", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTime.Multidevice-setup-screen", 1);
  CheckUserChoice(false);
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest, Skipped) {
  ShowMultiDeviceSetupScreen();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(),
            MultiDeviceSetupScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Multidevice-setup-screen.Next", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTime.Multidevice-setup-screen", 0);
}

}  // namespace ash
