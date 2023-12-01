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
  // QuickStart flow entry point locations.
  enum class EntryPoint {
    WELCOME_SCREEN,
    NETWORK_SCREEN,
    GAIA_INFO_SCREEN,
    GAIA_SCREEN,
  };

  // Main state used by the controller.
  // TODO(b:283965994) - Finalize states.
  enum class ControllerState {
    NOT_ACTIVE,
    INITIALIZING,
    ADVERTISING,
    CONNECTED,
    // TODO(b:283965994) - Replace with more appropriate state.
    CONTINUING_AFTER_ENROLLMENT_CHECKS,
  };

  enum class AbortFlowReason {
    USER_CLICKED_BACK,
    USER_CLICKED_CANCEL,
    QUICK_START_FLOW_COMPLETE,
    ERROR,
  };

  // Implemented by the QuickStartScreen
  class UiDelegate : public base::CheckedObserver {
   public:
    // UI State that is used for dictating what the QuickStartScreen should
    // show.
    enum class UiState {
      LOADING,
      SHOWING_QR,
      SHOWING_PIN,
      CONNECTING_TO_WIFI,
      WIFI_CREDENTIALS_RECEIVED,
      TRANSFERRING_GAIA_CREDENTIALS,
      SHOWING_FIDO,
      // Exits the screen.
      EXIT_SCREEN,
    };

    virtual void OnUiUpdateRequested(UiState desired_state) = 0;

   protected:
    ~UiDelegate() override = default;
  };

  using EntryPointButtonVisibilityCallback = base::OnceCallback<void(bool)>;
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

  // Invoked by the frontend whenever the user cancels the flow, the flow
  // completes, or we encounter an error.
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
  QRCode::PixelData GetQrCode() { return qr_code_data_.value(); }
  std::string GetPin() { return pin_.value(); }
  std::string GetDiscoverableName() { return discoverable_name_.value(); }
  FidoAssertionInfo GetFidoAssertion() { return fido_.value(); }
  std::string GetWiFiName() { return wifi_name_.value(); }

  // Check if bluetooth is disabled which would require showing the enable
  // bluetooth dialog to turn on bluetooth before continuing quick start flow.
  bool ShouldShowBluetoothDialog();

  // Turn on bluetooth for quick start flow to continue
  void TurnOnBluetooth();

  bluetooth_config::mojom::BluetoothSystemState
  get_bluetooth_system_state_for_testing();

  // Exit point to be used when the flow is cancelled.
  EntryPoint GetExitPoint();

 private:
  // Initializes the BootstrapController and starts to observe it.
  void InitTargetDeviceBootstrapController();

  // Initializes the Bluetooth and starts to observe it.
  void StartObservingBluetoothState();

  // bluetooth_config::mojom::SystemPropertiesObserver
  void OnPropertiesUpdated(bluetooth_config::mojom::BluetoothSystemPropertiesPtr
                               properties) override;

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

  // Steps to take when the connection with the phone is fully established.
  // Either transfers WiFi credentials if early in the OOBE flow, or starts
  // to transfer the user's credentials.
  void OnPhoneConnectionEstablished();

  void SavePhoneInstanceID();

  // Resets all internal values. Invoked when the flow is interrupted.
  void ResetState();

  // "Main" controller for interacting with the phone. Only valid when the
  // feature flag is enabled or if the feature was enabled via the keyboard
  // shortcut.
  base::WeakPtr<TargetDeviceBootstrapController> bootstrap_controller_;

  // Source of truth of OOBE's current state via OobeUI::Observer
  absl::optional<OobeScreenId> current_screen_, previous_screen_;

  // Bookkeeping where the quick start flow started and ended.
  absl::optional<EntryPoint> entry_point_, exit_point_;

  // Discoverable name to be used on the UI. e.g.: Chromebook (123)
  absl::optional<std::string> discoverable_name_;

  // QR Code to be shown on the UI when requested.
  absl::optional<QRCode::PixelData> qr_code_data_;

  // PIN to be shown on the UI when requested.
  absl::optional<std::string> pin_;

  // FIDO assertion returned by the phone. Used by the UI for debugging for now.
  absl::optional<FidoAssertionInfo> fido_;

  // WiFi name to be shown on the UI.
  absl::optional<std::string> wifi_name_;

  // Main state that the controller can be in.
  ControllerState controller_state_ = ControllerState::NOT_ACTIVE;

  // UI state that should be displayed by the QuickStartScreen. Only exists when
  // there is an ongoing setup.
  absl::optional<UiState> ui_state_;

  // QuickStartScreen implements the UiDelegate and registers itself whenever it
  // is shown. UI updates happen over this observation path.
  base::ObserverList<UiDelegate> ui_delegates_;

  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      cros_bluetooth_config_remote_;

  mojo::Receiver<bluetooth_config::mojom::SystemPropertiesObserver>
      cros_system_properties_observer_receiver_{this};

  bluetooth_config::mojom::BluetoothSystemState bluetooth_system_state_ =
      bluetooth_config::mojom::BluetoothSystemState::kUnavailable;

  base::ScopedObservation<OobeUI, OobeUI::Observer> observation_{this};
  base::WeakPtrFactory<QuickStartController> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICKSTART_CONTROLLER_H_
