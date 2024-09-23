// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/resources/ambient_dlc_background_installer.h"

#include "ash/ambient/util/time_of_day_utils.h"
#include "ash/public/cpp/network_config_service.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace ash {

AmbientBackgroundDlcInstaller::AmbientBackgroundDlcInstaller() {
  GetNetworkConfigService(cros_network_config_.BindNewPipeAndPassReceiver());
  cros_network_config_->AddObserver(
      cros_network_config_observer_.BindNewPipeAndPassRemote());
  cros_network_config_->GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilter::New(
          chromeos::network_config::mojom::FilterType::kActive,
          chromeos::network_config::mojom::NetworkType::kAll,
          chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(
          &AmbientBackgroundDlcInstaller::TryExecuteInstallForAllActiveNetworks,
          weak_factory_.GetWeakPtr()));
}

AmbientBackgroundDlcInstaller::~AmbientBackgroundDlcInstaller() = default;

// TODO(b/220202911): After the linked bug is complete:
// 1) Do the background install at a random delay after the user has logged in
//    and the user has enabled ambient mode.
// 2) Uninstall the DLC if the user disables ambient mode.
// 3) Wait for non-metered/cellular network availability (usually wifi) before
//    performing the background install. This can be done by modifying the
//    `chromeos::network_config::mojom::NetworkFilter` we pass to the
//    `cros_network_config_`.
// The above is currently not done (even though it saves the most disc space)
// because there is currently a corner case where the dlc install can repeatedly
// fail if the user needs to reboot for a pending OS update. Installing the dlc
// immediately after network connectivity reduces the odds of the user hitting
// this case for the time being.
void AmbientBackgroundDlcInstaller::OnNetworkStateChanged(
    chromeos::network_config::mojom::NetworkStatePropertiesPtr network) {
  TryExecuteInstall(std::move(network));
}

void AmbientBackgroundDlcInstaller::TryExecuteInstallForAllActiveNetworks(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        active_networks) {
  for (auto& network : active_networks) {
    TryExecuteInstall(std::move(network));
  }
}

void AmbientBackgroundDlcInstaller::TryExecuteInstall(
    chromeos::network_config::mojom::NetworkStatePropertiesPtr network) {
  if (has_executed_install_ || !network ||
      network->connection_state !=
          chromeos::network_config::mojom::ConnectionStateType::kOnline) {
    return;
  }

  has_executed_install_ = true;

  // Stop observing network changes.
  cros_network_config_observer_.reset();

  // Dbus calls for dlc installations must be performed on the main thread or
  // a crash occurs. Post a dummy `BEST_EFFORT` task as a signal that resources
  // are ready for background tasks, and then run the install back on the main
  // thread.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT}, base::DoNothing(),
      base::BindOnce(&InstallAmbientVideoDlcInBackground));
}

}  // namespace ash
