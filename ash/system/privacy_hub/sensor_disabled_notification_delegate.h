// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_SENSOR_DISABLED_NOTIFICATION_DELEGATE_H_
#define ASH_SYSTEM_PRIVACY_HUB_SENSOR_DISABLED_NOTIFICATION_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/enum_set.h"

namespace ash {

// This delegate exists so that code relevant to sensor (microphone and camera)
// disabled notifications under //ash can call back into //chrome.  The actual
// delegate instance is owned and constructed by code in //chrome during
// startup.
class ASH_EXPORT SensorDisabledNotificationDelegate {
 public:
  enum class Sensor {
    kCamera,
    kMinValue = kCamera,
    kLocation,
    kMicrophone,
    kMaxValue = kMicrophone,
  };

  using SensorSet = base::EnumSet<Sensor, Sensor::kMinValue, Sensor::kMaxValue>;

  virtual ~SensorDisabledNotificationDelegate();

  // Returns a list of names of the applications that have attempted to use the
  // sensor (camera or microphone), in order of most-recently-launched. If an
  // application is accessing the sensor but no name could be determined, the
  // name of that application will not be in the returned list.
  virtual std::vector<std::u16string> GetAppsAccessingSensor(Sensor sensor) = 0;
};

// This is used to set a fake notification delegate in tests.
class ASH_EXPORT ScopedSensorDisabledNotificationDelegateForTest {
 public:
  explicit ScopedSensorDisabledNotificationDelegateForTest(
      std::unique_ptr<SensorDisabledNotificationDelegate> delegate);
  ~ScopedSensorDisabledNotificationDelegateForTest();

 private:
  std::unique_ptr<SensorDisabledNotificationDelegate> previous_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_SENSOR_DISABLED_NOTIFICATION_DELEGATE_H_
