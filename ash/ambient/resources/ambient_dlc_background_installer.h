// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_RESOURCES_AMBIENT_DLC_BACKGROUND_INSTALLER_H_
#define ASH_AMBIENT_RESOURCES_AMBIENT_DLC_BACKGROUND_INSTALLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// Performs the ambient dlc background install immediately after network
// connectivity is up. For more information see
// `ash::InstallAmbientVideoDlcInBackground()`. The install happens only one
// time in `AmbientBackgroundDlcInstaller`'s lifetime.
class ASH_EXPORT AmbientBackgroundDlcInstaller
    : public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  AmbientBackgroundDlcInstaller();
  AmbientBackgroundDlcInstaller(const AmbientBackgroundDlcInstaller&) = delete;
  AmbientBackgroundDlcInstaller& operator=(
      const AmbientBackgroundDlcInstaller&) = delete;
  ~AmbientBackgroundDlcInstaller() override;

 private:
  // CrosNetworkConfigObserver:
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override;

  void TryExecuteInstallForAllActiveNetworks(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          active_networks);
  void TryExecuteInstall(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network);

  bool has_executed_install_ = false;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_{this};
  base::WeakPtrFactory<AmbientBackgroundDlcInstaller> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_RESOURCES_AMBIENT_DLC_BACKGROUND_INSTALLER_H_
