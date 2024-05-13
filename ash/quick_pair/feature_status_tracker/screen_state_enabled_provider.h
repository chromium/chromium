// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_SCREEN_STATE_ENABLED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_SCREEN_STATE_ENABLED_PROVIDER_H_

#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"
#include "base/scoped_observation.h"
#include "ui/display/manager/display_configurator.h"

namespace ash {
namespace quick_pair {

// Observes whether the screen state of any display is on.
class ScreenStateEnabledProvider
    : public BaseEnabledProvider,
      public display::DisplayConfigurator::Observer {
 public:
  ScreenStateEnabledProvider();
  ~ScreenStateEnabledProvider() override;

  // display::DisplayConfigurator::Observer
  void OnDisplayConfigurationChanged(
      const display::DisplayConfigurator::DisplayStateList& display_states)
      override;

 private:
  base::ScopedObservation<display::DisplayConfigurator,
                          display::DisplayConfigurator::Observer>
      configurator_observation_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_SCREEN_STATE_ENABLED_PROVIDER_H_
