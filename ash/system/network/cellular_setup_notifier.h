// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_CELLULAR_SETUP_NOTIFIER_H_
#define ASH_SYSTEM_NETWORK_CELLULAR_SETUP_NOTIFIER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/gtest_prod_util.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefRegistrySimple;

namespace base {
class OneShotTimer;
}  // namespace base

namespace ash {

// Notifies the user after OOBE to finish setting up their cellular network if
// user has a device with eSIM but no profiles have been configured, or they
// inserted a cold pSIM and need to provision in-session.
class ASH_EXPORT CellularSetupNotifier
    : public SessionObserver,
      public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  CellularSetupNotifier();
  CellularSetupNotifier(const CellularSetupNotifier&) = delete;
  CellularSetupNotifier& operator=(const CellularSetupNotifier&) = delete;
  ~CellularSetupNotifier() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  friend class CellularSetupNotifierTest;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // CrosNetworkConfigObserver:
  void OnDeviceStateListChanged() override;
  void OnNetworkStateListChanged() override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override;

  void OnGetNetworkStateList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  void OnGetDeviceStateList(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
          devices);

  // This function will evaluate whether the timer should be started, or whether
  // it should be stopped if it is already running.
  void StartStopTimer();

  void StopTimerOrHideNotification();
  void ShowCellularSetupNotification();

  static const char kCellularSetupNotificationId[];

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};

  // Notifications can only be shown when there is an active session.
  bool has_active_session_{false};
  // The cellular device may not be exposed immediately upon startup or login.
  // We keep track of whether it is exposed or not and will only start the timer
  // if it is exposed.
  bool has_cellular_device_{false};
  // Whether we have seen an activated cellular network is cached to simplify
  // the timer logic used in this class.
  bool has_activated_cellular_network_{false};
  std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_CELLULAR_SETUP_NOTIFIER_H_
