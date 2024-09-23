// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerator_actions.h"

#include <string>

#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"

namespace ash {

const char* GetAcceleratorActionName(AcceleratorAction action) {
  // Define the mapping between an AcceleratorAction and its string name.
  // Example:
  //   AcceleratorAction::kDevToggleUnifiedDesktop -> "DevToggleUnifiedDesktop".
  constexpr static auto kAcceleratorActionToName =
      base::MakeFixedFlatMap<AcceleratorAction, const char*>({
#define ACCELERATOR_ACTION_ENTRY(action) \
  {AcceleratorAction::k##action, #action},
#define ACCELERATOR_ACTION_ENTRY_FIXED_VALUE(action, value) \
  {AcceleratorAction::k##action, #action},
          ACCELERATOR_ACTIONS
#undef ACCELERATOR_ACTION_ENTRY
#undef ACCELERATOR_ACTION_ENTRY_FIXED_VALUE
      });
  auto iter = kAcceleratorActionToName.find(action);
  CHECK(iter != kAcceleratorActionToName.end());
  return iter->second;
}

}  // namespace ash
