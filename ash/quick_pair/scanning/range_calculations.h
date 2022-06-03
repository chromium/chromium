// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_RANGE_CALCULATIONS_H_
#define ASH_QUICK_PAIR_SCANNING_RANGE_CALCULATIONS_H_

#include <stdint.h>

namespace ash {
namespace quick_pair {
namespace range_calculations {

// Convert target distance and txPower to a RSSI value using the Log-distance
// path loss model with Path Loss at 1m of 41db.
// See https://en.wikipedia.org/wiki/Log-distance_path_loss_model
int8_t RssiFromTargetDistance(double distance_in_meters, int8_t tx_power);

// Convert RSSI and txPower to a distance value using the Log-distance path loss
// model with Path Loss at 1m of 41db.
// See https://en.wikipedia.org/wiki/Log-distance_path_loss_model
double DistanceFromRssiAndTxPower(int8_t rssi, int8_t tx_power);

}  // namespace range_calculations
}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_SCANNING_RANGE_CALCULATIONS_H_
