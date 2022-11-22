// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/hid_detection_screen.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/hid_detection/hid_detection_manager_impl.h"
#include "chromeos/ash/components/hid_detection/hid_detection_utils.h"
#include "chromeos/constants/devicetype.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "services/device/public/mojom/input_service.mojom.h"
#include "ui/base/l10n/l10n_util.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

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

// Client name for logging in BLE scanning.
constexpr char kScanClientName[] = "HID Detection Screen";

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

HIDDetectionScreen::InputDeviceManagerBinder&
GetInputDeviceManagerBinderOverride() {
  static base::NoDestructor<HIDDetectionScreen::InputDeviceManagerBinder>
      binder;
  return *binder;
}

std::unique_ptr<hid_detection::HidDetectionManager>&
GetHidDetectionManagerOverrideForTesting() {
  static base::NoDestructor<std::unique_ptr<hid_detection::HidDetectionManager>>
      hid_detection_manager;
  return *hid_detection_manager;
}

std::string GetDeviceUiState(
    const hid_detection::HidDetectionManager::InputState& state) {
  switch (state) {
    case hid_detection::HidDetectionManager::InputState::kSearching:
      return kSearchingState;
    case hid_detection::HidDetectionManager::InputState::kConnectedViaUsb:
      return kUSBState;
    case hid_detection::HidDetectionManager::InputState::kPairingViaBluetooth:
      return kBTPairingState;
    case hid_detection::HidDetectionManager::InputState::kPairedViaBluetooth:
      return kBTPairedState;
    case hid_detection::HidDetectionManager::InputState::kConnected:
      return kConnectedState;
  }
}

bool IsInputConnected(
    const hid_detection::HidDetectionManager::InputMetadata& metadata) {
  return metadata.state !=
             hid_detection::HidDetectionManager::InputState::kSearching &&
         metadata.state != hid_detection::HidDetectionManager::InputState::
                               kPairingViaBluetooth;
}

}  // namespace

// static
std::string HIDDetectionScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::SKIPPED_FOR_TESTS:
      return BaseScreen::kNotApplicable;
  }
}

bool HIDDetectionScreen::CanShowScreen() {
  if (StartupUtils::IsHIDDetectionScreenDisabledForTests() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHIDDetectionOnOOBEForTesting)) {
    // Store the flag inside the local state so it persists restart for the
    // autoupdate tests.
    StartupUtils::DisableHIDDetectionScreenForTests();
    return false;
  }

  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebase:
    case chromeos::DeviceType::kChromebit:
    case chromeos::DeviceType::kChromebox:
      return true;
    default:
      return false;
  }
}

HIDDetectionScreen::HIDDetectionScreen(base::WeakPtr<HIDDetectionView> view,
                                       const ScreenExitCallback& exit_callback)
    : BaseScreen(HIDDetectionView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  if (features::IsOobeHidDetectionRevampEnabled()) {
    VLOG(1) << "OOBE HID detection revamped flow started";
    const auto& hid_detection_manager_override =
        GetHidDetectionManagerOverrideForTesting();
    hid_detection_manager_ =
        hid_detection_manager_override
            ? std::unique_ptr<hid_detection::HidDetectionManager>(
                  hid_detection_manager_override.get())
            : std::make_unique<hid_detection::HidDetectionManagerImpl>(
                  &content::GetDeviceService());
    return;
  }

  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &HIDDetectionScreen::InitializeAdapter, weak_ptr_factory_.GetWeakPtr()));
  ConnectToInputDeviceManager();
}

HIDDetectionScreen::~HIDDetectionScreen() {
  if (features::IsOobeHidDetectionRevampEnabled()) {
    return;
  }

  adapter_initially_powered_.reset();

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

// static
void HIDDetectionScreen::OverrideHidDetectionManagerForTesting(
    std::unique_ptr<hid_detection::HidDetectionManager> hid_detection_manager) {
  GetHidDetectionManagerOverrideForTesting() = std::move(hid_detection_manager);
}

void HIDDetectionScreen::OnContinueButtonClicked() {
  if (features::IsOobeHidDetectionRevampEnabled()) {
    hid_detection_manager_->StopHidDetection();
  } else {
    hid_detection::RecordBluetoothPairingAttempts(num_pairing_attempts_);
    CleanupOnExit();
  }
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
}

bool HIDDetectionScreen::ShouldEnableContinueButton() {
  return !pointing_device_id_.empty() || !keyboard_device_id_.empty() ||
         !touchscreen_id_.empty();
}

void HIDDetectionScreen::CheckIsScreenRequired(
    base::OnceCallback<void(bool)> on_check_done) {
  if (features::IsOobeHidDetectionRevampEnabled()) {
    hid_detection_manager_->GetIsHidDetectionRequired(std::move(on_check_done));
    return;
  }

  DCHECK(input_device_manager_);
  input_device_manager_->GetDevices(
      base::BindOnce(&HIDDetectionScreen::OnGetInputDevicesListForCheck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_check_done)));
}

bool HIDDetectionScreen::MaybeSkip(WizardContext& context) {
  if (!CanShowScreen()) {
    // TODO(https://crbug.com/1275960): Introduce Result::SKIPPED.
    Exit(Result::SKIPPED_FOR_TESTS);
    return true;
  }

  return false;
}

void HIDDetectionScreen::ShowImpl() {
  if (!is_hidden())
    return;

  if (features::IsOobeHidDetectionRevampEnabled()) {
    if (view_)
      view_->Show();

    hid_detection_manager_->StartHidDetection(/*delegate=*/this);
    return;
  }

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
  if (view_) {
    view_->Show();
  }
}

void HIDDetectionScreen::HideImpl() {
  if (is_hidden())
    return;

  if (!features::IsOobeHidDetectionRevampEnabled()) {
    if (discovery_session_.get())
      discovery_session_->Stop();

    if (adapter_)
      adapter_->RemoveObserver(this);
  }
}

void HIDDetectionScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionContinue) {
    OnContinueButtonClicked();
    return;
  }
  BaseScreen::OnUserAction(args);
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
    adapter_initially_powered_ = std::make_unique<bool>(adapter_->IsPowered());
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
  ++num_pairing_attempts_;
  pairing_device_id_to_timer_map_[device->GetDeviceID()] =
      std::make_unique<base::ElapsedTimer>();

  device->Connect(
      this, base::BindOnce(&HIDDetectionScreen::OnConnect,
                           weak_ptr_factory_.GetWeakPtr(), device->GetAddress(),
                           device_type, device->GetDeviceID()));
}

void HIDDetectionScreen::OnConnect(
    const std::string& address,
    device::BluetoothDeviceType device_type,
    uint16_t device_id,
    absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  DCHECK(base::Contains(pairing_device_id_to_timer_map_, device_id));
  hid_detection::RecordBluetoothPairingResult(
      !error_code.has_value(),
      pairing_device_id_to_timer_map_[device_id]->Elapsed());
  pairing_device_id_to_timer_map_.erase(device_id);

  if (DeviceIsPointing(device_type))
    mouse_is_pairing_ = false;
  if (DeviceIsKeyboard(device_type)) {
    keyboard_is_pairing_ = false;
    SendKeyboardDeviceNotification();
  }

  if (error_code) {
    LOG(WARNING) << "BTConnectError while connecting " << address
                 << " error code = " << error_code.value();
    if (pointing_device_id_.empty() || keyboard_device_id_.empty())
      UpdateDevices();
  }
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
  DCHECK(!info_ref.is_null());
  hid_detection::RecordHidConnected(*info_ref);
}

void HIDDetectionScreen::InputDeviceAddedForTesting(InputDeviceInfoPtr info) {
  InputDeviceAdded(std::move(info));
}

void HIDDetectionScreen::InputDeviceRemoved(const std::string& id) {
  if (is_hidden()) {
    devices_.erase(id);
    return;
  }

  // Some devices may be removed that were not registered in InputDeviceAdded or
  // OnGetInputDevicesList.
  if (base::Contains(devices_, id))
    hid_detection::RecordHidDisconnected(*devices_[id]);

  devices_.erase(id);

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

void HIDDetectionScreen::OnHidDetectionStatusChanged(
    hid_detection::HidDetectionManager::HidDetectionStatus status) {
  if (!view_)
    return;

  view_->SetTouchscreenDetectedState(status.touchscreen_detected);
  view_->SetMouseState(GetDeviceUiState(status.pointer_metadata.state));
  view_->SetPointingDeviceName(status.pointer_metadata.detected_hid_name);

  std::string keyboard_state = GetDeviceUiState(status.keyboard_metadata.state);

  // Unlike pointing devices, which can be connected through serial IO or some
  // other medium, keyboards only report USB and Bluetooth states.
  if (keyboard_state == kConnectedState)
    keyboard_state = kUSBState;
  view_->SetKeyboardState(keyboard_state);
  view_->SetKeyboardDeviceName(status.keyboard_metadata.detected_hid_name);
  view_->SetContinueButtonEnabled(status.touchscreen_detected ||
                                  IsInputConnected(status.pointer_metadata) ||
                                  IsInputConnected(status.keyboard_metadata));

  view_->SetPinDialogVisible(status.pairing_state.has_value());
  if (status.pairing_state.has_value()) {
    view_->SetKeyboardPinCode(status.pairing_state.value().code);
    view_->SetNumKeysEnteredPinCode(
        status.pairing_state.value().num_keys_entered);
    return;
  }
}

void HIDDetectionScreen::InitializeAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  CHECK(adapter_.get());
}

void HIDDetectionScreen::StartBTDiscoverySession() {
  adapter_->StartDiscoverySession(
      kScanClientName,
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
      adapter_initially_powered_ = std::make_unique<bool>(false);
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

  hid_detection::HidsMissing hids_missing = hid_detection::HidsMissing::kNone;
  if (pointing_device_id.empty()) {
    if (keyboard_device_id.empty()) {
      hids_missing = hid_detection::HidsMissing::kPointerAndKeyboard;
    } else {
      hids_missing = hid_detection::HidsMissing::kPointer;
    }
  } else if (keyboard_device_id.empty()) {
    hids_missing = hid_detection::HidsMissing::kKeyboard;
  }
  hid_detection::RecordInitialHidsMissing(hids_missing);

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
  adapter_initially_powered_ = std::make_unique<bool>(powered);
}

}  // namespace ash
