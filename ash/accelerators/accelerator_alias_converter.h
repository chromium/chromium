// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_ALIAS_CONVERTER_H_
#define ASH_ACCELERATORS_ACCELERATOR_ALIAS_CONVERTER_H_

#include <vector>

#include "ash/ash_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"

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
  // Create accelerator alias for |top_row_key| if applicable.
  absl::optional<ui::Accelerator> CreateTopRowAlias(
      const ui::Accelerator& accelerator) const;

  // Create accelerator alias for |six_pack_key| if applicable.
  absl::optional<ui::Accelerator> CreateSixPackAlias(
      const ui::Accelerator& accelerator) const;

  // Create reversed six pack alias for |reversed_six_pack_key| if applicable.
  absl::optional<ui::Accelerator> CreateReversedSixPackAlias(
      const ui::Accelerator& accelerator) const;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_ALIAS_CONVERTER_H_
