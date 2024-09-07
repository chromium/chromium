// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_MANAGED_SIM_LOCK_NOTIFIER_H_
#define ASH_SYSTEM_NETWORK_MANAGED_SIM_LOCK_NOTIFIER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

// Notifies the user to unlock the currently active PIN locked SIM if the
// restrict cellular SIM lock Global Network Configuration is set to true.
class ASH_EXPORT ManagedSimLockNotifier
    : public SessionObserver,
      public chromeos::network_config::CrosNetworkConfigObserver,
      public message_center::NotificationObserver {
 public:
  static const char kManagedSimLockNotificationId[];

  ManagedSimLockNotifier();
  ManagedSimLockNotifier(const ManagedSimLockNotifier&) = delete;
  ManagedSimLockNotifier& operator=(const ManagedSimLockNotifier&) = delete;
  ~ManagedSimLockNotifier() override;

 private:
  friend class ManagedSimLockNotifierTest;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

  // CrosNetworkConfigObserver:
  void OnDeviceStateListChanged() override;
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

  std::string primary_iccid_ = std::string();
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};

  base::WeakPtrFactory<ManagedSimLockNotifier> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_MANAGED_SIM_LOCK_NOTIFIER_H_
