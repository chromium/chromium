// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/modifier_key_combo_recorder.h"

#include <optional>
#include <tuple>

#include "ash/events/event_rewriter_controller_impl.h"
#include "ash/events/prerewritten_event_forwarder.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/shell.h"
#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

using DeviceType = ui::KeyboardCapability::DeviceType;
using Modifier = ModifierKeyComboRecorder::Modifier;
using ModifierFlag = ModifierKeyComboRecorder::ModifierFlag;
using ModifierLocation = ModifierKeyComboRecorder::ModifierLocation;

constexpr auto kShiftKeys = base::MakeFixedFlatSet<ui::KeyboardCode>({
    ui::VKEY_SHIFT,
    ui::VKEY_LSHIFT,
    ui::VKEY_RSHIFT,
});

constexpr auto kAltKeys = base::MakeFixedFlatSet<ui::KeyboardCode>({
    ui::VKEY_MENU,
    ui::VKEY_LMENU,
    ui::VKEY_RMENU,
});

constexpr auto kAltGrKeys = base::MakeFixedFlatSet<ui::KeyboardCode>({
    ui::VKEY_ALTGR,
});

constexpr auto kMetaKeys = base::MakeFixedFlatSet<ui::KeyboardCode>({
    ui::VKEY_LWIN,
    ui::VKEY_RWIN,
});

constexpr auto kControlKeys = base::MakeFixedFlatSet<ui::KeyboardCode>({
    ui::VKEY_CONTROL,
    ui::VKEY_LCONTROL,
    ui::VKEY_RCONTROL,
});

constexpr auto kFunctionKeys = base::MakeFixedFlatSet<ui::KeyboardCode>({
    ui::VKEY_FUNCTION,
});

// The ModifierKeyCombo metric is formed as follows:
// The bottom 16 bits are composed of the `AcceleratorKeyInputType` that matches
// what key was pressed. The top 16 bits are composed of bitfields from the
// `ModifierFlag` enum for each modifier (including differentiating between Left
// and Right modifiers).
uint32_t CalculateHash(AcceleratorKeyInputType key_type,
                       uint32_t modifier_flags) {
  static_assert(sizeof(AcceleratorKeyInputType) <= sizeof(uint16_t));
  static_assert(static_cast<uint32_t>(ModifierFlag::kMaxValue) <
                (sizeof(uint16_t) * 8));
  return (modifier_flags << (sizeof(uint16_t) * 8)) +
         static_cast<uint16_t>(key_type);
}

ModifierFlag GetModifierFlagFromModifierAndLocation(Modifier modifier,
                                                    ModifierLocation location) {
  switch (modifier) {
    case Modifier::kMeta:
      return location == ModifierLocation::kLeft ? ModifierFlag::kMetaLeft
                                                 : ModifierFlag::kMetaRight;
    case Modifier::kControl:
      return location == ModifierLocation::kLeft ? ModifierFlag::kControlLeft
                                                 : ModifierFlag::kControlRight;
    case Modifier::kAlt: {
      return location == ModifierLocation::kLeft ? ModifierFlag::kAltLeft
                                                 : ModifierFlag::kAltRight;
    }
    case Modifier::kShift:
      return location == ModifierLocation::kLeft ? ModifierFlag::kShiftLeft
                                                 : ModifierFlag::kShiftRight;
  }
}

std::optional<std::pair<Modifier, ModifierLocation>> GetModifierFromKeyEvent(
    const ui::KeyEvent& key_event) {
  switch (key_event.code()) {
    case ui::DomCode::META_LEFT:
      return std::make_pair(Modifier::kMeta, ModifierLocation::kLeft);
    case ui::DomCode::META_RIGHT:
      return std::make_pair(Modifier::kMeta, ModifierLocation::kRight);
    case ui::DomCode::CONTROL_LEFT:
      return std::make_pair(Modifier::kControl, ModifierLocation::kLeft);
    case ui::DomCode::CONTROL_RIGHT:
      return std::make_pair(Modifier::kControl, ModifierLocation::kRight);
    case ui::DomCode::ALT_LEFT:
      return std::make_pair(Modifier::kAlt, ModifierLocation::kLeft);
    case ui::DomCode::ALT_RIGHT:
      return std::make_pair(Modifier::kAlt, ModifierLocation::kRight);
    case ui::DomCode::SHIFT_LEFT:
      return std::make_pair(Modifier::kShift, ModifierLocation::kLeft);
    case ui::DomCode::SHIFT_RIGHT:
      return std::make_pair(Modifier::kShift, ModifierLocation::kRight);
    default:
      return std::nullopt;
  }
}

void RecordMetricForDeviceType(ui::KeyboardCapability::DeviceType device_type,
                               uint32_t hash) {
  switch (device_type) {
    case ui::KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard:
      UMA_HISTOGRAM_SPARSE("ChromeOS.Inputs.ModifierKeyCombo.CrOSExternal",
                           hash);
      break;
    case ui::KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard:
    case ui::KeyboardCapability::DeviceType::
        kDeviceExternalNullTopRowChromeOsKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceExternalUnknown:
    case ui::KeyboardCapability::DeviceType::kDeviceHotrodRemote:
    case ui::KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard:
      UMA_HISTOGRAM_SPARSE("ChromeOS.Inputs.ModifierKeyCombo.External", hash);
      break;
    case ui::KeyboardCapability::DeviceType::kDeviceUnknown:
    case ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceInternalRevenKeyboard:
      UMA_HISTOGRAM_SPARSE("ChromeOS.Inputs.ModifierKeyCombo.Internal", hash);
      break;
  }
}

}  // namespace

ModifierKeyComboRecorder::ModifierKeyComboRecorder() = default;
ModifierKeyComboRecorder::~ModifierKeyComboRecorder() {
  CHECK(Shell::Get());
  auto* event_forwarder =
      Shell::Get()->event_rewriter_controller()->prerewritten_event_forwarder();
  if (initialized_ && event_forwarder) {
    event_forwarder->RemoveObserver(this);
  }
}

void ModifierKeyComboRecorder::Initialize() {
  CHECK(Shell::Get());
  auto* event_forwarder =
      Shell::Get()->event_rewriter_controller()->prerewritten_event_forwarder();
  if (!event_forwarder) {
    LOG(ERROR) << "Attempted to initialiaze ModifierKeyComboRecorder before "
               << "PrerewrittenEventForwarder was initialized.";
    return;
  }

  initialized_ = true;
  event_forwarder->AddObserver(this);
}

void ModifierKeyComboRecorder::OnPrerewriteKeyInputEvent(
    const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::EventType::kKeyReleased ||
      key_event.is_repeat()) {
    return;
  }

  UpdateModifierLocations(key_event);

  const AcceleratorKeyInputType type = GetKeyInputTypeFromKeyEvent(key_event);
  const uint32_t modifier_flags = GenerateModifierFlagsFromKeyEvent(key_event);
  const uint32_t hash = CalculateHash(type, modifier_flags);

  // True if Shift is the only modifier key present on the event.
  const bool only_shift_present =
      modifier_flags == GetModifierFlagFromModifier(Modifier::kShift);

  // Do not emit the metric if its only an alpha or digit key pressed (with or
  // without Shift held).
  if ((type == AcceleratorKeyInputType::kAlpha ||
       type == AcceleratorKeyInputType::kDigit) &&
      (modifier_flags == 0 || only_shift_present)) {
    return;
  }

  const auto device_type = Shell::Get()->keyboard_capability()->GetDeviceType(
      key_event.source_device_id());
  RecordMetricForDeviceType(device_type, hash);
}

uint32_t ModifierKeyComboRecorder::GenerateModifierFlagsFromKeyEvent(
    const ui::KeyEvent& event) {
  uint32_t modifier_flags = 0;
  if (event.flags() & ui::EF_ALT_DOWN && !kAltKeys.contains(event.key_code())) {
    modifier_flags += GetModifierFlagFromModifier(Modifier::kAlt);
  }

  // AltGr can be check purely from the modifier flags.
  if (event.flags() & ui::EF_ALTGR_DOWN &&
      !kAltGrKeys.contains(event.key_code())) {
    modifier_flags += 1 << static_cast<uint32_t>(ModifierFlag::kAltGr);
  }

  if (event.flags() & ui::EF_CONTROL_DOWN &&
      !kControlKeys.contains(event.key_code())) {
    modifier_flags += GetModifierFlagFromModifier(Modifier::kControl);
  }

  if (event.flags() & ui::EF_COMMAND_DOWN &&
      !kMetaKeys.contains(event.key_code())) {
    modifier_flags += GetModifierFlagFromModifier(Modifier::kMeta);
  }

  if (event.flags() & ui::EF_SHIFT_DOWN &&
      !kShiftKeys.contains(event.key_code())) {
    modifier_flags += GetModifierFlagFromModifier(Modifier::kShift);
  }

  if (event.flags() & ui::EF_FUNCTION_DOWN &&
      !kFunctionKeys.contains(event.key_code())) {
    modifier_flags += 1 << static_cast<uint32_t>(ModifierFlag::kFunction);
  }

  return modifier_flags;
}

uint32_t ModifierKeyComboRecorder::GetModifierFlagFromModifier(
    Modifier modifier) {
  const ModifierFlag modifier_flag = GetModifierFlagFromModifierAndLocation(
      modifier, modifier_locations_[static_cast<uint32_t>(modifier)]);
  return 1 << static_cast<uint32_t>(modifier_flag);
}

void ModifierKeyComboRecorder::UpdateModifierLocations(
    const ui::KeyEvent& event) {
  auto modifier_modifier_location_pair = GetModifierFromKeyEvent(event);
  if (!modifier_modifier_location_pair) {
    return;
  }

  const auto& [modifier, modifier_location] = *modifier_modifier_location_pair;
  modifier_locations_[static_cast<uint32_t>(modifier)] = modifier_location;
}

}  // namespace ash
