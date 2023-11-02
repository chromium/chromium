// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_WIFI_TOGGLE_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_NETWORK_WIFI_TOGGLE_NOTIFICATION_CONTROLLER_H_

#include "ash/system/network/network_observer.h"

namespace ash {

class WifiToggleNotificationController : public NetworkObserver {
 public:
  WifiToggleNotificationController();

  WifiToggleNotificationController(const WifiToggleNotificationController&) =
      delete;
  WifiToggleNotificationController& operator=(
      const WifiToggleNotificationController&) = delete;

  ~WifiToggleNotificationController() override;

  // NetworkObserver:
  void RequestToggleWifi() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_WIFI_TOGGLE_NOTIFICATION_CONTROLLER_H_
