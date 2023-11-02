// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_LAYOUT_TABLE_H_
#define ASH_ACCELERATORS_ACCELERATOR_LAYOUT_TABLE_H_

#include <cstdint>
#include <functional>

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "base/containers/fixed_flat_map.h"

namespace {

// Derived from the actions listed in `ash/accelerators/accelerator_table.h`.
constexpr int kNumAcceleratorActions = 143;

}  // namespace

namespace ash {

// Contains details for UI styling of an accelerator.
struct ASH_EXPORT AcceleratorLayoutDetails {
  // Category of the accelerator.
  mojom::AcceleratorCategory category;

  // Subcategory of the accelerator.
  mojom::AcceleratorSubcategory sub_category;

  // True if the accelerator cannot be modified through customization.
  // False if the accelerator can be modified through customization.
  bool locked;

  // The layout style of the accelerator, this provides additional context
  // on how to accelerator should be represented in the UI.
  mojom::AcceleratorLayoutStyle layout_style;
};

// A map between an accelerator action id and AcceleratorLayoutDetails. This map
// provides the UI-related details for an accelerator.
// Adding a new accelerator must add a new entry to this map.
// TODO(jimmyxgong): This is a stub map with stub details, replace with real
// one when categorization is available.
ASH_EXPORT extern const base::fixed_flat_map<AcceleratorAction,
                                             AcceleratorLayoutDetails,
                                             kNumAcceleratorActions,
                                             std::less<>>
    kAcceleratorLayouts;
}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_LAYOUT_TABLE_H_
