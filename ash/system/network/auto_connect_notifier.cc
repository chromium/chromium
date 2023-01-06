// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/auto_connect_notifier.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Timeout used for connecting to a managed network. When an auto-connection is
// initiated, we expect the connection to occur within this amount of time. If
// a timeout occurs, we assume that no auto-connection occurred and do not show
// a notification.
constexpr const base::TimeDelta kNetworkConnectionTimeout = base::Seconds(3);

void ShowToast(std::string id,
               ToastCatalogName catalog_name,
               const std::u16string& text) {
  ash::ToastManager::Get()->Show(ToastData(id, catalog_name, text));
}

}  // namespace

// static
const char AutoConnectNotifier::kAutoConnectToastId[] =
    "cros_auto_connect_notifier_ids.connected_to_network";

AutoConnectNotifier::AutoConnectNotifier()
    : timer_(std::make_unique<base::OneShotTimer>()) {
  // NetworkHandler may not be initialized in tests.
  if (NetworkHandler::IsInitialized()) {
    auto* network_handler = NetworkHandler::Get();
    network_handler->network_connection_handler()->AddObserver(this);
    network_handler->network_state_handler()->AddObserver(this, FROM_HERE);
    // AutoConnectHandler may not be initialized in tests with NetworkHandler.
    if (network_handler->auto_connect_handler())
      network_handler->auto_connect_handler()->AddObserver(this);
  }
}

AutoConnectNotifier::~AutoConnectNotifier() {
  // NetworkHandler may not be initialized in tests.
  if (NetworkHandler::IsInitialized()) {
    auto* network_handler = NetworkHandler::Get();
    // AutoConnectHandler may not be initialized in tests with NetworkHandler.
    if (network_handler->auto_connect_handler())
      network_handler->auto_connect_handler()->RemoveObserver(this);
    network_handler->network_state_handler()->RemoveObserver(this, FROM_HERE);
    network_handler->network_connection_handler()->RemoveObserver(this);
  }
}

void AutoConnectNotifier::ConnectToNetworkRequested(
    const std::string& /* service_path */) {
  has_user_explicitly_requested_connection_ = true;
}

void AutoConnectNotifier::NetworkConnectionStateChanged(
    const NetworkState* network) {
  // Ignore non WiFi networks completely.
  if (!network->Matches(NetworkTypePattern::WiFi()))
    return;

  // The notification is only shown when a connection has succeeded; if
  // |network| is not connected, there is nothing to do.
  if (!network->IsConnectedState()) {
    // Clear the tracked network if it is no longer connected or connecting.
    if (!network->IsConnectingState() &&
        network->guid() == connected_network_guid_) {
      connected_network_guid_.clear();
    }
    return;
  }

  // No notification should be shown unless an auto-connection is underway.
  if (!timer_->IsRunning()) {
    // Track the currently connected network.
    connected_network_guid_ = network->guid();
    return;
  }

  // Ignore NetworkConnectionStateChanged for a previously connected network.
  if (network->guid() == connected_network_guid_)
    return;

  // An auto-connected network has connected successfully. Display a
  // notification alerting the user that this has occurred.
  DisplayToast(network);
  has_user_explicitly_requested_connection_ = false;
}

void AutoConnectNotifier::OnAutoConnectedInitiated(int auto_connect_reasons) {
  // If the user has not explicitly requested a connection to another network,
  // the notification does not need to be shown.
  if (!has_user_explicitly_requested_connection_)
    return;

  // The notification should only be shown if a network is joined due to a
  // policy or certificate. Other reasons (e.g., joining a network due to login)
  // do not require that a notification be shown.
  const int kManagedNetworkReasonsBitmask =
      AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED |
      AutoConnectHandler::AUTO_CONNECT_REASON_CERTIFICATE_RESOLVED;
  if (!(auto_connect_reasons & kManagedNetworkReasonsBitmask))
    return;

  // If a potential connection is already underway, reset the timeout and
  // continue waiting.
  if (timer_->IsRunning()) {
    timer_->Reset();
    return;
  }

  // Auto-connection has been requested, so start a timer. If a network connects
  // successfully before the timer expires, auto-connection has succeeded, so a
  // notification should be shown. If no connection occurs before the timer
  // fires, we assume that auto-connect attempted to search for networks to
  // join but did not succeed in joining one (in that case, no notification
  // should be shown).
  timer_->Start(FROM_HERE, kNetworkConnectionTimeout, base::DoNothing());
}

void AutoConnectNotifier::DisplayToast(const NetworkState* network) {
  NET_LOG(EVENT) << "Show AutoConnect Toast for: " << NetworkId(network);
  // Remove previous toast if one was already being shown.
  ash::ToastManager::Get()->Cancel(kAutoConnectToastId);
  ShowToast(kAutoConnectToastId, ToastCatalogName::kNetworkAutoConnect,
            l10n_util::GetStringUTF16(IDS_ASH_NETWORK_AUTOCONNECT));
}

}  // namespace ash
