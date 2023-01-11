// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_alias_converter.h"

#include <vector>

#include "ash/shell.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/chromeos/events/keyboard_capability.h"

namespace ash {

// TODO(zhangwenyu): This should also handle creating alias for all different
// keyboard layouts connected.
std::vector<ui::Accelerator> AcceleratorAliasConverter::CreateAcceleratorAlias(
    const ui::Accelerator& accelerator) const {
  std::vector<ui::Accelerator> accelerator_aliases;

  if (Shell::Get()->keyboard_capability()->IsTopRowKey(
          accelerator.key_code())) {
    // For |top_row_key|, replace the base accelerator with top-row remapped
    // accelerator if applicable. Otherwise, only show base accelerator.
    absl::optional<ui::Accelerator> alias = CreateTopRowAlias(accelerator);
    if (alias.has_value()) {
      accelerator_aliases.push_back(alias.value());
      return accelerator_aliases;
    }
  }

  if (Shell::Get()->keyboard_capability()->IsSixPackKey(
          accelerator.key_code())) {
    // For |six_pack_key|, show both the base accelerator and the six-pack
    // remapped accelerator if applicable. Otherwise, only show base
    // accelerator.
    absl::optional<ui::Accelerator> alias = CreateSixPackAlias(accelerator);
    if (alias.has_value()) {
      accelerator_aliases.push_back(alias.value());
    }
  }

  // Add base accelerator.
  accelerator_aliases.push_back(accelerator);
  return accelerator_aliases;
}

absl::optional<ui::Accelerator> AcceleratorAliasConverter::CreateTopRowAlias(
    const ui::Accelerator& accelerator) const {
  // Avoid remapping if [Search] is part of the original accelerator.
  // TODO(zhangwenyu): Handle all 4 legacy layouts and custom vivaldi layouts.
  if (accelerator.IsCmdDown() ||
      !Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys() ||
      !ui::kLayout2TopRowKeyToFKeyMap.contains(accelerator.key_code())) {
    return absl::nullopt;
  }

  // If top row keys are function keys, top row shortcut will become
  // [FKey] + [Search] + [modifiers]
  return ui::Accelerator(
      ui::kLayout2TopRowKeyToFKeyMap.at(accelerator.key_code()),
      accelerator.modifiers() | ui::EF_COMMAND_DOWN, accelerator.key_state());
}

absl::optional<ui::Accelerator> AcceleratorAliasConverter::CreateSixPackAlias(
    const ui::Accelerator& accelerator) const {
  // For all |six_pack_keys|, avoid remapping if [Search] is part of the
  // original accelerator.
  if (accelerator.IsCmdDown() ||
      !::features::IsImprovedKeyboardShortcutsEnabled() ||
      !ui::kSixPackKeyToSystemKeyMap.contains(accelerator.key_code())) {
    return absl::nullopt;
  }

  // Edge cases:
  // 1. [Shift] + [Delete] should not be remapped to [Shift] + [Search] +
  // [Back] (aka, Insert).
  // 2. For [Insert], avoid remapping if [Shift] is part of original
  // accelerator.
  if (accelerator.IsShiftDown() &&
      (accelerator.key_code() == ui::KeyboardCode::VKEY_DELETE ||
       accelerator.key_code() == ui::KeyboardCode::VKEY_INSERT)) {
    return absl::nullopt;
  }

  // For Insert: [modifiers] = [Search] + [Shift] + [original_modifiers].
  // For other |six_pack_keys|: [modifiers] = [Search] + [original_modifiers].
  int updated_modifiers =
      accelerator.key_code() == ui::KeyboardCode::VKEY_INSERT
          ? accelerator.modifiers() | ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN
          : accelerator.modifiers() | ui::EF_COMMAND_DOWN;
  return ui::Accelerator(
      ui::kSixPackKeyToSystemKeyMap.at(accelerator.key_code()),
      updated_modifiers, accelerator.key_state());
}

}  // namespace ash
