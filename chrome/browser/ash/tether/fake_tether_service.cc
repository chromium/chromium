// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/tether/fake_tether_service.h"

#include "base/strings/string_number_conversions.h"

namespace ash {
namespace tether {

namespace {

constexpr char kTetherGuidPrefix[] = "tether-guid-";
constexpr char kTetherNamePrefix[] = "tether";
constexpr char kCarrier[] = "FakeCarrier";

}  // namespace

FakeTetherService::FakeTetherService(
    Profile* profile,
    chromeos::PowerManagerClient* power_manager_client,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    session_manager::SessionManager* session_manager)
    : TetherService(profile,
                    power_manager_client,
                    device_sync_client,
                    secure_channel_client,
                    multidevice_setup_client,
                    session_manager) {}

void FakeTetherService::StartTetherIfPossible() {
  if (GetTetherTechnologyState() !=
      NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED) {
    return;
  }

  for (int i = 0; i < num_tether_networks_; ++i) {
    network_state_handler()->AddTetherNetworkState(
        kTetherGuidPrefix + base::NumberToString(i),
        kTetherNamePrefix + base::NumberToString(i), kCarrier,
        100 /* battery_percentage */, 100 /* signal_strength */,
        false /* has_connected_to_host */);
  }
}

void FakeTetherService::StopTetherIfNecessary() {
  for (int i = 0; i < num_tether_networks_; ++i) {
    network_state_handler()->RemoveTetherNetworkState(kTetherGuidPrefix + i);
  }
}

bool FakeTetherService::HasSyncedTetherHosts() const {
  return true;
}

}  // namespace tether
}  // namespace ash
