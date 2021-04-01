// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/hid_detection_screen.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Possible ui-states for device-blocks.
constexpr char kSearchingState[] = "searching";
constexpr char kUSBState[] = "usb";
constexpr char kConnectedState[] = "connected";
constexpr char kBTPairedState[] = "paired";
constexpr char kBTPairingState[] = "pairing";
constexpr char kUserActionContinue[] = "HIDDetectionOnContinue";

// Standard length of pincode for pairing BT keyboards.
constexpr int kPincodeLength = 6;

bool DeviceIsPointing(device::BluetoothDeviceType device_type) {
  return device_type == device::BluetoothDeviceType::MOUSE ||
         device_type == device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO ||
         device_type == device::BluetoothDeviceType::TABLET;
}

bool DeviceIsPointing(const device::mojom::InputDeviceInfoPtr& info) {
  return info->is_mouse || info->is_touchpad;
}

bool DeviceIsTouchScreen(const device::mojom::InputDeviceInfoPtr& info) {
  return info->is_touchscreen || info->is_tablet;
}

bool DeviceIsKeyboard(device::BluetoothDeviceType device_type) {
  return device_type == device::BluetoothDeviceType::KEYBOARD ||
         device_type == device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO;
}

chromeos::HIDDetectionScreen::InputDeviceManagerBinder&
GetInputDeviceManagerBinderOverride() {
  static base::NoDestructor<
      chromeos::HIDDetectionScreen::InputDeviceManagerBinder>
      binder;
  return *binder;
}

}  // namespace

namespace chromeos {

// static
std::string HIDDetectionScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::SKIP:
    case Result::SKIPPED_FOR_TESTS:
      return BaseScreen::kNotApplicable;
  }
}

HIDDetectionScreen::HIDDetectionScreen(HIDDetectionView* view,
                                       const ScreenExitCallback& exit_callback)
    : BaseScreen(HIDDetectionView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);

  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &HIDDetectionScreen::InitializeAdapter, weak_ptr_factory_.GetWeakPtr()));
  ConnectToInputDeviceManager();
}

HIDDetectionScreen::~HIDDetectionScreen() {
  adapter_initially_powered_.reset();
  if (view_)
    view_->Unbind();
  if (discovery_session_.get())
    discovery_session_->Stop();
  if (adapter_.get())
    adapter_->RemoveObserver(this);
}

// static
void HIDDetectionScreen::OverrideInputDeviceManagerBinderForTesting(
    InputDeviceManagerBinder binder) {
  GetInputDeviceManagerBinderOverride() = std::move(binder);
}

void HIDDetectionScreen::OnContinueButtonClicked() {
  ContinueScenarioType scenario_type;
  if (!pointing_device_id_.empty() && !keyboard_device_id_.empty())
    scenario_type = All_DEVICES_DETECTED;
  else if (pointing_device_id_.empty())
    scenario_type = KEYBOARD_DEVICE_ONLY_DETECTED;
  else
    scenario_type = POINTING_DEVICE_ONLY_DETECTED;

  UMA_HISTOGRAM_ENUMERATION("HIDDetection.OOBEDevicesDetectedOnContinuePressed",
                            scenario_type, CONTINUE_SCENARIO_TYPE_SIZE);

  CleanupOnExit();
  Exit(Result::NEXT);
}

void HIDDetectionScreen::CleanupOnExit() {
  // Switch off BT adapter if it was off before the screen and no BT device
  // connected.
  const bool adapter_is_powered =
      adapter_.get() && adapter_->IsPresent() && adapter_->IsPowered();
  const bool need_switching_off =
      adapter_initially_powered_ && !(*adapter_initially_powered_);
  if (adapter_is_powered && need_switching_off)
    PowerOff();

  demo_mode_detector_.reset();
}

void HIDDetectionScreen::OnViewDestroyed(HIDDetectionView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool HIDDetectionScreen::ShouldEnableContinueButton() {
  return !pointing_device_id_.empty() || !keyboard_device_id_.empty() ||
         !touchscreen_id_.empty();
}

void HIDDetectionScreen::CheckIsScreenRequired(
    base::OnceCallback<void(bool)> on_check_done) {
  DCHECK(input_device_manager_);
  input_device_manager_->GetDevices(
      base::BindOnce(&HIDDetectionScreen::OnGetInputDevicesListForCheck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_check_done)));
}

bool HIDDetectionScreen::MaybeSkip(WizardContext* context) {
  const auto* skip_screen_key = context->configuration.FindKeyOfType(
      configuration::kSkipHIDDetection, base::Value::Type::BOOLEAN);
  const bool skip_screen = skip_screen_key && skip_screen_key->GetBool();

  if (skip_screen) {
    Exit(Result::SKIP);
    return true;
  }

  if (chromeos::StartupUtils::IsHIDDetectionScreenDisabledForTests() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableHIDDetectionOnOOBEForTesting)) {
    Exit(Result::SKIPPED_FOR_TESTS);
    return true;
  }

  return false;
}

void HIDDetectionScreen::ShowImpl() {
  if (!is_hidden())
    return;

  if (adapter_)
    adapter_->AddObserver(this);

  if (view_)
    view_->SetPinDialogVisible(false);

  SendTouchScreenDeviceNotification();
  SendPointingDeviceNotification();
  SendKeyboardDeviceNotification();

  if (!devices_enumerated_)
    GetInputDevicesList();
  else
    UpdateDevices();
  demo_mode_detector_ = std::make_unique<DemoModeDetector>(
      base::DefaultTickClock::GetInstance(), this);
  if (view_) {
    view_->Show();
  }
}

void HIDDetectionScreen::HideImpl() {
  if (is_hidden())
    return;
  demo_mode_detector_.reset();

  if (discovery_session_.get())
    discovery_session_->Stop();

  if (adapter_)
    adapter_->RemoveObserver(this);

  if (view_)
    view_->Hide();
}

void HIDDetectionScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionContinue) {
    OnContinueButtonClicked();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void HIDDetectionScreen::RequestPinCode(device::BluetoothDevice* device) {
  VLOG(1) << "RequestPinCode id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  device->CancelPairing();
}

void HIDDetectionScreen::RequestPasskey(device::BluetoothDevice* device) {
  VLOG(1) << "RequestPassKey id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  device->CancelPairing();
}

void HIDDetectionScreen::DisplayPinCode(device::BluetoothDevice* device,
                                        const std::string& pincode) {
  VLOG(1) << "DisplayPinCode id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  if (view_) {
    view_->SetKeyboardPinCode(pincode);
    view_->SetPinDialogVisible(true);
  }

  SetKeyboardDeviceName(base::UTF16ToUTF8(device->GetNameForDisplay()));
  SendKeyboardDeviceNotification();
}

void HIDDetectionScreen::DisplayPasskey(device::BluetoothDevice* device,
                                        uint32_t passkey) {
  VLOG(1) << "DisplayPassKey id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  std::string pincode = base::NumberToString(passkey);
  pincode = std::string(kPincodeLength - pincode.length(), '0').append(pincode);
  // No differences in UI for passkey and pincode authentication calls.
  DisplayPinCode(device, pincode);
}

void HIDDetectionScreen::KeysEntered(device::BluetoothDevice* device,
                                     uint32_t entered) {
  VLOG(1) << "Number of keys entered " << entered;
  if (view_) {
    view_->SetPinDialogVisible(true);
    view_->SetNumKeysEnteredPinCode(entered);
  }
  SendKeyboardDeviceNotification();
}

void HIDDetectionScreen::ConfirmPasskey(device::BluetoothDevice* device,
                                        uint32_t passkey) {
  VLOG(1) << "Confirm Passkey";
  device->CancelPairing();
}

void HIDDetectionScreen::AuthorizePairing(device::BluetoothDevice* device) {
  // There is never any circumstance where this will be called, since the
  // HID detection screen will only be used for outgoing pairing
  // requests, but play it safe.
  VLOG(1) << "Authorize pairing";
  device->ConfirmPairing();
}

void HIDDetectionScreen::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  if (present && switch_on_adapter_when_ready_) {
    VLOG(1) << "Switching on BT adapter on HID OOBE screen.";
    adapter_initially_powered_.reset(new bool(adapter_->IsPowered()));
    adapter_->SetPowered(
        true,
        base::BindOnce(&HIDDetectionScreen::StartBTDiscoverySession,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&HIDDetectionScreen::SetPoweredError,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void HIDDetectionScreen::TryPairingAsPointingDevice(
    device::BluetoothDevice* device) {
  if (pointing_device_id_.empty() &&
      DeviceIsPointing(device->GetDeviceType()) && device->IsPairable() &&
      !(device->IsConnected() && device->IsPaired()) && !mouse_is_pairing_) {
    ConnectBTDevice(device);
  }
}

void HIDDetectionScreen::TryPairingAsKeyboardDevice(
    device::BluetoothDevice* device) {
  if (keyboard_device_id_.empty() &&
      DeviceIsKeyboard(device->GetDeviceType()) && device->IsPairable() &&
      !(device->IsConnected() && device->IsPaired()) && !keyboard_is_pairing_) {
    ConnectBTDevice(device);
  }
}

void HIDDetectionScreen::ConnectBTDevice(device::BluetoothDevice* device) {
  bool device_busy =
      (device->IsConnected() && device->IsPaired()) || device->IsConnecting();
  if (!device->IsPairable() || device_busy)
    return;
  device::BluetoothDeviceType device_type = device->GetDeviceType();

  if (device_type == device::BluetoothDeviceType::MOUSE ||
      device_type == device::BluetoothDeviceType::TABLET) {
    if (mouse_is_pairing_)
      return;
    mouse_is_pairing_ = true;
  } else if (device_type == device::BluetoothDeviceType::KEYBOARD) {
    if (keyboard_is_pairing_)
      return;
    keyboard_is_pairing_ = true;
  } else if (device_type == device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO) {
    if (mouse_is_pairing_ && keyboard_is_pairing_)
      return;
    mouse_is_pairing_ = true;
    keyboard_is_pairing_ = true;
  }
  device->Connect(this,
                  base::BindOnce(&HIDDetectionScreen::BTConnected,
                                 weak_ptr_factory_.GetWeakPtr(), device_type),
                  base::BindOnce(&HIDDetectionScreen::BTConnectError,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 device->GetAddress(), device_type));
}

void HIDDetectionScreen::BTConnected(device::BluetoothDeviceType device_type) {
  if (DeviceIsPointing(device_type))
    mouse_is_pairing_ = false;
  if (DeviceIsKeyboard(device_type)) {
    keyboard_is_pairing_ = false;
    SendKeyboardDeviceNotification();
  }
}

void HIDDetectionScreen::BTConnectError(
    const std::string& address,
    device::BluetoothDeviceType device_type,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  LOG(WARNING) << "BTConnectError while connecting " << address
               << " error code = " << error_code;
  if (DeviceIsPointing(device_type))
    mouse_is_pairing_ = false;
  if (DeviceIsKeyboard(device_type)) {
    keyboard_is_pairing_ = false;
    SendKeyboardDeviceNotification();
  }

  if (pointing_device_id_.empty() || keyboard_device_id_.empty())
    UpdateDevices();
}

void HIDDetectionScreen::SendPointingDeviceNotification() {
  std::string state;
  if (pointing_device_id_.empty()) {
    state = kSearchingState;
  } else if (pointing_device_type_ ==
             device::mojom::InputDeviceType::TYPE_BLUETOOTH) {
    state = kBTPairedState;
  } else if (pointing_device_type_ ==
             device::mojom::InputDeviceType::TYPE_USB) {
    state = kUSBState;
  } else {
    state = kConnectedState;
  }

  if (view_) {
    view_->SetMouseState(state);
    view_->SetPointingDeviceName(pointing_device_name_);
    view_->SetContinueButtonEnabled(ShouldEnableContinueButton());
  }
}

void HIDDetectionScreen::SendKeyboardDeviceNotification() {
  std::string state;
  if (keyboard_device_id_.empty()) {  // Device not enumerated yet.
    state = keyboard_is_pairing_ ? kBTPairingState : kSearchingState;
  } else {
    state = (keyboard_type_ == device::mojom::InputDeviceType::TYPE_BLUETOOTH)
                ? kBTPairedState
                : kUSBState;
  }

  if (view_) {
    // If the keyboard is not pairing, the PIN dialog should not be open
    // The dialog is opened directly through the bluetooth events
    // DisplayPinCode, DisplayPasskey, and KeysEntered. When the device connects
    // or fails, the pairing flag is set to false and we close it here.
    if (!keyboard_is_pairing_) {
      view_->SetPinDialogVisible(false);
    }
    view_->SetKeyboardState(state);
    view_->SetKeyboardDeviceName(keyboard_device_name_);
    view_->SetContinueButtonEnabled(ShouldEnableContinueButton());
  }
}

void HIDDetectionScreen::SendTouchScreenDeviceNotification() {
  if (!view_)
    return;

  view_->SetTouchscreenDetectedState(!touchscreen_id_.empty());
  view_->SetContinueButtonEnabled(ShouldEnableContinueButton());
}

void HIDDetectionScreen::SetKeyboardDeviceName(const std::string& name) {
  keyboard_device_name_ =
      keyboard_device_id_.empty() || !name.empty()
          ? name
          : l10n_util::GetStringUTF8(IDS_HID_DETECTION_DEFAULT_KEYBOARD_NAME);
}

void HIDDetectionScreen::SetPointingDeviceName(const std::string& name) {
  pointing_device_name_ = name;
}

void HIDDetectionScreen::Exit(Result result) {
  exit_result_for_testing_ = result;
  exit_callback_.Run(result);
}

void HIDDetectionScreen::DeviceAdded(device::BluetoothAdapter* adapter,
                                     device::BluetoothDevice* device) {
  VLOG(1) << "BT input device added id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  TryPairingAsPointingDevice(device);
  TryPairingAsKeyboardDevice(device);
}

void HIDDetectionScreen::DeviceChanged(device::BluetoothAdapter* adapter,
                                       device::BluetoothDevice* device) {
  VLOG(1) << "BT device changed id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  TryPairingAsPointingDevice(device);
  TryPairingAsKeyboardDevice(device);
}

void HIDDetectionScreen::DeviceRemoved(device::BluetoothAdapter* adapter,
                                       device::BluetoothDevice* device) {
  VLOG(1) << "BT device removed id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
}

void HIDDetectionScreen::InputDeviceAdded(InputDeviceInfoPtr info) {
  VLOG(1) << "Input device added id = " << info->id << " name = " << info->name;
  const InputDeviceInfoPtr& info_ref = devices_[info->id] = std::move(info);
  if (is_hidden())
    return;

  if (touchscreen_id_.empty() && DeviceIsTouchScreen(info_ref)) {
    touchscreen_id_ = info_ref->id;
    SendTouchScreenDeviceNotification();
  }
  if (pointing_device_id_.empty() && DeviceIsPointing(info_ref)) {
    pointing_device_id_ = info_ref->id;
    pointing_device_type_ = info_ref->type;
    SetPointingDeviceName(info_ref->name);
    SendPointingDeviceNotification();
  }
  if (keyboard_device_id_.empty() && info_ref->is_keyboard) {
    keyboard_device_id_ = info_ref->id;
    keyboard_type_ = info_ref->type;
    SetKeyboardDeviceName(info_ref->name);
    SendKeyboardDeviceNotification();
  }
}

void HIDDetectionScreen::InputDeviceAddedForTesting(InputDeviceInfoPtr info) {
  InputDeviceAdded(std::move(info));
}

void HIDDetectionScreen::InputDeviceRemoved(const std::string& id) {
  devices_.erase(id);
  if (is_hidden())
    return;

  if (id == touchscreen_id_) {
    touchscreen_id_.clear();
    SendTouchScreenDeviceNotification();
    UpdateDevices();
  }
  if (id == keyboard_device_id_) {
    keyboard_device_id_.clear();
    keyboard_type_ = device::mojom::InputDeviceType::TYPE_UNKNOWN;
    SendKeyboardDeviceNotification();
    UpdateDevices();
  }
  if (id == pointing_device_id_) {
    pointing_device_id_.clear();
    pointing_device_type_ = device::mojom::InputDeviceType::TYPE_UNKNOWN;
    SendPointingDeviceNotification();
    UpdateDevices();
  }
}

void HIDDetectionScreen::InitializeAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  CHECK(adapter_.get());
}

void HIDDetectionScreen::StartBTDiscoverySession() {
  adapter_->StartDiscoverySession(
      base::BindOnce(&HIDDetectionScreen::OnStartDiscoverySession,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HIDDetectionScreen::FindDevicesError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HIDDetectionScreen::ProcessConnectedDevicesList() {
  for (const auto& map_entry : devices_) {
    if (touchscreen_id_.empty() && DeviceIsTouchScreen(map_entry.second)) {
      touchscreen_id_ = map_entry.second->id;
      SendTouchScreenDeviceNotification();
    }
    if (pointing_device_id_.empty() && DeviceIsPointing(map_entry.second)) {
      pointing_device_id_ = map_entry.second->id;
      if (view_)
        view_->SetPointingDeviceName(map_entry.second->name);
      pointing_device_type_ = map_entry.second->type;
      SendPointingDeviceNotification();
    }
    if (keyboard_device_id_.empty() && (map_entry.second->is_keyboard)) {
      keyboard_device_id_ = map_entry.second->id;
      SetKeyboardDeviceName(map_entry.second->name);
      keyboard_type_ = map_entry.second->type;
      SendKeyboardDeviceNotification();
    }
  }
}

void HIDDetectionScreen::TryInitiateBTDevicesUpdate() {
  if ((pointing_device_id_.empty() || keyboard_device_id_.empty()) &&
      adapter_.get()) {
    if (!adapter_->IsPresent()) {
      // Switch on BT adapter later when it's available.
      switch_on_adapter_when_ready_ = true;
    } else if (!adapter_->IsPowered()) {
      VLOG(1) << "Switching on BT adapter on HID OOBE screen.";
      adapter_initially_powered_.reset(new bool(false));
      adapter_->SetPowered(
          true,
          base::BindOnce(&HIDDetectionScreen::StartBTDiscoverySession,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&HIDDetectionScreen::SetPoweredError,
                         weak_ptr_factory_.GetWeakPtr()));
    } else if (!discovery_session_ || !discovery_session_->IsActive()) {
      StartBTDiscoverySession();
    } else {
      UpdateBTDevices();
    }
  }
}

void HIDDetectionScreen::ConnectToInputDeviceManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto receiver = input_device_manager_.BindNewPipeAndPassReceiver();
  const auto& binder = GetInputDeviceManagerBinderOverride();
  if (binder)
    binder.Run(std::move(receiver));
  else
    content::GetDeviceService().BindInputDeviceManager(std::move(receiver));
}

void HIDDetectionScreen::OnGetInputDevicesListForCheck(
    base::OnceCallback<void(bool)> on_check_done,
    std::vector<InputDeviceInfoPtr> devices) {
  std::string pointing_device_id;
  std::string keyboard_device_id;
  for (const auto& device : devices) {
    if (pointing_device_id.empty() && DeviceIsPointing(device))
      pointing_device_id = device->id;

    if (keyboard_device_id.empty() && device->is_keyboard)
      keyboard_device_id = device->id;

    if (!pointing_device_id.empty() && !keyboard_device_id.empty())
      break;
  }
  // Screen is not required if both devices are present.
  const bool all_devices_autodetected =
      !pointing_device_id.empty() && !keyboard_device_id.empty();
  UMA_HISTOGRAM_BOOLEAN("HIDDetection.OOBEDialogShown",
                        !all_devices_autodetected);

  std::move(on_check_done).Run(!all_devices_autodetected);
}

void HIDDetectionScreen::OnGetInputDevicesList(
    std::vector<InputDeviceInfoPtr> devices) {
  devices_enumerated_ = true;
  for (auto& device : devices) {
    devices_[device->id] = std::move(device);
  }
  UpdateDevices();
}

void HIDDetectionScreen::GetInputDevicesList() {
  DCHECK(input_device_manager_);
  input_device_manager_->GetDevicesAndSetClient(
      receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&HIDDetectionScreen::OnGetInputDevicesList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HIDDetectionScreen::UpdateDevices() {
  ProcessConnectedDevicesList();
  TryInitiateBTDevicesUpdate();
}

void HIDDetectionScreen::UpdateBTDevices() {
  if (!adapter_.get() || !adapter_->IsPresent() || !adapter_->IsPowered())
    return;

  // If no connected devices found as pointing device and keyboard, we try to
  // connect some type-suitable active bluetooth device.
  std::vector<device::BluetoothDevice*> bt_devices = adapter_->GetDevices();
  for (std::vector<device::BluetoothDevice*>::const_iterator it =
           bt_devices.begin();
       it != bt_devices.end() &&
       (keyboard_device_id_.empty() || pointing_device_id_.empty());
       ++it) {
    TryPairingAsPointingDevice(*it);
    TryPairingAsKeyboardDevice(*it);
  }
}

void HIDDetectionScreen::OnStartDiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  VLOG(1) << "BT Discovery session started";
  discovery_session_ = std::move(discovery_session);
  UpdateDevices();
}

void HIDDetectionScreen::PowerOff() {
  bool use_bluetooth = false;
  for (const auto& map_entry : devices_) {
    if (map_entry.second->type ==
        device::mojom::InputDeviceType::TYPE_BLUETOOTH) {
      use_bluetooth = true;
      break;
    }
  }
  if (!use_bluetooth) {
    VLOG(1) << "Switching off BT adapter after HID OOBE screen as unused.";
    adapter_->SetPowered(false, base::DoNothing(),
                         base::BindOnce(&HIDDetectionScreen::SetPoweredOffError,
                                        weak_ptr_factory_.GetWeakPtr()));
  }
}

void HIDDetectionScreen::SetPoweredError() {
  LOG(ERROR) << "Failed to power BT adapter";
}

void HIDDetectionScreen::SetPoweredOffError() {
  LOG(ERROR) << "Failed to power off BT adapter";
}

void HIDDetectionScreen::FindDevicesError() {
  VLOG(1) << "Failed to start Bluetooth discovery.";
}

scoped_refptr<device::BluetoothAdapter>
HIDDetectionScreen::GetAdapterForTesting() {
  return adapter_;
}

void HIDDetectionScreen::SetAdapterInitialPoweredForTesting(bool powered) {
  adapter_initially_powered_.reset(new bool(powered));
}

}  // namespace chromeos
