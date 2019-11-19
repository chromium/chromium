// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_AUTO_CONNECT_NOTIFIER_H_
#define ASH_SYSTEM_NETWORK_AUTO_CONNECT_NOTIFIER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "chromeos/network/auto_connect_handler.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace base {
class OneShotTimer;
}

namespace ash {

// Notifies the user when a managed device policy auto-connects to a secure
// network after the user has explicitly requested another network connection.
// See https://crbug.com/764000 for details.
class ASH_EXPORT AutoConnectNotifier
    : public chromeos::NetworkConnectionObserver,
      public chromeos::NetworkStateHandlerObserver,
      public chromeos::AutoConnectHandler::Observer {
 public:
  AutoConnectNotifier();
  ~AutoConnectNotifier() override;

  // chromeos::NetworkConnectionObserver:
  void ConnectToNetworkRequested(const std::string& service_path) override;

  // chromeos::NetworkStateHandlerObserver:
  void NetworkConnectionStateChanged(
      const chromeos::NetworkState* network) override;

  // chromeos::AutoConnectHandler::Observer:
  void OnAutoConnectedInitiated(int auto_connect_reasons) override;

  void set_timer_for_testing(std::unique_ptr<base::OneShotTimer> test_timer) {
    timer_ = std::move(test_timer);
  }

  static const char kAutoConnectNotificationId[];

 private:
  void DisplayNotification(const chromeos::NetworkState* network);

  bool has_user_explicitly_requested_connection_ = false;
  std::string connected_network_guid_;
  std::unique_ptr<base::OneShotTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(AutoConnectNotifier);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_AUTO_CONNECT_NOTIFIER_H_
