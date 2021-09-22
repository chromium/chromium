// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CONTROLLER_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CONTROLLER_H_

#include <memory>

#include "ash/components/device_activity/trigger.h"
#include "base/component_export.h"

class PrefRegistrySimple;

namespace ash {
namespace device_activity {

class DeviceActivityClient;

// Counts device actives in a privacy compliant way.
class COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY) DeviceActivityController {
 public:
  // Retrieves a singleton instance.
  static DeviceActivityController* Get();

  // Registers local state preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  DeviceActivityController();
  DeviceActivityController(const DeviceActivityController&) = delete;
  DeviceActivityController& operator=(const DeviceActivityController&) = delete;
  ~DeviceActivityController();

  // Start Device Activity reporting for a trigger.
  void Start(Trigger t);

  // Stop Device Activity reporting for a trigger.
  void Stop(Trigger t);

 private:
  std::unique_ptr<DeviceActivityClient> da_client_network_;
};

}  // namespace device_activity
}  // namespace ash

#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CONTROLLER_H_
