// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_MODEL_H_
#define ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_MODEL_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"

namespace ash {

class VpnList;

// TrayNetworkStateModel observes the mojo interface and tracks the devices
// and active networks. It has UI observers that are informed when important
// changes occur.
class ASH_EXPORT TrayNetworkStateModel {
 public:
  TrayNetworkStateModel();

  TrayNetworkStateModel(const TrayNetworkStateModel&) = delete;
  TrayNetworkStateModel& operator=(const TrayNetworkStateModel&) = delete;

  ~TrayNetworkStateModel();

  void AddObserver(TrayNetworkStateObserver* observer);
  void RemoveObserver(TrayNetworkStateObserver* observer);

  // Returns DeviceStateProperties for |type| if it exists or null.
  const chromeos::network_config::mojom::DeviceStateProperties* GetDevice(
      chromeos::network_config::mojom::NetworkType type) const;

  // Returns the DeviceStateType for |type| if a device exists or kUnavailable.
  chromeos::network_config::mojom::DeviceStateType GetDeviceState(
      chromeos::network_config::mojom::NetworkType type) const;

  // Convenience method to call the |remote_cros_network_config_| method.
  void SetNetworkTypeEnabledState(
      chromeos::network_config::mojom::NetworkType type,
      bool enabled);

  // Returns true if built-in VPN is prohibited.
  // Note: Currently only built-in VPNs can be prohibited by policy.
  bool IsBuiltinVpnProhibited() const;

  // This used to be inlined but now requires details from the Impl class.
  chromeos::network_config::mojom::CrosNetworkConfig* cros_network_config();

  void ConfigureRemoteForTesting(
      mojo::PendingRemote<chromeos::network_config::mojom::CrosNetworkConfig>
          cros_network_config);

  const chromeos::network_config::mojom::NetworkStateProperties*
  default_network() const {
    return default_network_.get();
  }
  const chromeos::network_config::mojom::NetworkStateProperties*
  active_non_cellular() const {
    return active_non_cellular_.get();
  }
  const chromeos::network_config::mojom::NetworkStateProperties*
  active_cellular() const {
    return active_cellular_.get();
  }
  const chromeos::network_config::mojom::NetworkStateProperties* active_vpn()
      const {
    return active_vpn_.get();
  }
  bool has_vpn() const { return has_vpn_; }
  VpnList* vpn_list() { return vpn_list_.get(); }
  const chromeos::network_config::mojom::GlobalPolicy* global_policy() {
    return global_policy_.get();
  }

 private:
  friend class VPNFeaturePodControllerTest;

  void OnGetDeviceStateList(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
          devices);

  void UpdateActiveNetworks(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  void OnGetVirtualNetworks(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  void OnGetGlobalPolicy(
      chromeos::network_config::mojom::GlobalPolicyPtr global_policy);

  void NotifyNetworkListChanged();
  void NotifyGlobalPolicyChanged();
  void NotifyVpnProvidersChanged();
  void SendActiveNetworkStateChanged();
  void SendNetworkListChanged();
  void SendDeviceStateListChanged();

  class Impl;
  std::unique_ptr<Impl> impl_;

  base::ObserverList<TrayNetworkStateObserver> observer_list_;

  // Frequency at which to push NetworkListChanged updates. This avoids
  // unnecessarily frequent UI updates (which can be expensive). We set this
  // to 0 for tests to eliminate timing variance.
  int update_frequency_;

  // Timer used to limit the frequency of NetworkListChanged updates.
  base::OneShotTimer timer_;

  base::flat_map<chromeos::network_config::mojom::NetworkType,
                 chromeos::network_config::mojom::DeviceStatePropertiesPtr>
      devices_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr default_network_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr
      active_non_cellular_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr active_cellular_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr active_vpn_;
  chromeos::network_config::mojom::GlobalPolicyPtr global_policy_;
  bool has_vpn_ = false;
  std::unique_ptr<VpnList> vpn_list_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_MODEL_H_
