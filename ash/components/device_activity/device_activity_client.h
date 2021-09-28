// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_

#include <memory>

#include "ash/components/device_activity/trigger.h"
#include "base/component_export.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace ash {
namespace device_activity {

class DeviceActivityClientTest;

// Counts device activity while guaranteeing complete device privacy.
class COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY) DeviceActivityClient {
 public:
  class NetworkObserver;

  DeviceActivityClient(Trigger t, NetworkStateHandler* handler);
  DeviceActivityClient(const DeviceActivityClient&) = delete;
  DeviceActivityClient& operator=(const DeviceActivityClient&) = delete;
  ~DeviceActivityClient();

 private:
  NetworkStateHandler* const network_state_handler_;
  std::unique_ptr<NetworkStateHandlerObserver> network_observer_;
};

}  // namespace device_activity
}  // namespace ash

#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
