// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELEROMETER_ACCELEROMETER_CONSTANTS_H_
#define ASH_ACCELEROMETER_ACCELEROMETER_CONSTANTS_H_

#include "ash/accelerometer/accelerometer_types.h"

namespace ash {

const char kAccelerometerChannels[][8] = {"accel_x", "accel_y", "accel_z"};

const char kGyroscopeChannels[][10] = {"anglvel_x", "anglvel_y", "anglvel_z"};

// The number of axes for which there are accelerometer readings.
constexpr uint32_t kNumberOfAxes = 3u;

// The names of the accelerometers. Matches up with the enum AccelerometerSource
// in ash/accelerometer/accelerometer_types.h.
const char kLocationStrings[ACCELEROMETER_SOURCE_COUNT][5] = {"lid", "base"};

}  // namespace ash

#endif  // ASH_ACCELEROMETER_ACCELEROMETER_CONSTANTS_H_
