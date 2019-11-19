// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/auto_connect_notifier.h"

#include "ash/public/cpp/network_icon_image_source.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

using chromeos::NetworkHandler;

namespace ash {

namespace {

// Timeout used for connecting to a managed network. When an auto-connection is
// initiated, we expect the connection to occur within this amount of time. If
// a timeout occurs, we assume that no auto-connection occurred and do not show
// a notification.
constexpr const base::TimeDelta kNetworkConnectionTimeout =
    base::TimeDelta::FromSeconds(3);

const char kNotifierAutoConnect[] = "ash.auto-connect";

}  // namespace

// static
const char AutoConnectNotifier::kAutoConnectNotificationId[] =
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
    const chromeos::NetworkState* network) {
  // Ignore non WiFi networks completely.
  if (!network->Matches(chromeos::NetworkTypePattern::WiFi()))
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
  DisplayNotification(network);
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
      chromeos::AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED |
      chromeos::AutoConnectHandler::AUTO_CONNECT_REASON_CERTIFICATE_RESOLVED;
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

void AutoConnectNotifier::DisplayNotification(
    const chromeos::NetworkState* network) {
  NET_LOG(EVENT) << "Show AutoConnect Notification for: " << network->name();
  auto notification = ash::CreateSystemNotification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kAutoConnectNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_NETWORK_AUTOCONNECT_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(
          IDS_ASH_NETWORK_AUTOCONNECT_NOTIFICATION_MESSAGE),
      base::string16() /* display_source */, GURL() /* origin_url */,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierAutoConnect),
      {} /* optional_fields */,
      base::MakeRefCounted<message_center::NotificationDelegate>(),
      gfx::VectorIcon() /* small_image */,
      message_center::SystemNotificationWarningLevel::NORMAL);

  notification->set_small_image(gfx::Image(network_icon::GetImageForWifiNetwork(
      notification->accent_color(),
      gfx::Size(message_center::kSmallImageSizeMD,
                message_center::kSmallImageSizeMD))));

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (message_center->FindVisibleNotificationById(kAutoConnectNotificationId))
    message_center->RemoveNotification(kAutoConnectNotificationId, false);
  message_center->AddNotification(std::move(notification));
}

}  // namespace ash
