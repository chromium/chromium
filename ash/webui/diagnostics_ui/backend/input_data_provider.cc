// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_provider.h"

#include <fcntl.h>
#include <linux/input.h>
#include <algorithm>
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
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ash {
namespace diagnostics {

namespace {

bool GetEventNodeId(base::FilePath path, int* id) {
  const std::string base_name_prefix = "event";

  std::string base_name = path.BaseName().value();
  DCHECK(base::StartsWith(base_name, base_name_prefix));
  base_name.erase(0, base_name_prefix.length());
  return base::StringToInt(base_name, id);
}

mojom::ConnectionType ConnectionTypeFromInputDeviceType(
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

mojom::MechanicalLayout GetSystemMechanicalLayout() {
  chromeos::system::StatisticsProvider* stats_provider =
      chromeos::system::StatisticsProvider::GetInstance();
  std::string layout_string;
  if (!stats_provider->GetMachineStatistic(
          chromeos::system::kKeyboardMechanicalLayoutKey, &layout_string)) {
    LOG(ERROR) << "Couldn't determine mechanical layout";
    return mojom::MechanicalLayout::kUnknown;
  }
  if (layout_string == "ANSI") {
    return mojom::MechanicalLayout::kAnsi;
  } else if (layout_string == "ISO") {
    return mojom::MechanicalLayout::kIso;
  } else if (layout_string == "JIS") {
    return mojom::MechanicalLayout::kJis;
  } else {
    LOG(ERROR) << "Unknown mechanical layout " << layout_string;
    return mojom::MechanicalLayout::kUnknown;
  }
}

// Convert an XKB layout string as stored in VPD (e.g. "xkb:us::eng" or
// "xkb:cz:qwerty:cze") into the form used by XkbKeyboardLayoutEngine (e.g. "us"
// or "cz(qwerty)").
std::string ConvertXkbLayoutString(const std::string& input) {
  std::vector<base::StringPiece> parts = base::SplitStringPiece(
      input, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  const base::StringPiece& id = parts[1];
  const base::StringPiece& variant = parts[2];
  return variant.empty() ? std::string(id)
                         : base::StrCat({id, "(", variant, ")"});
}

}  // namespace

std::unique_ptr<ui::EventDeviceInfo> InputDeviceInfoHelper::GetDeviceInfo(
    base::FilePath path) {
  base::ScopedFD fd(open(path.value().c_str(), O_RDWR | O_NONBLOCK));
  if (fd.get() < 0) {
    LOG(ERROR) << "Couldn't open device path " << path;
    return nullptr;
  }

  auto device_info = std::make_unique<ui::EventDeviceInfo>();
  if (!device_info->Initialize(fd.get(), path)) {
    LOG(ERROR) << "Failed to get device info for " << path;
    return nullptr;
  }
  return device_info;
}

InputDataProvider::InputDataProvider()
    : device_manager_(ui::CreateDeviceManager()),
      xkb_layout_engine_(xkb_evdev_codes_) {
  Initialize();
}

InputDataProvider::InputDataProvider(
    std::unique_ptr<ui::DeviceManager> device_manager_for_test)
    : device_manager_(std::move(device_manager_for_test)),
      xkb_layout_engine_(xkb_evdev_codes_) {
  Initialize();
}

InputDataProvider::~InputDataProvider() {
  device_manager_->RemoveObserver(this);
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

  std::string layout_name;
  if (keyboards_[id]->connection_type == mojom::ConnectionType::kInternal) {
    chromeos::system::StatisticsProvider* stats_provider =
        chromeos::system::StatisticsProvider::GetInstance();
    if (!stats_provider->GetMachineStatistic(
            chromeos::system::kKeyboardLayoutKey, &layout_name) ||
        layout_name.empty()) {
      LOG(ERROR) << "Couldn't determine visual layout for keyboard with ID "
                 << id;
      return;
    }
    // In some regions, the keyboard layout string from the region database will
    // contain multiple comma-separated parts, where the first is the XKB layout
    // name. (For example, in region "gcc" (Gulf Cooperation Council), the
    // string is "xkb:us::eng,m17n:ar,t13n:ar".) We just want the first part.
    layout_name = base::SplitString(layout_name, ",", base::KEEP_WHITESPACE,
                                    base::SPLIT_WANT_ALL)[0];
    layout_name = ConvertXkbLayoutString(layout_name);
  } else {
    // External keyboards generally don't tell us what layout they have, so
    // assume the layout the user has currently selected.
    layout_name = input_method::InputMethodManager::Get()
                      ->GetActiveIMEState()
                      ->GetCurrentInputMethod()
                      .keyboard_layout();
  }

  xkb_layout_engine_.SetCurrentLayoutByNameWithCallback(
      layout_name,
      base::BindOnce(&InputDataProvider::ProcessXkbLayout,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void InputDataProvider::ProcessXkbLayout(
    GetKeyboardVisualLayoutCallback callback) {
  base::flat_map<uint32_t, mojom::KeyGlyphSetPtr> layout;

  // Add the glyphs for each range of evdev keycodes we're concerned with.
  // (The keycode ranges generally correspond to rows on a QWERTY keyboard, see
  // Linux's input-event-codes.h.)
  for (int evdev_code = KEY_1; evdev_code <= KEY_EQUAL; evdev_code++) {
    layout[evdev_code] = LookupGlyphSet(evdev_code);
  }
  for (int evdev_code = KEY_Q; evdev_code <= KEY_RIGHTBRACE; evdev_code++) {
    layout[evdev_code] = LookupGlyphSet(evdev_code);
  }
  for (int evdev_code = KEY_A; evdev_code <= KEY_GRAVE; evdev_code++) {
    layout[evdev_code] = LookupGlyphSet(evdev_code);
  }
  for (int evdev_code = KEY_BACKSLASH; evdev_code <= KEY_SLASH; evdev_code++) {
    layout[evdev_code] = LookupGlyphSet(evdev_code);
  }
  layout[KEY_102ND] = LookupGlyphSet(KEY_102ND);

  std::move(callback).Run(std::move(layout));
}

mojom::KeyGlyphSetPtr InputDataProvider::LookupGlyphSet(uint32_t evdev_code) {
  ui::DomCode dom_code = ui::KeycodeConverter::EvdevCodeToDomCode(evdev_code);
  ui::DomKey dom_key;
  ui::KeyboardCode key_code;
  if (!xkb_layout_engine_.Lookup(dom_code, ui::EF_NONE, &dom_key, &key_code)) {
    LOG(ERROR) << "Couldn't look up glyph for evdev code " << evdev_code;
    return nullptr;
  }
  mojom::KeyGlyphSetPtr glyph_set = mojom::KeyGlyphSet::New();
  glyph_set->main_glyph = ui::KeycodeConverter::DomKeyToKeyString(dom_key);

  if (!xkb_layout_engine_.Lookup(dom_code, ui::EF_SHIFT_DOWN, &dom_key,
                                 &key_code)) {
    LOG(WARNING) << "Couldn't look up shift glyph for evdev code "
                 << evdev_code;
  } else {
    const std::string shift_glyph =
        ui::KeycodeConverter::DomKeyToKeyString(dom_key);
    if (shift_glyph != base::ToUpperASCII(glyph_set->main_glyph)) {
      glyph_set->shift_glyph = shift_glyph;
    }
  }
  return glyph_set;
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
        .WithArgs(event.path())
        .Then(base::BindOnce(&InputDataProvider::ProcessDeviceInfo,
                             weak_factory_.GetWeakPtr(), id));
  } else {
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

void InputDataProvider::ProcessDeviceInfo(
    int id,
    std::unique_ptr<ui::EventDeviceInfo> device_info) {
  if (device_info == nullptr) {
    return;
  }

  if (device_info->HasTouchpad() ||
      (device_info->HasTouchscreen() && !device_info->HasStylus())) {
    AddTouchDevice(id, device_info.get());
  } else if (device_info->HasKeyboard()) {
    AddKeyboard(id, device_info.get());
  }
}

void InputDataProvider::AddTouchDevice(int id,
                                       const ui::EventDeviceInfo* device_info) {
  touch_devices_[id] = mojom::TouchDeviceInfo::New();
  touch_devices_[id]->id = id;
  touch_devices_[id]->connection_type =
      ConnectionTypeFromInputDeviceType(device_info->device_type());
  touch_devices_[id]->type = device_info->HasTouchpad()
                                 ? mojom::TouchDeviceType::kPointer
                                 : mojom::TouchDeviceType::kDirect;
  touch_devices_[id]->name = device_info->name();

  for (auto& observer : connected_devices_observers_) {
    observer->OnTouchDeviceConnected(touch_devices_[id]->Clone());
  }
}

void InputDataProvider::AddKeyboard(int id,
                                    const ui::EventDeviceInfo* device_info) {
  keyboards_[id] = mojom::KeyboardInfo::New();
  keyboards_[id]->id = id;
  keyboards_[id]->connection_type =
      ConnectionTypeFromInputDeviceType(device_info->device_type());
  keyboards_[id]->name = device_info->name();

  if (keyboards_[id]->connection_type == mojom::ConnectionType::kInternal) {
    if (device_info->HasKeyEvent(KEY_KBD_LAYOUT_NEXT)) {
      // Only Dell Enterprise devices have this key, marked by a globe icon.
      keyboards_[id]->physical_layout =
          mojom::PhysicalLayout::kChromeOSDellEnterprise;
    } else {
      keyboards_[id]->physical_layout = mojom::PhysicalLayout::kChromeOS;
    }
    // TODO(crbug.com/1207678): set internal keyboard as unknown on CloudReady
    // (board names chromeover64 or reven).
    keyboards_[id]->mechanical_layout = GetSystemMechanicalLayout();

    keyboards_[id]->number_pad_present =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kHasNumberPad)
            ? mojom::NumberPadPresence::kPresent
            : mojom::NumberPadPresence::kNotPresent;
  } else {
    keyboards_[id]->physical_layout = mojom::PhysicalLayout::kUnknown;
    keyboards_[id]->number_pad_present = mojom::NumberPadPresence::kUnknown;
    // TODO(crbug.com/1207678): support WWCB keyboards, Chromebase keyboards,
    // and Dell KM713 Chrome keyboard.
  }

  keyboards_[id]->has_assistant_key = device_info->HasKeyEvent(KEY_ASSISTANT);

  for (auto& observer : connected_devices_observers_) {
    observer->OnKeyboardConnected(keyboards_[id]->Clone());
  }
}

}  // namespace diagnostics
}  // namespace ash
