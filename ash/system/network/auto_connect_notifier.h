// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_AUTO_CONNECT_NOTIFIER_H_
#define ASH_SYSTEM_NETWORK_AUTO_CONNECT_NOTIFIER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "chromeos/ash/components/network/auto_connect_handler.h"
#include "chromeos/ash/components/network/network_connection_observer.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace base {
class OneShotTimer;
}

namespace ash {

// Notifies the user when a managed device policy auto-connects to a secure
// network after the user has explicitly requested another network connection.
// See https://crbug.com/764000 for details.
class ASH_EXPORT AutoConnectNotifier : public NetworkConnectionObserver,
                                       public NetworkStateHandlerObserver,
                                       public AutoConnectHandler::Observer {
 public:
  AutoConnectNotifier();

  AutoConnectNotifier(const AutoConnectNotifier&) = delete;
  AutoConnectNotifier& operator=(const AutoConnectNotifier&) = delete;

  ~AutoConnectNotifier() override;

  // NetworkConnectionObserver:
  void ConnectToNetworkRequested(const std::string& service_path) override;

  // NetworkStateHandlerObserver:
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  // AutoConnectHandler::Observer:
  void OnAutoConnectedInitiated(int auto_connect_reasons) override;

  void set_timer_for_testing(std::unique_ptr<base::OneShotTimer> test_timer) {
    timer_ = std::move(test_timer);
  }

  static const char kAutoConnectToastId[];

 private:
  void DisplayToast(const NetworkState* network);

  bool has_user_explicitly_requested_connection_ = false;
  std::string connected_network_guid_;
  std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_AUTO_CONNECT_NOTIFIER_H_
