// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sensor_info/sensor_types.h"

#include "base/check_op.h"
#include "base/numerics/math_constants.h"

namespace ash {

SensorReading::SensorReading() = default;

SensorReading::SensorReading(float x) : x(x) {
  y = std::numeric_limits<float>::quiet_NaN();
  z = std::numeric_limits<float>::quiet_NaN();
}

SensorReading::SensorReading(float x, float y, float z) : x(x), y(y), z(z) {}

SensorReading::~SensorReading() = default;

// -----------------------------------------------------------------------------
// SensorUpdate:

SensorUpdate::SensorUpdate() = default;

SensorUpdate::SensorUpdate(const SensorUpdate& update) = default;

SensorUpdate& SensorUpdate::operator=(const SensorUpdate& update) = default;

SensorUpdate::~SensorUpdate() = default;

std::vector<float> SensorUpdate::GetReadingAsVector(SensorType source) const {
  const absl::optional<SensorReading>& reading = data_[source];
  if (source == SensorType::LID_ANGLE) {
    return reading.has_value() ? std::vector<float>{reading->x}
                               : std::vector<float>{0.0};
  }
  return reading.has_value()
             ? std::vector<float>{reading->x, reading->y, reading->z}
             : std::vector<float>{0.0, 0.0, 0.0};
}

void SensorUpdate::Set(SensorType source, float x, float y, float z) {
  CHECK_NE(source, SensorType::LID_ANGLE);
  data_[source] = SensorReading(x, y, z);
}

void SensorUpdate::Set(SensorType source, float x) {
  DCHECK_EQ(source, SensorType::LID_ANGLE);
  data_[source] = SensorReading(x);
}

void SensorUpdate::Reset() {
  for (auto& i : data_) {
    i = absl::nullopt;
  }
}

}  // namespace ash
