// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chromeos/ash/components/hid_detection/hid_detection_manager.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/input_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Representation independent class that controls screen showing warning about
// HID absence to users.
class HIDDetectionScreen : public BaseScreen,
                           public device::BluetoothAdapter::Observer,
                           public device::BluetoothDevice::PairingDelegate,
                           public device::mojom::InputDeviceManagerClient,
                           public hid_detection::HidDetectionManager::Delegate {
 public:
  using TView = HIDDetectionView;
  using InputDeviceInfoPtr = device::mojom::InputDeviceInfoPtr;
  using DeviceMap = std::map<std::string, InputDeviceInfoPtr>;

  enum class Result { NEXT, SKIPPED_FOR_TESTS };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  HIDDetectionScreen(base::WeakPtr<HIDDetectionView> view,
                     const ScreenExitCallback& exit_callback);

  HIDDetectionScreen(const HIDDetectionScreen&) = delete;
  HIDDetectionScreen& operator=(const HIDDetectionScreen&) = delete;

  ~HIDDetectionScreen() override;

  static std::string GetResultString(Result result);

  // The HID detection screen is only allowed for form factors without built-in
  // inputs: Chromebases, Chromebits, and Chromeboxes (crbug.com/965765).
  // Also different testing flags might forcefully skip the screen
  static bool CanShowScreen();

  // Checks if this screen should be displayed. `on_check_done` should be
  // invoked with the result; true if the screen should be displayed, false
  // otherwise.
  void CheckIsScreenRequired(base::OnceCallback<void(bool)> on_check_done);

  // Allows tests to override how this class binds InputDeviceManager receivers.
  using InputDeviceManagerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::InputDeviceManager>)>;
  static void OverrideInputDeviceManagerBinderForTesting(
      InputDeviceManagerBinder binder);

  // Allows tests to override what HidDetectionManager implementation is used
  // when the kOobeHidDetectionRevamp flag is enabled.
  static void OverrideHidDetectionManagerForTesting(
      std::unique_ptr<hid_detection::HidDetectionManager>
          hid_detection_manager);

  void InputDeviceAddedForTesting(InputDeviceInfoPtr info);
  const absl::optional<Result>& get_exit_result_for_testing() const {
    return exit_result_for_testing_;
  }

 private:
  friend class HIDDetectionScreenChromeboxTest;

  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // device::BluetoothDevice::PairingDelegate:
  void RequestPinCode(device::BluetoothDevice* device) override;
  void RequestPasskey(device::BluetoothDevice* device) override;
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pincode) override;
  void DisplayPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void KeysEntered(device::BluetoothDevice* device, uint32_t entered) override;
  void ConfirmPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void AuthorizePairing(device::BluetoothDevice* device) override;

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  // device::mojom::InputDeviceManagerClient:
  void InputDeviceAdded(InputDeviceInfoPtr info) override;
  void InputDeviceRemoved(const std::string& id) override;

  // hid_detection::HidDetectionManager::Delegate:
  void OnHidDetectionStatusChanged(
      hid_detection::HidDetectionManager::HidDetectionStatus status) override;

  // Called when continue button was clicked.
  void OnContinueButtonClicked();

  void CleanupOnExit();

  bool ShouldEnableContinueButton();

  void InitializeAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  void StartBTDiscoverySession();

  // Updates internal state and UI (if ready) using list of connected devices.
  void ProcessConnectedDevicesList();

  // Checks for lack of mouse or keyboard. If found starts BT devices update.
  // Initiates BTAdapter if it's not active and BT devices update required.
  void TryInitiateBTDevicesUpdate();

  void ConnectToInputDeviceManager();

  // Processes list of input devices on the check request. Calls the callback
  // that expects true if screen is required. The returned devices list is not
  // saved.
  void OnGetInputDevicesListForCheck(
      base::OnceCallback<void(bool)> on_check_done,
      std::vector<InputDeviceInfoPtr> devices);

  // Saves and processes the list of input devices returned by the request made
  // in GetInputDevicesList().
  void OnGetInputDevicesList(std::vector<InputDeviceInfoPtr> devices);

  // Called for revision of active devices. If current-placement is available
  // for mouse or keyboard device, sets one of active devices as current or
  // tries to connect some BT device if no appropriate devices are connected.
  void UpdateDevices();

  // Gets the input devices list. The devices list will be kept updated by
  // OnInputDeviceAdded() and OnInputDeviceRemoved().
  void GetInputDevicesList();

  // Tries to connect some BT devices if no type-appropriate devices are
  // connected.
  void UpdateBTDevices();

  // Called by device::BluetoothAdapter in response to a successful request
  // to initiate a discovery session.
  void OnStartDiscoverySession(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);

  // Called by device::BluetoothAdapter in response to a failure to
  // initiate a discovery session.
  void FindDevicesError();

  // Check the input devices one by one and power off the BT adapter if there
  // is no bluetooth device.
  void PowerOff();

  // Called by device::BluetoothAdapter in response to a failure to
  // power BT adapter.
  void SetPoweredError();

  // Called by device::BluetoothAdapter in response to a failure to
  // power off BT adapter.
  void SetPoweredOffError();

  // Tries to connect given BT device as pointing one.
  void TryPairingAsPointingDevice(device::BluetoothDevice* device);

  // Tries to connect given BT device as keyboard.
  void TryPairingAsKeyboardDevice(device::BluetoothDevice* device);

  // Tries to connect given BT device.
  void ConnectBTDevice(device::BluetoothDevice* device);

  // Response callback for device::BluetoothDevice::Connect().
  void OnConnect(
      const std::string& address,
      device::BluetoothDeviceType device_type,
      uint16_t device_id,
      absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

  // Sends a notification to the Web UI of the status of available Bluetooth/USB
  // pointing device.
  void SendPointingDeviceNotification();

  // Sends a notification to the Web UI of the status of available Bluetooth/USB
  // keyboard device.
  void SendKeyboardDeviceNotification();

  // Sends a notification to the Web UI of the status of available Touch Screen
  void SendTouchScreenDeviceNotification();

  // Helper methods. Sets device name or placeholder if the name is empty.
  void SetKeyboardDeviceName(const std::string& name);
  void SetPointingDeviceName(const std::string& name);

  void Exit(Result result);

  scoped_refptr<device::BluetoothAdapter> GetAdapterForTesting();
  void SetAdapterInitialPoweredForTesting(bool powered);

  base::WeakPtr<HIDDetectionView> view_;

  const ScreenExitCallback exit_callback_;
  absl::optional<Result> exit_result_for_testing_;

  // Default bluetooth adapter, used for all operations.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  mojo::Remote<device::mojom::InputDeviceManager> input_device_manager_;

  mojo::AssociatedReceiver<device::mojom::InputDeviceManagerClient> receiver_{
      this};

  // Save the connected input devices.
  DeviceMap devices_;

  // The current device discovery session. Only one active discovery session is
  // kept at a time and the instance that `discovery_session_` points to gets
  // replaced by a new one when a new discovery session is initiated.
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // Does the screen has a touch screen available?
  std::string touchscreen_id_;

  // Current pointing device, if any. Device name is kept in screen context.
  std::string pointing_device_id_;
  bool mouse_is_pairing_ = false;
  device::mojom::InputDeviceType pointing_device_type_ =
      device::mojom::InputDeviceType::TYPE_UNKNOWN;
  std::string pointing_device_name_;

  // Current keyboard device, if any. Device name is kept in screen context.
  std::string keyboard_device_id_;
  bool keyboard_is_pairing_ = false;
  device::mojom::InputDeviceType keyboard_type_ =
      device::mojom::InputDeviceType::TYPE_UNKNOWN;
  std::string keyboard_device_name_;

  // State of BT adapter before screen-initiated changes.
  std::unique_ptr<bool> adapter_initially_powered_;

  bool switch_on_adapter_when_ready_ = false;

  bool devices_enumerated_ = false;

  size_t num_pairing_attempts_ = 0;

  std::unique_ptr<hid_detection::HidDetectionManager> hid_detection_manager_;

  // Map that contains the start times of pairings for devices.
  base::flat_map<uint16_t, std::unique_ptr<base::ElapsedTimer>>
      pairing_device_id_to_timer_map_;

  base::WeakPtrFactory<HIDDetectionScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_
