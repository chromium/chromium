// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/accelerometer/accelerometer_types.h"

#include <cmath>

#include "base/numerics/math_constants.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace ash {
namespace {

// The maximum deviation from the acceleration expected due to gravity for which
// the device will be considered stable: 1g.
constexpr float kDeviationFromGravityThreshold = 1.0f;

}  // namespace

AccelerometerReading::AccelerometerReading() = default;

AccelerometerReading::~AccelerometerReading() = default;

AccelerometerUpdate::AccelerometerUpdate() = default;

AccelerometerUpdate::AccelerometerUpdate(const AccelerometerUpdate& update) {
  *this = update;
}

AccelerometerUpdate& AccelerometerUpdate::operator=(
    const AccelerometerUpdate& update) {
  if (this == &update)
    return *this;

  for (int i = 0; i < ACCELEROMETER_SOURCE_COUNT; ++i) {
    data_[i].present = update.data_[i].present;
    data_[i].x = update.data_[i].x;
    data_[i].y = update.data_[i].y;
    data_[i].z = update.data_[i].z;
  }

  return *this;
}

AccelerometerUpdate::~AccelerometerUpdate() = default;

gfx::Vector3dF AccelerometerUpdate::GetVector(
    AccelerometerSource source) const {
  const AccelerometerReading& reading = data_[source];
  return gfx::Vector3dF(reading.x, reading.y, reading.z);
}

void AccelerometerUpdate::Set(AccelerometerSource source,
                              float x,
                              float y,
                              float z) {
  data_[source].present = true;
  data_[source].x = x;
  data_[source].y = y;
  data_[source].z = z;
}

bool AccelerometerUpdate::IsReadingStable(AccelerometerSource source) const {
  if (!has(source))
    return false;

  return std::abs(GetVector(source).Length() - base::kMeanGravityFloat) <=
         kDeviationFromGravityThreshold;
}

void AccelerometerUpdate::Reset() {
  for (int i = 0; i < ACCELEROMETER_SOURCE_COUNT; ++i) {
    data_[i].present = false;
    data_[i].x = 0;
    data_[i].y = 0;
    data_[i].z = 0;
  }
}

}  // namespace ash
