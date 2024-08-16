// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef ASH_SENSOR_INFO_SENSOR_TYPES_H_
#define ASH_SENSOR_INFO_SENSOR_TYPES_H_

#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "base/observer_list_types.h"

namespace ash {

// Location of sensor. There are three types of sensors: gyroscope,
// accelerometer, lid_angle.
enum class SensorLocation {
  kLid,
  kBase,
  kOther,
};

// Sensor types.
enum class SensorType {
  kLidAngle = 0,
  kAccelerometerBase = 1,
  kAccelerometerLid = 2,
  kGyroscopeBase = 3,
  kGyroscopeLid = 4,
  kSensorTypeCount = 5,
};

// Stores one sensor's reading info.
struct ASH_EXPORT SensorReading {
  SensorReading();
  // We use SensorReading(x) for lid_angle sensor, and SensorReading(x, y, z)
  // for gyroscope and accelerometer.
  explicit SensorReading(float x);
  SensorReading(float x, float y, float z);
  ~SensorReading();

  // The readings from this sensor.
  float x;
  float y;
  float z;
};

// Stores all present sensors' reading info.
class ASH_EXPORT SensorUpdate {
 public:
  SensorUpdate();
  SensorUpdate(const SensorUpdate& update);
  SensorUpdate& operator=(const SensorUpdate& update);
  ~SensorUpdate();

  // Returns true if `source` has a valid value in this update.
  bool has(SensorType source) const {
    return data_[static_cast<int>(source)].has_value();
  }
  // Returns the last known value for |source|.
  const std::optional<SensorReading>& get(SensorType source) const {
    return data_[static_cast<int>(source)];
  }

  // Returns the last known value for `source` as a vector.
  std::vector<float> GetReadingAsVector(SensorType source) const;
  // We use Set(x) for lid_angle sensor, and Set(x, y, z) for gyroscope and
  // accelerometer.
  void Set(SensorType source, float x);
  void Set(SensorType source, float x, float y, float z);
  // Clear content in data_.
  void Reset();

 protected:
  std::optional<SensorReading>
      data_[static_cast<int>(SensorType::kSensorTypeCount)];
};

// Class for all potential observers for sensor updates.
class ASH_EXPORT SensorObserver : public base::CheckedObserver {
 public:
  // SensorProvider will gather updates from AccelGyroSamplesObserver. Then
  // SensorProvider will call OnSensorUpdated to notify SensorObserver.
  virtual void OnSensorUpdated(const SensorUpdate& update) = 0;
};

}  // namespace ash

#endif  // ASH_SENSOR_INFO_SENSOR_TYPES_H_
