// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_POWER_CONNECTED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_POWER_CONNECTED_PROVIDER_H_

#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"
#include "ash/system/power/power_status.h"

namespace ash {
namespace quick_pair {

class PowerConnectedProvider : public BaseEnabledProvider,
                               public PowerStatus::Observer {
 public:
  explicit PowerConnectedProvider();
  ~PowerConnectedProvider() override;

 private:
  friend class PowerConnectedProviderTest;

  // PoweStatus::Observer
  void OnPowerStatusChanged() override;

  raw_ptr<PowerStatus> power_status_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_POWER_CONNECTED_PROVIDER_H_
