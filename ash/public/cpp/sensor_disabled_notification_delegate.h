// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SENSOR_DISABLED_NOTIFICATION_DELEGATE_H_
#define ASH_PUBLIC_CPP_SENSOR_DISABLED_NOTIFICATION_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/containers/enum_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// This delegate exists so that code relevant to sensor (microphone and camera)
// disabled notifications under //ash can call back into //chrome.  The actual
// delegate instance is owned and constructed by code in //chrome during
// startup.
class ASH_PUBLIC_EXPORT SensorDisabledNotificationDelegate {
 public:
  static SensorDisabledNotificationDelegate* Get();

  enum class Sensor {
    kCamera,
    kMinValue = kCamera,
    kLocation,
    kMicrophone,
    kMaxValue = kMicrophone,
  };

  using SensorSet = base::EnumSet<Sensor, Sensor::kMinValue, Sensor::kMaxValue>;

  // Returns a list of names of the applications that have attempted to use the
  // sensor (camera or microphone), in order of most-recently-launched. If an
  // application is accessing the sensor but no name could be determined, the
  // name of that application will not be in the returned list.
  virtual std::vector<std::u16string> GetAppsAccessingSensor(Sensor sensor) = 0;

 protected:
  SensorDisabledNotificationDelegate();
  virtual ~SensorDisabledNotificationDelegate();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SENSOR_DISABLED_NOTIFICATION_DELEGATE_H_
