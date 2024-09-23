// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICKSTART_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICKSTART_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-forward.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-shared.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::quick_start {

class QuickStartMetrics;

// Main orchestrator of the QuickStart flow in OOBE
//
// QuickStartController holds all the logic for QuickStart and acts as the
// source of truth for what the UI (QuickStartScreen) should be showing. Unlike
// other OOBE screens, QuickStartScreen just acts as a delegate for this main
// controller.
class QuickStartController
    : public OobeUI::Observer,
      public TargetDeviceBootstrapController::Observer,
      public bluetooth_config::mojom::SystemPropertiesObserver {
 public:
  using AbortFlowReason = QuickStartMetrics::AbortFlowReason;
  using EntryPoint = QuickStartMetrics::EntryPoint;

  // Main state used by the controller.
  // TODO(b:283965994) - Finalize states.
  enum class ControllerState {
    NOT_ACTIVE,
    WAITING_FOR_BLUETOOTH_PERMISSION,
    WAITING_FOR_BLUETOOTH_ACTIVATION,
    WAITING_TO_RESUME_AFTER_UPDATE,
    INITIALIZING,
    ADVERTISING,
    CONNECTED,
    // TODO(b:283965994) - Replace with more appropriate state.
    CONTINUING_AFTER_ENROLLMENT_CHECKS,
    FALLBACK_URL_FLOW_ON_GAIA_SCREEN,
    SETUP_COMPLETE,
  };

  // Implemented by the QuickStartScreen
  class UiDelegate : public base::CheckedObserver {
   public:
    // UI State that is used for dictating what the QuickStartScreen should
    // show.
    enum class UiState {
      SHOWING_BLUETOOTH_DIALOG,
      CONNECTING_TO_PHONE,
      SHOWING_QR,
      SHOWING_PIN,
      CONNECTING_TO_WIFI,
      WIFI_CREDENTIALS_RECEIVED,
      CONFIRM_GOOGLE_ACCOUNT,
      SIGNING_IN,
      // Same state as 'SIGNING_IN' but without the 'Cancel' button.
      CREATING_ACCOUNT,
      // Triggers a screen exit into the Gaia screen for the fallback URL flow.
      FALLBACK_URL_FLOW,
      SETUP_COMPLETE,
      // Exits the screen.
      EXIT_SCREEN,
    };

    virtual void OnUiUpdateRequested(UiState desired_state) = 0;

   protected:
    ~UiDelegate() override = default;
  };

  // For showing the user information on the UI
  struct UserInfo {
    std::string email = "";
    std::string full_name = "";
    std::string avatar_url = "";
  };

  // EntryPointButtonVisibilityCallback is a RepeatingCallback since the
  // bluetooth adapter may not be present and powered the first time it's
  // invoked. The bluetooth adapter asynchronously affects the feature support
  // status and thus the entry point visibility.
  using EntryPointButtonVisibilityCallback =
      base::RepeatingCallback<void(bool)>;
  using UiState = UiDelegate::UiState;

  QuickStartController();

  QuickStartController(const QuickStartController&) = delete;
  QuickStartController& operator=(const QuickStartController&) = delete;

  ~QuickStartController() override;

  // Enable QuickStart even when the feature isn't enabled. This is only called
  // when enabling via the keyboard shortcut Ctrl+Alt+Q on the Welcome screen.
  void ForceEnableQuickStart();

  // Whether QuickStart is supported. Used for determining whether the entry
  // point buttons are shown.
  void DetermineEntryPointVisibility(
      EntryPointButtonVisibilityCallback callback);

  // Invoked by the frontend whenever the user cancels the flow or proceeds with
  // enterprise enrollment, the flow completes, or we encounter an error.
  void AbortFlow(AbortFlowReason reason);

  // Whether QuickStart is ongoing and orchestrating the flow.
  bool IsSetupOngoing() {
    return controller_state_ != ControllerState::NOT_ACTIVE;
  }

  // Whenever the QuickStartScreen is shown, it will attach itself and observe
  // the controller so that it knows when to update the UI.
  void AttachFrontend(UiDelegate* delegate);
  void DetachFrontend(UiDelegate* delegate);

  // Accessors methods to be used by the UI for retrieving data. It is an error
  // to retrieve these values when they do not exist.
  QRCode GetQrCode() { return qr_code_.value(); }
  std::string GetPin() { return pin_.value(); }
  UserInfo GetUserInfo() { return user_info_; }
  std::string GetWiFiName() { return wifi_name_.value(); }
  std::string GetFallbackUrl() { return fallback_url_.value(); }

  // If we're already connected to Wi-Fi at the start of the flow we won't
  // request Wi-Fi details from the source device. This lets us reflect that in
  // the UI.
  bool WillRequestWiFi();

  // Called by the Gaia screen during the 'CompleteAuthentication' call. This
  // notifies us that the flow succeeded and we use this signal to show the
  // 'setup complete' step of QuickStart.
  void OnFallbackUrlFlowSuccess();

  // Triggered when the user clicks on 'Turn on Bluetooth'
  void OnBluetoothPermissionGranted();

  // Exit point to be used when the flow is cancelled.
  EntryPoint GetExitPoint();

  // Exposes TargetDeviceBootstrapController::PrepareForUpdate() to the OOBE
  // UpdateScreen and ConsumerUpdateScreen.
  void PrepareForUpdate(bool is_forced);

  // Resumes current session if an update is aborted on
  // the OOBE UpdateScreen or ConsumerUpdateScreen.
  void ResumeSessionAfterCancelledUpdate();

  // Called after a user clicks "next" on final Setup Complete UI.
  void RecordFlowFinished();

  bool did_transfer_wifi() const {
    return bootstrap_controller_->did_transfer_wifi();
  }

 private:
  // Initializes the BootstrapController and starts to observe it.
  void InitTargetDeviceBootstrapController();

  // Initializes the Bluetooth and starts to observe it.
  void StartObservingBluetoothState();

  // Request advertising to start. Should only be called when bluetooth is
  // enabled.
  void StartAdvertising();

  // bluetooth_config::mojom::SystemPropertiesObserver
  void OnPropertiesUpdated(bluetooth_config::mojom::BluetoothSystemPropertiesPtr
                               properties) override;

  // Records ScreenOpened metric when UiState or OOBE screen changes.
  void MaybeRecordQuickStartScreenOpened(UiState new_ui);

  // Records ScreenClosed metric when UiState or OOBE screen changes.
  void MaybeRecordQuickStartScreenAdvanced(std::optional<UiState> closed_ui);

  // Updates the UI state and notifies the frontend.
  void UpdateUiState(UiState ui_state);

  // TargetDeviceBootstrapController::Observer
  void OnStatusChanged(
      const TargetDeviceBootstrapController::Status& status) final;

  void OnGetQuickStartFeatureSupportStatus(
      EntryPointButtonVisibilityCallback callback,
      TargetDeviceConnectionBroker::FeatureSupportStatus status);

  // OobeUI::Observer:
  void OnCurrentScreenChanged(OobeScreenId previous_screen,
                              OobeScreenId current_screen) override;
  void OnDestroyingOobeUI() override;

  // Activates the OobeUI::Observer
  void StartObservingScreenTransitions();

  // Invoked whenever OOBE transitions into the QuickStart screen.
  void HandleTransitionToQuickStartScreen();

  // Starts transferring the user account from the phone.
  void StartAccountTransfer();

  // Steps to take when all the account data has been exchanged with the phone.
  void OnOAuthTokenReceived(TargetDeviceBootstrapController::GaiaCredentials);

  // Steps to take when the connection with the phone is fully established.
  // Either transfers WiFi credentials if early in the OOBE flow, or starts
  // to transfer the user's credentials.
  void OnPhoneConnectionEstablished();

  void SavePhoneInstanceID();

  // Performs the final steps and triggers ChromeOS account creation flow.
  void FinishAccountCreation();

  bool IsBluetoothDisabled();

  // Resets all internal values. Invoked when the flow is interrupted.
  void ResetState();

  // "Main" controller for interacting with the phone. Only valid when the
  // feature flag is enabled or if the feature was enabled via the keyboard
  // shortcut.
  base::WeakPtr<TargetDeviceBootstrapController> bootstrap_controller_;

  // Source of truth of OOBE's current state via OobeUI::Observer
  std::optional<OobeScreenId> current_screen_, previous_screen_;

  // Bookkeeping where the quick start flow started and ended.
  std::optional<EntryPoint> entry_point_, exit_point_;

  // QR Code to be shown on the UI when requested.
  std::optional<QRCode> qr_code_;

  // PIN to be shown on the UI when requested.
  std::optional<std::string> pin_;

  // Fallback URL to be used on the Gaia screen when needed.
  std::optional<std::string> fallback_url_;

  // User information that is shown while 'Signing in...'
  UserInfo user_info_;

  // WiFi name to be shown on the UI.
  std::optional<std::string> wifi_name_;

  // Main state that the controller can be in.
  ControllerState controller_state_ = ControllerState::NOT_ACTIVE;

  // UI state that should be displayed by the QuickStartScreen. Only exists when
  // there is an ongoing setup.
  std::optional<UiState> ui_state_;

  // QuickStartScreen implements the UiDelegate and registers itself whenever it
  // is shown. UI updates happen over this observation path.
  base::ObserverList<UiDelegate> ui_delegates_;

  std::unique_ptr<QuickStartMetrics> metrics_;

  // Gaia credentials used for account creation.
  TargetDeviceBootstrapController::GaiaCredentials gaia_creds_;

  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      cros_bluetooth_config_remote_;

  mojo::Receiver<bluetooth_config::mojom::SystemPropertiesObserver>
      cros_system_properties_observer_receiver_{this};

  bluetooth_config::mojom::BluetoothSystemState bluetooth_system_state_ =
      bluetooth_config::mojom::BluetoothSystemState::kUnavailable;

  // Whether OOBE is transitioning to the QuickStartScreen. Used for recording
  // UI metrics.
  bool is_transitioning_to_quick_start_screen_ = false;

  bool should_resume_quick_start_after_update_ = false;

  // Used for sanity checks in order to discard unrequested data from the phone.
  // Similar checks exist on the TargetDeviceBootstrapController level.
  bool did_request_wifi_credentials_ = false;
  bool did_request_account_info_ = false;
  bool did_request_account_transfer_ = false;

  base::ScopedObservation<OobeUI, OobeUI::Observer> observation_{this};
  base::WeakPtrFactory<QuickStartController> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& stream,
                         const QuickStartController::UiState& ui_state);

std::ostream& operator<<(
    std::ostream& stream,
    const QuickStartController::AbortFlowReason& abort_flow_reason);

std::ostream& operator<<(
    std::ostream& stream,
    const QuickStartController::ControllerState& controller_state);

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICKSTART_CONTROLLER_H_
