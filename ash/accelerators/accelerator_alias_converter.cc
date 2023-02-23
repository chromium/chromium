// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_alias_converter.h"

#include <vector>

#include "ash/shell.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/chromeos/events/keyboard_capability.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

// TODO(zhangwenyu): Handle cases when an accelerator should be suppressed
// because certain keys are unavailable.
std::vector<ui::Accelerator> AcceleratorAliasConverter::CreateAcceleratorAlias(
    const ui::Accelerator& accelerator) const {
  // For |top_row_key|, replace the base accelerator with top-row remapped
  // accelerator if applicable. Otherwise, only show base accelerator.
  std::vector<ui::Accelerator> aliases = CreateTopRowAliases(accelerator);
  // Return early here since an accelerator can never have a top row alias and a
  // six pack alias at the same time. Because top row keys and six pack keys are
  // two completely different sets of keys.
  if (!aliases.empty()) {
    return aliases;
  }

  // For |six_pack_key| and |reversed_six_pack_key|, show both the base
  // accelerator and the remapped accelerator if applicable. Otherwise, only
  // show base accelerator.
  aliases = CreateSixPackAliases(accelerator);
  std::vector<ui::Accelerator> reversed_aliases =
      CreateReversedSixPackAliases(accelerator);
  // An accelerator can never have both six pack alias and reversed six
  // pack alias at the same time. Concatenating two vectors works here. Note
  // that both vectors could be empty.
  aliases.insert(aliases.end(), reversed_aliases.begin(),
                 reversed_aliases.end());

  // Add base accelerator.
  aliases.push_back(accelerator);
  return aliases;
}

std::vector<ui::Accelerator> AcceleratorAliasConverter::CreateTopRowAliases(
    const ui::Accelerator& accelerator) const {
  // TODO(zhangwenyu): Handle the case when meta + top row key rewrite is
  // suppressed, following https://crrev.com/c/4160339.
  // Avoid remapping if [Search] is part of the original accelerator.
  if (accelerator.IsCmdDown() ||
      !Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys()) {
    return std::vector<ui::Accelerator>();
  }

  // Deduping is needed since keyboards with the same top row layouts generate
  // the same alias. Use flat_set since the size is small.
  base::flat_set<ui::Accelerator> aliases_set;
  // TODO(zhangwenyu): Handle custom vivaldi layouts.
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    absl::optional<ui::KeyboardCode> f_key =
        Shell::Get()->keyboard_capability()->GetMappedFKeyIfExists(
            accelerator.key_code(), keyboard);
    if (f_key.has_value()) {
      // If top row keys are function keys, top row shortcut will become
      // [FKey] + [Search] + [modifiers]
      aliases_set.insert(ui::Accelerator(
          f_key.value(), accelerator.modifiers() | ui::EF_COMMAND_DOWN,
          accelerator.key_state()));
    }
  }

  return std::vector<ui::Accelerator>(aliases_set.begin(), aliases_set.end());
}

std::vector<ui::Accelerator> AcceleratorAliasConverter::CreateSixPackAliases(
    const ui::Accelerator& accelerator) const {
  // For all |six_pack_keys|, avoid remapping if [Search] is part of the
  // original accelerator.
  if (accelerator.IsCmdDown() ||
      !::features::IsImprovedKeyboardShortcutsEnabled() ||
      !ui::KeyboardCapability::IsSixPackKey(accelerator.key_code())) {
    return std::vector<ui::Accelerator>();
  }

  // Edge cases:
  // 1. [Shift] + [Delete] should not be remapped to [Shift] + [Search] +
  // [Back] (aka, Insert).
  // 2. For [Insert], avoid remapping if [Shift] is part of original
  // accelerator.
  if (accelerator.IsShiftDown() &&
      (accelerator.key_code() == ui::KeyboardCode::VKEY_DELETE ||
       accelerator.key_code() == ui::KeyboardCode::VKEY_INSERT)) {
    return std::vector<ui::Accelerator>();
  }

  // For Insert: [modifiers] = [Search] + [Shift] + [original_modifiers].
  // For other |six_pack_keys|: [modifiers] = [Search] + [original_modifiers].
  int updated_modifiers =
      accelerator.key_code() == ui::KeyboardCode::VKEY_INSERT
          ? accelerator.modifiers() | ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN
          : accelerator.modifiers() | ui::EF_COMMAND_DOWN;
  return {
      ui::Accelerator(ui::kSixPackKeyToSystemKeyMap.at(accelerator.key_code()),
                      updated_modifiers, accelerator.key_state())};
}

std::vector<ui::Accelerator>
AcceleratorAliasConverter::CreateReversedSixPackAliases(
    const ui::Accelerator& accelerator) const {
  // To find the reversed six pack alias, an accelerator must include [Search]
  // key, and must be one of the reversed six pack keys.
  if (!accelerator.IsCmdDown() ||
      !::features::IsImprovedKeyboardShortcutsEnabled() ||
      !ui::KeyboardCapability::IsReversedSixPackKey(accelerator.key_code())) {
    return std::vector<ui::Accelerator>();
  }

  int modifiers = accelerator.modifiers() & ~ui::EF_COMMAND_DOWN;
  // [Back] maps back to [Insert] if modifier contains [Shift]. Otherwise,
  // it maps back to [Delete].
  if (accelerator.key_code() == ui::KeyboardCode::VKEY_BACK) {
    if (!accelerator.IsShiftDown()) {
      return {ui::Accelerator(ui::KeyboardCode::VKEY_DELETE, modifiers,
                              accelerator.key_state())};
    }

    modifiers &= ~ui::EF_SHIFT_DOWN;
    return {ui::Accelerator(ui::KeyboardCode::VKEY_INSERT, modifiers,
                            accelerator.key_state())};
  }

  // Handle modifiers other than [Back].
  return {ui::Accelerator(
      ui::kReversedSixPackKeyToSystemKeyMap.at(accelerator.key_code()),
      modifiers, accelerator.key_state())};
}

}  // namespace ash
