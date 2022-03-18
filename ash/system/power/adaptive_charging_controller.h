// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_CONTROLLER_H_
#define ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_CONTROLLER_H_

namespace ash {

// The controller responsible for the adaptive charging toast and notifications,
// and communication with the power daemon.
//
// Is currently a stub. TODO(b:216035280): add in real logic.
class AdaptiveChargingController {
 public:
  AdaptiveChargingController();
  AdaptiveChargingController(const AdaptiveChargingController&) = delete;
  AdaptiveChargingController& operator=(const AdaptiveChargingController&) =
      delete;
  ~AdaptiveChargingController();
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_CONTROLLER_H_
