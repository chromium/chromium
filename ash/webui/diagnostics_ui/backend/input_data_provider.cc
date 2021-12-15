// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_provider.h"

#include <fcntl.h>
#include <linux/input.h>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/system/statistics_provider.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ash {
namespace diagnostics {

namespace {

bool GetEventNodeId(base::FilePath path, int* id) {
  const std::string base_name_prefix = "event";

  std::string base_name = path.BaseName().value();
  if (!base::StartsWith(base_name, base_name_prefix))
    return false;
  base_name.erase(0, base_name_prefix.length());
  return base::StringToInt(base_name, id);
}

// Determine if this particular evdev provides touchpad or touchscreen input;
// we do not want stylus devices, which also claim to be touchscreens.
bool IsTouchInputDevice(InputDeviceInformation* device_info) {
  return (device_info->event_device_info.HasTouchpad() ||
          (device_info->event_device_info.HasTouchscreen() &&
           !device_info->event_device_info.HasStylus()));
}

}  // namespace

// All blockings calls for identifying hardware need to go here: both
// EventDeviceInfo::Initialize and ui::GetInputPathInSys can block in
// base::MakeAbsoluteFilePath.
std::unique_ptr<InputDeviceInformation> InputDeviceInfoHelper::GetDeviceInfo(
    int id,
    base::FilePath path) {
  base::ScopedFD fd(open(path.value().c_str(), O_RDWR | O_NONBLOCK));
  if (fd.get() < 0) {
    LOG(ERROR) << "Couldn't open device path " << path;
    return nullptr;
  }

  auto info = std::make_unique<InputDeviceInformation>();

  if (!info->event_device_info.Initialize(fd.get(), path)) {
    LOG(ERROR) << "Failed to get device info for " << path;
    return nullptr;
  }

  const base::FilePath sys_path = ui::GetInputPathInSys(path);

  info->path = path;
  info->evdev_id = id;
  info->connection_type = InputDataProvider::ConnectionTypeFromInputDeviceType(
      info->event_device_info.device_type());
  info->input_device = ui::InputDevice(
      id, info->event_device_info.device_type(), info->event_device_info.name(),
      info->event_device_info.phys(), sys_path,
      info->event_device_info.vendor_id(), info->event_device_info.product_id(),
      info->event_device_info.version());

  return info;
}

InputDataProvider::InputDataProvider()
    : device_manager_(ui::CreateDeviceManager()) {
  Initialize();
}

InputDataProvider::InputDataProvider(
    std::unique_ptr<ui::DeviceManager> device_manager_for_test)
    : device_manager_(std::move(device_manager_for_test)) {
  Initialize();
}

InputDataProvider::~InputDataProvider() {
  device_manager_->RemoveObserver(this);
}

// static
mojom::ConnectionType InputDataProvider::ConnectionTypeFromInputDeviceType(
    ui::InputDeviceType type) {
  switch (type) {
    case ui::InputDeviceType::INPUT_DEVICE_INTERNAL:
      return mojom::ConnectionType::kInternal;
    case ui::InputDeviceType::INPUT_DEVICE_USB:
      return mojom::ConnectionType::kUsb;
    case ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH:
      return mojom::ConnectionType::kBluetooth;
    case ui::InputDeviceType::INPUT_DEVICE_UNKNOWN:
      return mojom::ConnectionType::kUnknown;
  }
}

void InputDataProvider::Initialize() {
  device_manager_->AddObserver(this);
  device_manager_->ScanDevices(this);
}

void InputDataProvider::BindInterface(
    mojo::PendingReceiver<mojom::InputDataProvider> pending_receiver) {
  DCHECK(!ReceiverIsBound());
  receiver_.Bind(std::move(pending_receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &InputDataProvider::OnBoundInterfaceDisconnect, base::Unretained(this)));
}

bool InputDataProvider::ReceiverIsBound() {
  return receiver_.is_bound();
}

void InputDataProvider::OnBoundInterfaceDisconnect() {
  receiver_.reset();
}

void InputDataProvider::GetConnectedDevices(
    GetConnectedDevicesCallback callback) {
  std::vector<mojom::KeyboardInfoPtr> keyboard_vector;
  keyboard_vector.reserve(keyboards_.size());
  for (auto& keyboard_info : keyboards_) {
    keyboard_vector.push_back(keyboard_info.second.Clone());
  }

  std::vector<mojom::TouchDeviceInfoPtr> touch_device_vector;
  touch_device_vector.reserve(touch_devices_.size());
  for (auto& touch_device_info : touch_devices_) {
    touch_device_vector.push_back(touch_device_info.second.Clone());
  }

  base::ranges::sort(keyboard_vector, std::less<>(), &mojom::KeyboardInfo::id);
  base::ranges::sort(touch_device_vector, std::less<>(),
                     &mojom::TouchDeviceInfo::id);

  std::move(callback).Run(std::move(keyboard_vector),
                          std::move(touch_device_vector));
}

void InputDataProvider::ObserveConnectedDevices(
    mojo::PendingRemote<mojom::ConnectedDevicesObserver> observer) {
  connected_devices_observers_.Add(std::move(observer));
}

void InputDataProvider::GetKeyboardVisualLayout(
    uint32_t id,
    GetKeyboardVisualLayoutCallback callback) {
  if (!keyboards_.contains(id)) {
    LOG(ERROR) << "Couldn't find keyboard with ID " << id
               << "when retrieving visual layout.";
    return;
  }

  keyboard_helper_.GetKeyboardVisualLayout(keyboards_[id]->Clone(),
                                           std::move(callback));
}

void InputDataProvider::OnDeviceEvent(const ui::DeviceEvent& event) {
  if (event.device_type() != ui::DeviceEvent::DeviceType::INPUT ||
      event.action_type() == ui::DeviceEvent::ActionType::CHANGE) {
    return;
  }

  int id = -1;
  if (!GetEventNodeId(event.path(), &id)) {
    LOG(ERROR) << "Ignoring DeviceEvent: invalid path " << event.path();
    return;
  }

  if (event.action_type() == ui::DeviceEvent::ActionType::ADD) {
    info_helper_.AsyncCall(&InputDeviceInfoHelper::GetDeviceInfo)
        .WithArgs(id, event.path())
        .Then(base::BindOnce(&InputDataProvider::ProcessDeviceInfo,
                             weak_factory_.GetWeakPtr()));

  } else {
    DCHECK(event.action_type() == ui::DeviceEvent::ActionType::REMOVE);
    if (keyboards_.contains(id)) {
      keyboards_.erase(id);
      for (auto& observer : connected_devices_observers_) {
        observer->OnKeyboardDisconnected(id);
      }
    } else if (touch_devices_.contains(id)) {
      touch_devices_.erase(id);
      for (auto& observer : connected_devices_observers_) {
        observer->OnTouchDeviceDisconnected(id);
      }
    }
  }
}

InputDeviceInformation::InputDeviceInformation() = default;
InputDeviceInformation::~InputDeviceInformation() = default;

void InputDataProvider::ProcessDeviceInfo(
    std::unique_ptr<InputDeviceInformation> device_info) {
  if (device_info == nullptr) {
    return;
  }

  if (IsTouchInputDevice(device_info.get())) {
    AddTouchDevice(device_info.get());
  } else if (device_info->event_device_info.HasKeyboard()) {
    AddKeyboard(device_info.get());
  }
}

void InputDataProvider::AddTouchDevice(
    const InputDeviceInformation* device_info) {
  touch_devices_[device_info->evdev_id] =
      touch_helper_.ConstructTouchDevice(device_info);

  for (auto& observer : connected_devices_observers_) {
    observer->OnTouchDeviceConnected(
        touch_devices_[device_info->evdev_id]->Clone());
  }
}

void InputDataProvider::AddKeyboard(const InputDeviceInformation* device_info) {
  keyboards_[device_info->evdev_id] =
      keyboard_helper_.ConstructKeyboard(device_info);

  for (auto& observer : connected_devices_observers_) {
    observer->OnKeyboardConnected(keyboards_[device_info->evdev_id]->Clone());
  }
}

}  // namespace diagnostics
}  // namespace ash
