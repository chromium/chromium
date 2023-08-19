// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/consumer_update_screen.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/version_updater/version_updater.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_test_helpers.h"
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "dbus/object_path.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {
namespace {

const test::UIPath kCellularPermissionDialog = {"consumer-update",
                                                "consumerUpdateCellularDialog"};
const test::UIPath kUpdateChekingDialog = {"consumer-update",
                                           "consumerUpdateCheckingDialog"};
const test::UIPath kUpdateInProgressDialog = {"consumer-update",
                                              "consumerUpdateInProgressDialog"};
const test::UIPath kUpdateRebootDialog = {"consumer-update",
                                          "consumerUpdateRestartingDialog"};
const test::UIPath kUpdateCellularAcceptButton = {"consumer-update",
                                                  "acceptButton"};
const test::UIPath kUpdateCellularDeclineButton = {"consumer-update",
                                                   "declineButton"};
const test::UIPath kLowBatteryWarningMessage = {"consumer-update",
                                                "battery-warning"};

OobeUI* GetOobeUI() {
  auto* host = LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

constexpr char kWifiServicePath[] = "/service/wifi2";

void ErrorCallbackFunction(base::OnceClosure run_loop_quit_closure,
                           const std::string& error_name,
                           const std::string& error_message) {
  std::move(run_loop_quit_closure).Run();
  FAIL() << "Shill Error: " << error_name << " : " << error_message;
}

void SetConnected(const std::string& service_path) {
  base::RunLoop run_loop;
  ShillServiceClient::Get()->Connect(
      dbus::ObjectPath(service_path), run_loop.QuitWhenIdleClosure(),
      base::BindOnce(&ErrorCallbackFunction, run_loop.QuitClosure()));
  run_loop.Run();
}

class ConsumerUpdateScreenTest : public OobeBaseTest {
 public:
  ConsumerUpdateScreenTest() {
    feature_list_.InitAndEnableFeature(ash::features::kOobeSoftwareUpdate);
  }

  ~ConsumerUpdateScreenTest() override = default;
  ConsumerUpdateScreenTest(const ConsumerUpdateScreenTest&) = delete;
  ConsumerUpdateScreenTest& operator=(const ConsumerUpdateScreenTest&) = delete;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    error_screen_ = GetOobeUI()->GetErrorScreen();
    consumer_update_screen_ = WizardController::default_controller()
                                  ->GetScreen<ConsumerUpdateScreen>();
    consumer_update_screen_->set_exit_callback_for_testing(base::BindRepeating(
        &ConsumerUpdateScreenTest::HandleScreenExit, base::Unretained(this)));
    version_updater_ =
        consumer_update_screen_->get_version_updater_for_testing();

    // Set up fake networks.
    network_state_test_helper_ = std::make_unique<NetworkStateTestHelper>(
        true /*use_default_devices_and_services*/);
    network_state_test_helper_->manager_test()->SetupDefaultEnvironment();
    // Fake networks have been set up. Connect to WiFi network.
    SetConnected(kWifiServicePath);

    // Waiting for update screen to be shown might take a long time on some test
    // build and the timer might be fired already. Increase the delay and call
    // fire from the test instead.
    consumer_update_screen_->set_delay_for_delayed_timer_for_testing(
        base::TimeDelta::Max());

    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->is_branded_build = true;
  }

  void TearDownOnMainThread() override {
    network_state_test_helper_.reset();
    consumer_update_screen_ = nullptr;
    version_updater_ = nullptr;
    error_screen_ = nullptr;
    OobeBaseTest::TearDownOnMainThread();
  }

  void SetUpdateEngineStatus(update_engine::Operation operation) {
    update_engine::StatusResult status;
    status.set_current_operation(operation);
    update_engine_client()->set_default_status(status);
    update_engine_client()->NotifyObserversThatStatusChanged(status);
  }

  void SetUpdateEngineStatusWithProgress(update_engine::Operation operation,
                                         double progress) {
    update_engine::StatusResult status;
    status.set_progress(progress);
    status.set_current_operation(operation);
    update_engine_client()->set_default_status(status);
    update_engine_client()->NotifyObserversThatStatusChanged(status);
  }

  void SetNetworkState(const std::string& service_path,
                       const std::string& state) {
    network_state_test_helper_->service_test()->SetServiceProperty(
        service_path, shill::kStateProperty, base::Value(state));
  }

  void ShowConsumerUpdateScreen() {
    WaitForOobeUI();
    WizardController::default_controller()->AdvanceToScreen(
        ConsumerUpdateScreenView::kScreenId);
  }

 protected:
  void WaitForScreenResult() {
    if (screen_result_.has_value()) {
      return;
    }
    base::test::TestFuture<void> waiter;
    screen_callback_ = waiter.GetCallback();
    EXPECT_TRUE(waiter.Wait());
  }

  chromeos::FakePowerManagerClient* power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

  raw_ptr<ConsumerUpdateScreen> consumer_update_screen_ = nullptr;
  ;
  raw_ptr<VersionUpdater> version_updater_ = nullptr;
  raw_ptr<ErrorScreen> error_screen_ = nullptr;

  // Handles network connections
  std::unique_ptr<NetworkStateTestHelper> network_state_test_helper_;

  absl::optional<ConsumerUpdateScreen::Result> screen_result_;

  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

 private:
  void HandleScreenExit(ConsumerUpdateScreen::Result result) {
    EXPECT_FALSE(screen_result_.has_value());
    screen_result_ = result;

    if (screen_callback_) {
      std::move(screen_callback_).Run();
    }
  }

  base::OnceClosure screen_callback_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ConsumerUpdateScreenTest, NoUpdateAvailable) {
  ShowConsumerUpdateScreen();

  SetUpdateEngineStatus(update_engine::Operation::IDLE);
  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);

  OobeScreenWaiter consumer_update_screen_waiter(
      ConsumerUpdateScreenView::kScreenId);
  consumer_update_screen_waiter.set_assert_next_screen();
  consumer_update_screen_waiter.Wait();

  test::OobeJS().ExpectVisiblePath(kUpdateChekingDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);

  SetUpdateEngineStatus(update_engine::Operation::IDLE);

  ASSERT_TRUE(screen_result_.has_value());
  EXPECT_EQ(ConsumerUpdateScreen::Result::UPDATE_NOT_REQUIRED,
            screen_result_.value());
}

// TODO(b/293419661) create function SimulateUpdateAvailable
IN_PROC_BROWSER_TEST_F(ConsumerUpdateScreenTest, UpdateAvailable) {
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::REGULAR);
  ShowConsumerUpdateScreen();

  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  status.set_new_version("latest and greatest");
  status.set_new_size(1'000'000'000);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  OobeScreenWaiter update_screen_waiter(ConsumerUpdateScreenView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisiblePath(kUpdateChekingDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateRebootDialog);

  SetUpdateEngineStatusWithProgress(update_engine::Operation::UPDATE_AVAILABLE,
                                    0.0);

  SetUpdateEngineStatusWithProgress(update_engine::Operation::DOWNLOADING, 0.0);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdateInProgressDialog)->Wait();
  test::OobeJS().ExpectHiddenPath(kUpdateChekingDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateRebootDialog);

  SetUpdateEngineStatusWithProgress(update_engine::Operation::DOWNLOADING,
                                    0.08);

  SetUpdateEngineStatusWithProgress(update_engine::Operation::VERIFYING, 1.0);

  test::OobeJS().ExpectVisiblePath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateChekingDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateRebootDialog);

  SetUpdateEngineStatus(update_engine::Operation::FINALIZING);

  test::OobeJS().ExpectVisiblePath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateChekingDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateRebootDialog);

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdateRebootDialog)->Wait();
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateChekingDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);
}

IN_PROC_BROWSER_TEST_F(ConsumerUpdateScreenTest, UpdateOverCellularAccepted) {
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::REGULAR);
  status.set_current_operation(
      update_engine::Operation::NEED_PERMISSION_TO_UPDATE);
  status.set_new_version("latest and greatest");

  ShowConsumerUpdateScreen();

  SetUpdateEngineStatus(update_engine::Operation::NEED_PERMISSION_TO_UPDATE);

  OobeScreenWaiter update_screen_waiter(ConsumerUpdateScreenView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisiblePath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateChekingDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateChekingDialog);

  test::OobeJS().TapOnPath(kUpdateCellularAcceptButton);

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdateChekingDialog)->Wait();
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectVisiblePath(kUpdateChekingDialog);
}

IN_PROC_BROWSER_TEST_F(ConsumerUpdateScreenTest, UpdateOverCellularDecline) {
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::REGULAR);
  status.set_current_operation(
      update_engine::Operation::NEED_PERMISSION_TO_UPDATE);
  status.set_new_version("latest and greatest");

  ShowConsumerUpdateScreen();

  SetUpdateEngineStatus(update_engine::Operation::NEED_PERMISSION_TO_UPDATE);

  OobeScreenWaiter update_screen_waiter(ConsumerUpdateScreenView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisiblePath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateChekingDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateChekingDialog);

  test::OobeJS().TapOnPath(kUpdateCellularDeclineButton);
  ASSERT_TRUE(screen_result_.has_value());
  EXPECT_EQ(ConsumerUpdateScreen::Result::DECLINE_CELLULAR,
            screen_result_.value());
}

IN_PROC_BROWSER_TEST_F(ConsumerUpdateScreenTest, LostNetworkDuringUpdate) {
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::REGULAR);
  ShowConsumerUpdateScreen();

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);

  OobeScreenWaiter update_screen_waiter(ConsumerUpdateScreenView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdateInProgressDialog)->Wait();

  network_portal_detector_.SimulateNoNetwork();

  ASSERT_EQ(ConsumerUpdateScreenView::kScreenId.AsId(),
            error_screen_->GetParentScreen());

  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  error_screen_waiter.set_assert_next_screen();
  error_screen_waiter.Wait();

  test::OobeJS().ExpectVisiblePath({"error-message"});
  test::OobeJS().ExpectVisiblePath({"error-message", "offlineMessageBody"});
}

IN_PROC_BROWSER_TEST_F(ConsumerUpdateScreenTest, LowBatteryStatus) {
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::REGULAR);

  // Set low battery and discharging status before oobe-update screen is shown.
  power_manager::PowerSupplyProperties props;
  props.set_battery_percent(30);
  props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  power_manager_client()->UpdatePowerProperties(props);

  ShowConsumerUpdateScreen();
  EXPECT_TRUE(power_manager_client()->HasObserver(consumer_update_screen_));

  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);

  // Warning message is shown while not charging and battery is low.
  test::OobeJS().ExpectVisiblePath(kLowBatteryWarningMessage);
}

}  // namespace
}  // namespace ash
