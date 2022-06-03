// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/range_calculations.h"

#include <cmath>

namespace {

// See http://en.wikipedia.org/wiki/Free-space_path_loss.
//
// c   = speed of light (2.9979 x 10^8 m/s);
// f   = frequency (Bluetooth center frequency is 2.44175GHz = 2.44175x10^9 Hz);
// l   = wavelength (in meters);
// d   = distance (from transmitter to receiver in meters);
// dB  = decibels
// dBm = decibel milliwatts
//
// Free-space path loss (FSPL) is proportional to the square of the distance
// between the transmitter and the receiver, and also proportional to the square
// of the frequency of the radio signal.
//
// FSPL      = (4 * pi * d / l)^2 = (4 * pi * d * f / c)^2
//
// FSPL (dB) = 10 * log10((4 * pi * d  * f / c)^2)
//           = 20 * log10(4 * pi * d * f / c)
//           = (20 * log10(d)) + (20 * log10(f)) + (20 * log10(4 * pi/c))
//
// Calculating constants:
//
// FSPL_FREQ        = 20 * log10(f)
//                  = 20 * log10(2.44175 * 10^9)
//                  = 187.75
//
// FSPL_LIGHT       = 20 * log10(4 * pi/c)
//                  = 20 * log10(4 * pi/(2.9979 * 10^8))
//                  = 20 * log10(4 * pi/(2.9979 * 10^8))
//                  = 20 * log10(41.9172441s * 10^-9)
//                  = -147.55
//
// FSPL_DISTANCE_1M = 20 * log10(1)
//                  = 0
//
// PATH_LOSS_AT_1M  = FSPL_DISTANCE_1M + FSPL_FREQ + FSPL_LIGHT
//                  =       0          + 187.75    + (-147.55)
//                  = 40.20db [round to 41db]
//
// Note: Rounding up makes us "closer" and makes us more aggressive at showing
// notifications.
constexpr int kRssiDropOffAtOneMeter = 41;

}  // namespace

namespace ash {
namespace quick_pair {
namespace range_calculations {

int8_t RssiFromTargetDistance(double distance_in_meters, int8_t tx_power) {
  // See https://en.wikipedia.org/wiki/Log-distance_path_loss_model
  //
  // PL       = total path loss in db
  // tx_power = TxPower in dbm
  // rssi     = Received signal strength in dbm
  // PL_0     = Path loss at reference distance d_0 kRssiDropOffAtOneMeter dbm
  // d        = length of path
  // d_0      = reference distance  (1 m)
  // gamma    = path loss exponent (2 in free space)
  //
  // Log-distance path loss (LDPL) formula:
  //
  // PL = tx_power - rssi = PL_0 + 10 * gamma * log_10(d / d_0)
  //
  // PL = tx_power - rssi = kRssiDropOffAtOneMeter + 10 * 2 *
  //      log_10(distanceInMeters / 1)
  //
  // PL = - rssi = -tx_power + kRssiDropOffAtOneMeter + 20 *
  //               log_10(distanceInMeters)
  //
  // PL = rssi = tx_power - kRssiDropOffAtOneMeter - 20 *
  //             log_10(distanceInMeters)
  return distance_in_meters <= 0 ? tx_power
                                 : floor((tx_power - kRssiDropOffAtOneMeter) -
                                         20 * log10(distance_in_meters));
}

double DistanceFromRssiAndTxPower(int8_t rssi, int8_t tx_power) {
  // See https://en.wikipedia.org/wiki/Log-distance_path_loss_model
  //
  // PL       = total path loss in db
  // tx_power = TxPower in dbm
  // rssi     = Received signal strength in dbm
  // PL_0     = Path loss at reference distance d_0 kRssiDropOffAtOneMeter} dbm
  // d        = length of path
  // d_0      = reference distance  (1 m)
  // gamma    = path loss exponent (2 in free space)
  //
  // Log-distance path loss (LDPL) formula:
  //
  // PL = tx_power - rssi = PL_0 + 10 * gamma  * log_10(d / d_0)
  //
  // PL = tx_power - rssi = kRssiDropOffAtOneMeter + 10 * gamma * log_10(d /
  // d_0)
  //
  // PL = tx_power - rssi - kRssiDropOffAtOneMeter = 10 * 2 *
  // log_10(distanceInMeters / 1)
  //
  // PL = tx_power - rssi - kRssiDropOffAtOneMeter = 20 *
  // log_10(distanceInMeters / 1)
  //
  // PL = (tx_power - rssi - kRssiDropOffAtOneMeter) / 20 =
  // log_10(distanceInMeters)
  //
  // PL = 10 ^ ((tx_power - rssi - kRssiDropOffAtOneMeter) / 20) =
  // distanceInMeters
  //
  return pow(10, (tx_power - rssi - kRssiDropOffAtOneMeter) / 20.0);
}

}  // namespace range_calculations
}  // namespace quick_pair
}  // namespace ash
