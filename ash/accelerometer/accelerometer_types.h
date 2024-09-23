// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef ASH_ACCELEROMETER_ACCELEROMETER_TYPES_H_
#define ASH_ACCELEROMETER_ACCELEROMETER_TYPES_H_

#include "ash/ash_export.h"

namespace gfx {
class Vector3dF;
}

namespace ash {

enum AccelerometerSource {
  // Accelerometer is located in the device's screen. In the screen's natural
  // orientation, positive X points to the right, consistent with the pixel
  // position. Positive Y points up the screen. Positive Z is perpendicular to
  // the screen, pointing outwards towards the user. The orientation is
  // described at:
  // http://www.html5rocks.com/en/tutorials/device/orientation/.
  ACCELEROMETER_SOURCE_SCREEN = 0,

  // Accelerometer is located in a keyboard attached to the device's screen.
  // If the device is open 180 degrees the orientation is consistent with the
  // screen. I.e. Positive X points to the right, positive Y points up on the
  // keyboard and positive Z is perpendicular to the keyboard pointing out
  // towards the user.
  ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD,

  ACCELEROMETER_SOURCE_COUNT
};

struct ASH_EXPORT AccelerometerReading {
  AccelerometerReading();
  ~AccelerometerReading();

  // If true, this accelerometer is being updated.
  bool present = false;

  // The readings from this accelerometer measured in m/s^2.
  float x;
  float y;
  float z;
};

// An accelerometer update contains the last known value for each of the
// accelerometers present on the device.
class ASH_EXPORT AccelerometerUpdate {
 public:
  AccelerometerUpdate();
  AccelerometerUpdate(const AccelerometerUpdate& update);
  AccelerometerUpdate& operator=(const AccelerometerUpdate& update);
  ~AccelerometerUpdate();

  // Returns true if |source| has a valid value in this update.
  bool has(AccelerometerSource source) const { return data_[source].present; }

  // Returns the last known value for |source|.
  const AccelerometerReading& get(AccelerometerSource source) const {
    return data_[source];
  }

  // Returns the last known value for |source| as a vector.
  gfx::Vector3dF GetVector(AccelerometerSource source) const;

  void Set(AccelerometerSource source, float x, float y, float z);

  // A reading is considered stable if its deviation from gravity is small. This
  // returns false if the deviation is too high, or if |source| is not present
  // in the update.
  bool IsReadingStable(AccelerometerSource source) const;

  void Reset();

 protected:
  AccelerometerReading data_[ACCELEROMETER_SOURCE_COUNT];
};

}  // namespace ash

#endif  // ASH_ACCELEROMETER_ACCELEROMETER_TYPES_H_
