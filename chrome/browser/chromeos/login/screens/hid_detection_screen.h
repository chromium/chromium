// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/input_service.mojom.h"

namespace chromeos {

class HIDDetectionView;

// Representation independent class that controls screen showing warning about
// HID absence to users.
class HIDDetectionScreen : public BaseScreen,
                           public device::BluetoothAdapter::Observer,
                           public device::BluetoothDevice::PairingDelegate,
                           public device::mojom::InputDeviceManagerClient {
 public:
  using InputDeviceInfoPtr = device::mojom::InputDeviceInfoPtr;
  using DeviceMap = std::map<std::string, InputDeviceInfoPtr>;

  HIDDetectionScreen(HIDDetectionView* view,
                     const base::RepeatingClosure& exit_callback);
  ~HIDDetectionScreen() override;

  // Called when continue button was clicked.
  void OnContinueButtonClicked();

  // This method is called when the view is being destroyed.
  void OnViewDestroyed(HIDDetectionView* view);

  // Checks if this screen should be displayed. |on_check_done| should be
  // invoked with the result; true if the screen should be displayed, false
  // otherwise.
  void CheckIsScreenRequired(const base::Callback<void(bool)>& on_check_done);

 private:
  friend class HIDDetectionScreenTest;

  // BaseScreen implementation:
  void Show() override;
  void Hide() override;

  // device::BluetoothDevice::PairingDelegate implementation:
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

  // device::BluetoothAdapter::Observer implementation.
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  // device::mojom::InputDeviceManagerClient implementation.
  void InputDeviceAdded(InputDeviceInfoPtr info) override;
  void InputDeviceRemoved(const std::string& id) override;

  // Types of dialog leaving scenarios for UMA metric.
  enum ContinueScenarioType {
    // Only pointing device detected, user pressed 'Continue'.
    POINTING_DEVICE_ONLY_DETECTED,

    // Only keyboard detected, user pressed 'Continue'.
    KEYBOARD_DEVICE_ONLY_DETECTED,

    // All devices detected.
    All_DEVICES_DETECTED,

    // Must be last enum element.
    CONTINUE_SCENARIO_TYPE_SIZE
  };

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
      const base::Callback<void(bool)>& on_check_done,
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

  // Called by device::BluetoothDevice on a successful pairing and connection
  // to a device.
  void BTConnected(device::BluetoothDeviceType device_type);

  // Called by device::BluetoothDevice in response to a failure to
  // connect to the device with bluetooth address |address| due to an error
  // encoded in |error_code|.
  void BTConnectError(const std::string& address,
                      device::BluetoothDeviceType device_type,
                      device::BluetoothDevice::ConnectErrorCode error_code);

  // Sends a notification to the Web UI of the status of available Bluetooth/USB
  // pointing device.
  void SendPointingDeviceNotification();

  // Sends a notification to the Web UI of the status of available Bluetooth/USB
  // keyboard device.
  void SendKeyboardDeviceNotification();

  // Helper method. Sets device name or placeholder if the name is empty.
  void SetKeyboardDeviceName(const std::string& name);

  scoped_refptr<device::BluetoothAdapter> GetAdapterForTesting();
  void SetAdapterInitialPoweredForTesting(bool powered);

  HIDDetectionView* view_;
  base::RepeatingClosure exit_callback_;

  // Default bluetooth adapter, used for all operations.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  mojo::Remote<device::mojom::InputDeviceManager> input_device_manager_;

  mojo::AssociatedReceiver<device::mojom::InputDeviceManagerClient> receiver_{
      this};

  // Save the connected input devices.
  DeviceMap devices_;

  // The current device discovery session. Only one active discovery session is
  // kept at a time and the instance that |discovery_session_| points to gets
  // replaced by a new one when a new discovery session is initiated.
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // Current pointing device, if any. Device name is kept in screen context.
  std::string pointing_device_id_;
  bool mouse_is_pairing_ = false;
  device::mojom::InputDeviceType pointing_device_connect_type_ =
      device::mojom::InputDeviceType::TYPE_UNKNOWN;

  // Current keyboard device, if any. Device name is kept in screen context.
  std::string keyboard_device_id_;
  bool keyboard_is_pairing_ = false;
  device::mojom::InputDeviceType keyboard_device_connect_type_ =
      device::mojom::InputDeviceType::TYPE_UNKNOWN;
  std::string keyboard_device_name_;

  // State of BT adapter before screen-initiated changes.
  std::unique_ptr<bool> adapter_initially_powered_;

  bool switch_on_adapter_when_ready_ = false;

  bool devices_enumerated_ = false;

  bool showing_ = false;

  base::WeakPtrFactory<HIDDetectionScreen> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HIDDetectionScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_
