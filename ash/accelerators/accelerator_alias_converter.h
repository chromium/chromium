// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_ALIAS_CONVERTER_H_
#define ASH_ACCELERATORS_ACCELERATOR_ALIAS_CONVERTER_H_

#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {
struct KeyboardDevice;
}

namespace ash {

// AcceleratorAliasConverter class creates alias for given accelerators,
// including top row key alias, six pack key alias and reversed six pack key,
// etc.
class ASH_EXPORT AcceleratorAliasConverter {
 public:
  AcceleratorAliasConverter() = default;
  AcceleratorAliasConverter(const AcceleratorAliasConverter&) = delete;
  AcceleratorAliasConverter& operator=(const AcceleratorAliasConverter&) =
      delete;
  ~AcceleratorAliasConverter() = default;

  // Create accelerator alias when the accelerator contains a top row key,
  // six pack key or reversed six pack key. For |top_row_key|, replace the base
  // accelerator with top-row remapped accelerator. For |six_pack_key| and
  // |reversed_six_pack_key|, show both the base accelerator and the remapped
  // accelerator. Use a vector here since it may display two accelerators.
  std::vector<ui::Accelerator> CreateAcceleratorAlias(
      const ui::Accelerator& accelerator) const;

 private:
  // Create accelerator alias for |top_row_key| for the given |keyboard|.
  std::optional<ui::Accelerator> CreateTopRowAliases(
      const ui::KeyboardDevice& keyboard,
      const ui::Accelerator& accelerator) const;

  // Create accelerator alias for |function_key| for the given |keyboard|.
  std::optional<ui::Accelerator> CreateFunctionKeyAliases(
      const ui::KeyboardDevice& keyboard,
      const ui::Accelerator& accelerator) const;

  // Create accelerator alias for |six_pack_key|. Result could be either zero or
  // one alias found. Use a vector to be more consistent and cleaner.
  std::vector<ui::Accelerator> CreateSixPackAliases(
      const ui::Accelerator& accelerator,
      std::optional<int> device_id) const;

  // Create accelerator alias for |capslock| for the given |keyboard|.
  std::optional<ui::Accelerator> CreateCapsLockAliases(
      const ui::KeyboardDevice& keyboard,
      const ui::Accelerator& accelerator) const;

  // Create accelerator alias for extended function keys (F11+) for the
  // given `keyboard`.
  std::optional<ui::Accelerator> CreateExtendedFKeysAliases(
      const ui::KeyboardDevice& keyboard,
      const ui::Accelerator& accelerator,
      std::optional<int> device_id) const;

  // Given a list of accelerators, filter out those accelerators that have
  // unsupported keys. Return a list of filtered accelerators with supported
  // keys only.
  std::vector<ui::Accelerator> FilterAliasBySupportedKeys(
      const std::vector<ui::Accelerator>& accelerators) const;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_ALIAS_CONVERTER_H_
