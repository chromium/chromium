// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/screen_state_enabled_provider.h"

#include "ash/shell.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"

namespace ash {
namespace quick_pair {

ScreenStateEnabledProvider::ScreenStateEnabledProvider() {
  auto* configurator = Shell::Get()->display_configurator();
  DCHECK(configurator);

  configurator_observation_.Observe(configurator);
  // IsDisplayOn() is true for a screen with brightness 0 but in practice
  // that edge case does not occur at initialization. We cover that edge
  // case later but we don't have access to a DisplayStateList here.
  SetEnabledAndInvokeCallback(configurator->IsDisplayOn());
}

ScreenStateEnabledProvider::~ScreenStateEnabledProvider() = default;

void ScreenStateEnabledProvider::OnDisplayConfigurationChanged(
    const display::DisplayConfigurator::DisplayStateList& display_states) {
  for (const display::DisplaySnapshot* state : display_states) {
    // If a display has current_mode, then it is (1) an external monitor or (2)
    // the internal display with non-zero brightness and an open laptop lid.
    if (state->current_mode()) {
      SetEnabledAndInvokeCallback(true);
      return;
    }
  }

  SetEnabledAndInvokeCallback(false);
}

}  // namespace quick_pair
}  // namespace ash
