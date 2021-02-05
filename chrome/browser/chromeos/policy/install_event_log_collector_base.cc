// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/install_event_log_collector_base.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"

namespace policy {

InstallEventLogCollectorBase::InstallEventLogCollectorBase(Profile* profile)
    : online_(GetOnlineState()), profile_(profile) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
}

InstallEventLogCollectorBase::~InstallEventLogCollectorBase() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

bool InstallEventLogCollectorBase::GetOnlineState() {
  chromeos::NetworkStateHandler::NetworkStateList network_state_list;
  chromeos::NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkListByType(
          chromeos::NetworkTypePattern::Default(), true /* configured_only */,
          false /* visible_only */, 0 /* limit */, &network_state_list);

  for (const chromeos::NetworkState* network_state : network_state_list) {
    if (network_state->connection_state() == shill::kStateOnline) {
      return true;
    }
  }
  return false;
}

void InstallEventLogCollectorBase::OnLogin() {
  // Don't log in case session is restared or recovered from crash.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kLoginUser) ||
      profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    return;
  }

  online_ = GetOnlineState();
  OnLoginInternal();
}

void InstallEventLogCollectorBase::OnLogout() {
  // Don't log in case session is restared.
  if (g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted))
    return;

  OnLogoutInternal();
}

void InstallEventLogCollectorBase::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  const bool currently_online = GetOnlineState();
  if (currently_online == online_) {
    return;
  }
  online_ = currently_online;

  OnConnectionStateChanged(type);
}
}  // namespace policy
