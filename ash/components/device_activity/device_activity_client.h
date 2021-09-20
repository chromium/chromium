// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_

#include "ash/components/device_activity/trigger.h"
#include "base/component_export.h"

namespace ash {
namespace device_activity {

// Counts device activity while guaranteeing complete device privacy.
class COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY) DeviceActivityClient {
 public:
  explicit DeviceActivityClient(Trigger t);
  DeviceActivityClient(const DeviceActivityClient&) = delete;
  DeviceActivityClient& operator=(const DeviceActivityClient&) = delete;
  ~DeviceActivityClient() = default;
};

}  // namespace device_activity
}  // namespace ash

#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
