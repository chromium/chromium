// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/multidevice_setup_screen.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/multidevice_setup_screen_handler.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
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
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    screen->set_device_sync_client_for_testing(fake_device_sync_client_.get());
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

  void CheckSkipped(bool should_be_skipped) {
    if (should_be_skipped) {
      EXPECT_EQ(screen_result_.value(),
                MultiDeviceSetupScreen::Result::NOT_APPLICABLE);
      histogram_tester_.ExpectBucketCount(
          "OOBE.StepShownStatus.Multidevice-setup-screen", true, 0);
      histogram_tester_.ExpectBucketCount(
          "OOBE.StepShownStatus.Multidevice-setup-screen", false, 1);
      return;
    }

    histogram_tester_.ExpectBucketCount(
        "OOBE.StepShownStatus.Multidevice-setup-screen", true, 1);
    histogram_tester_.ExpectBucketCount(
        "OOBE.StepShownStatus.Multidevice-setup-screen", false, 0);
  }

  void CheckHostPhoneAlreadySetSkippedReason() {
    multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
        host_status_with_device = multidevice_setup::MultiDeviceSetupClient::
            GenerateDefaultHostStatusWithDevice();
    host_status_with_device.first =
        multidevice_setup::mojom::HostStatus::kHostSetButNotYetVerified;
    fake_multidevice_setup_client_->SetHostStatusWithDevice(
        host_status_with_device);

    CheckSkippedReason(
        /*better_together_metadata_status=*/device_sync::
            BetterTogetherMetadataStatus::kMetadataDecrypted,
        /*group_private_key_status=*/
        device_sync::GroupPrivateKeyStatus::
            kGroupPrivateKeySuccessfullyDecrypted,
        /*expected_skipped_reason=*/
        MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason::
            kHostPhoneAlreadySet);
  }

  void CheckDeviceSyncFinishedAndNoEligibleHostPhoneSkippedReason() {
    CheckSkippedReason(
        /*better_together_metadata_status=*/device_sync::
            BetterTogetherMetadataStatus::kMetadataDecrypted,
        /*group_private_key_status=*/
        device_sync::GroupPrivateKeyStatus::
            kGroupPrivateKeySuccessfullyDecrypted,
        /*expected_skipped_reason=*/
        MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason::
            kDeviceSyncFinishedAndNoEligibleHostPhone);
  }

  void
  CheckDeviceSyncNotInitializedDuringBetterTogetherMetadataStatusFetchSkippedReason() {
    CheckSkippedReason(
        /*better_together_metadata_status=*/device_sync::
            BetterTogetherMetadataStatus::
                kStatusUnavailableBecauseDeviceSyncIsNotInitialized,
        /*group_private_key_status=*/
        device_sync::GroupPrivateKeyStatus::
            kStatusUnavailableBecauseDeviceSyncIsNotInitialized,
        /*expected_skipped_reason=*/
        MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason::
            kDeviceSyncNotInitializedDuringBetterTogetherMetadataStatusFetch);
  }

  void
  CheckDeviceSyncNotInitializedDuringGroupPrivateKeyStatusFetchSkippedReason() {
    CheckSkippedReason(
        /*better_together_metadata_status=*/device_sync::
            BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata,
        /*group_private_key_status=*/
        device_sync::GroupPrivateKeyStatus::
            kStatusUnavailableBecauseDeviceSyncIsNotInitialized,
        /*expected_skipped_reason=*/
        MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason::
            kDeviceSyncNotInitializedDuringGroupPrivateKeyStatusFetch);
  }

  void CheckEncryptedMetadataEmptySkippedReason() {
    CheckSkippedReason(
        /*better_together_metadata_status=*/device_sync::
            BetterTogetherMetadataStatus::kEncryptedMetadataEmpty,
        /*group_private_key_status=*/
        device_sync::GroupPrivateKeyStatus::
            kGroupPrivateKeySuccessfullyDecrypted,
        /*expected_skipped_reason=*/
        MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason::
            kEncryptedMetadataEmpty);
  }

  void CheckWaitingForGroupPrivateKeySkippedReason() {
    CheckSkippedReason(
        /*better_together_metadata_status=*/device_sync::
            BetterTogetherMetadataStatus::kGroupPrivateKeyMissing,
        /*group_private_key_status=*/
        device_sync::GroupPrivateKeyStatus::kWaitingForGroupPrivateKey,
        /*expected_skipped_reason=*/
        MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason::
            kWaitingForGroupPrivateKey);
  }

  void CheckNoEncryptedGroupPrivateKeyReceivedSkippedReason() {
    CheckSkippedReason(
        /*better_together_metadata_status=*/device_sync::
            BetterTogetherMetadataStatus::kGroupPrivateKeyMissing,
        /*group_private_key_status=*/
        device_sync::GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
        /*expected_skipped_reason=*/
        MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason::
            kNoEncryptedGroupPrivateKeyReceived);
  }

  void CheckEncryptedGroupPrivateKeyEmptySkippedReason() {
    CheckSkippedReason(
        /*better_together_metadata_status=*/device_sync::
            BetterTogetherMetadataStatus::kGroupPrivateKeyMissing,
        /*group_private_key_status=*/
        device_sync::GroupPrivateKeyStatus::kEncryptedGroupPrivateKeyEmpty,
        /*expected_skipped_reason=*/
        MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason::
            kEncryptedGroupPrivateKeyEmpty);
  }

  void CheckLocalDeviceSyncBetterTogetherKeyMissingSkippedReason() {
    CheckSkippedReason(
        /*better_together_metadata_status=*/device_sync::
            BetterTogetherMetadataStatus::kGroupPrivateKeyMissing,
        /*group_private_key_status=*/
        device_sync::GroupPrivateKeyStatus::
            kLocalDeviceSyncBetterTogetherKeyMissing,
        /*expected_skipped_reason=*/
        MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason::
            kLocalDeviceSyncBetterTogetherKeyMissing);
  }

  void CheckGroupPrivateKeyDecryptionFailedSkippedReason() {
    CheckSkippedReason(
        /*better_together_metadata_status=*/device_sync::
            BetterTogetherMetadataStatus::kGroupPrivateKeyMissing,
        /*group_private_key_status=*/
        device_sync::GroupPrivateKeyStatus::kGroupPrivateKeyDecryptionFailed,
        /*expected_skipped_reason=*/
        MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason::
            kGroupPrivateKeyDecryptionFailed);
  }

  void CheckUnknownSkippedReason() {
    // This combination of better together metadata status and group private key
    // status should never actually occur together
    CheckSkippedReason(
        /*better_together_metadata_status=*/device_sync::
            BetterTogetherMetadataStatus::kGroupPrivateKeyMissing,
        /*group_private_key_status=*/
        device_sync::GroupPrivateKeyStatus::
            kGroupPrivateKeySuccessfullyDecrypted,
        /*expected_skipped_reason=*/
        MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason::kUnknown);
  }

  std::optional<MultiDeviceSetupScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;

 private:
  void CheckSkippedReason(
      device_sync::BetterTogetherMetadataStatus better_together_metadata_status,
      device_sync::GroupPrivateKeyStatus group_private_key_status,
      MultiDeviceSetupScreen::OobeMultideviceScreenSkippedReason
          expected_skipped_reason) {
    CheckSkipped(/*should_be_skipped=*/true);
    EXPECT_TRUE(fake_device_sync_client_
                    ->GetBetterTogetherMetadataStatusCallbackQueueSize() > 0);
    fake_device_sync_client_
        ->InvokePendingGetBetterTogetherMetadataStatusCallback(
            better_together_metadata_status);

    // The screen should only attempt to fetch the group private key status when
    // the better together metadata status is kWaitingToProcessDeviceMetadata or
    // kGroupPrivateKeyMissing
    if (better_together_metadata_status ==
            device_sync::BetterTogetherMetadataStatus::
                kWaitingToProcessDeviceMetadata ||
        better_together_metadata_status ==
            device_sync::BetterTogetherMetadataStatus::
                kGroupPrivateKeyMissing) {
      EXPECT_TRUE(fake_device_sync_client_
                      ->GetGroupPrivateKeyStatusCallbackQueueSize() > 0);
      fake_device_sync_client_->InvokePendingGetGroupPrivateKeyStatusCallback(
          group_private_key_status);
    } else {
      EXPECT_FALSE(fake_device_sync_client_
                       ->GetGroupPrivateKeyStatusCallbackQueueSize() > 0);
    }

    histogram_tester_.ExpectBucketCount(
        "OOBE.StepShownStatus.Multidevice-setup-screen.Skipped",
        expected_skipped_reason, 1);
  }

  void HandleScreenExit(MultiDeviceSetupScreen::Result result) {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;

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
  CheckSkipped(/*should_be_skipped=*/false);
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
  CheckSkipped(/*should_be_skipped=*/false);
  CheckUserChoice(false);
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest,
                       SkippedReason_HostPhoneAlreadySet) {
  ShowMultiDeviceSetupScreen();
  WaitForScreenExit();
  CheckHostPhoneAlreadySetSkippedReason();
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest,
                       SkippedReason_DeviceSyncFinishedAndNoEligibleHostPhone) {
  ShowMultiDeviceSetupScreen();
  WaitForScreenExit();
  CheckDeviceSyncFinishedAndNoEligibleHostPhoneSkippedReason();
}

IN_PROC_BROWSER_TEST_F(
    MultiDeviceSetupScreenTest,
    SkippedReason_DeviceSyncNotInitializedDuringBetterTogetherMetadataStatusFetch) {
  ShowMultiDeviceSetupScreen();
  WaitForScreenExit();
  CheckDeviceSyncNotInitializedDuringBetterTogetherMetadataStatusFetchSkippedReason();
}

IN_PROC_BROWSER_TEST_F(
    MultiDeviceSetupScreenTest,
    SkippedReason_DeviceSyncNotInitializedDuringGroupPrivateKeyStatusFetch) {
  ShowMultiDeviceSetupScreen();
  WaitForScreenExit();
  CheckDeviceSyncNotInitializedDuringGroupPrivateKeyStatusFetchSkippedReason();
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest,
                       SkippedReason_EncryptedMetadataEmpty) {
  ShowMultiDeviceSetupScreen();
  WaitForScreenExit();
  CheckEncryptedMetadataEmptySkippedReason();
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest,
                       SkippedReason_WaitingForGroupPrivateKey) {
  ShowMultiDeviceSetupScreen();
  WaitForScreenExit();
  CheckWaitingForGroupPrivateKeySkippedReason();
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest,
                       SkippedReason_NoEncryptedGroupPrivateKeyReceived) {
  ShowMultiDeviceSetupScreen();
  WaitForScreenExit();
  CheckNoEncryptedGroupPrivateKeyReceivedSkippedReason();
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest,
                       SkippedReason_EncryptedGroupPrivateKeyEmpty) {
  ShowMultiDeviceSetupScreen();
  WaitForScreenExit();
  CheckEncryptedGroupPrivateKeyEmptySkippedReason();
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest,
                       SkippedReason_LocalDeviceSyncBetterTogetherKeyMissing) {
  ShowMultiDeviceSetupScreen();
  WaitForScreenExit();
  CheckLocalDeviceSyncBetterTogetherKeyMissingSkippedReason();
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest,
                       SkippedReason_GroupPrivateKeyDecryptionFailed) {
  ShowMultiDeviceSetupScreen();
  WaitForScreenExit();
  CheckGroupPrivateKeyDecryptionFailedSkippedReason();
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest, SkippedReason_Unknown) {
  ShowMultiDeviceSetupScreen();
  WaitForScreenExit();
  CheckUnknownSkippedReason();
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest,
                       NoQuickStartPhoneInstanceIdSet) {
  WizardContext* context =
      LoginDisplayHost::default_host()->GetWizardContextForTesting();
  ASSERT_TRUE(context->quick_start_phone_instance_id.empty());

  SimulateHostStatusChange();
  ShowMultiDeviceSetupScreen();
  WaitForScreenShown();
  EXPECT_TRUE(fake_multidevice_setup_client_->qs_phone_instance_id().empty());
}

IN_PROC_BROWSER_TEST_F(MultiDeviceSetupScreenTest,
                       QuickStartPhoneInstanceIdSet) {
  WizardContext* context =
      LoginDisplayHost::default_host()->GetWizardContextForTesting();
  ASSERT_TRUE(context->quick_start_phone_instance_id.empty());
  std::string expected_phone_instance_id = "someArbitraryID";
  context->quick_start_phone_instance_id = expected_phone_instance_id;

  SimulateHostStatusChange();
  ShowMultiDeviceSetupScreen();
  WaitForScreenShown();
  EXPECT_EQ(fake_multidevice_setup_client_->qs_phone_instance_id(),
            expected_phone_instance_id);
}

}  // namespace ash
