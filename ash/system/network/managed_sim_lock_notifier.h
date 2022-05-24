// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_MANAGED_SIM_LOCK_NOTIFIER_H_
#define ASH_SYSTEM_NETWORK_MANAGED_SIM_LOCK_NOTIFIER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// Notifies the user to unlock the currently active PIN locked SIM if the
// restrict cellular SIM lock Global Network Configuration is set to true.
class ASH_EXPORT ManagedSimLockNotifier
    : public SessionObserver,
      public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
  ManagedSimLockNotifier();
  ManagedSimLockNotifier(const ManagedSimLockNotifier&) = delete;
  ManagedSimLockNotifier& operator=(const ManagedSimLockNotifier&) = delete;
  ~ManagedSimLockNotifier() override;

 private:
  friend class ManagedSimLockNotifierTest;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // CrosNetworkConfigObserver:
  void OnNetworkStateListChanged() override {}
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override {}
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override {}
  void OnDeviceStateListChanged() override;
  void OnVpnProvidersChanged() override {}
  void OnNetworkCertificatesChanged() override {}
  void OnPoliciesApplied(const std::string& userhash) override;

  void OnGetDeviceStateList(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
          devices);
  void OnCellularNetworksList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  void OnGetGlobalPolicy(
      chromeos::network_config::mojom::GlobalPolicyPtr global_policy);

  void RemoveNotification();
  void CheckGlobalNetworkConfiguration();
  void MaybeShowNotification();
  void ShowNotification();

  static const char kManagedSimLockNotificationId[];

  std::string primary_iccid_ = std::string();
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};

  base::WeakPtrFactory<ManagedSimLockNotifier> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_MANAGED_SIM_LOCK_NOTIFIER_H_
