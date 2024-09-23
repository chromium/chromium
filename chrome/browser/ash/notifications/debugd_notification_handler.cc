// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/debugd_notification_handler.h"

#include <optional>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

using ::message_center::MessageCenter;
using ::message_center::Notification;

constexpr char kPacketCaptureNotificationId[] = "debugd-packetcapture";
constexpr char kNotifierPacketCapture[] = "ash.debugd-packetcapture";

DebugdNotificationHandler::DebugdNotificationHandler(
    DebugDaemonClient* debug_daemon_client)
    : debug_daemon_client_(debug_daemon_client) {
  debug_daemon_client_->AddObserver(this);
}

DebugdNotificationHandler::~DebugdNotificationHandler() {
  debug_daemon_client_->RemoveObserver(this);
}

// Show the notification when the packet capture is started. If the notification
// for a previous ongoing packet capture is active, nothing will be done and the
// existing notification will be kept there as the notification id is the same
// for all packet capture notifications.
void DebugdNotificationHandler::OnPacketCaptureStarted() {
  MessageCenter::Get()->AddNotification(CreateNotification());
}

void DebugdNotificationHandler::OnPacketCaptureStopped() {
  CloseNotification();
}

std::unique_ptr<Notification> DebugdNotificationHandler::CreateNotification() {
  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_DEBUG_STOP_PACKET_CAPTURE_BUTTON)));

  message_center::HandleNotificationClickDelegate::ButtonClickCallback
      callback_wrapper =
          base::BindRepeating(&DebugdNotificationHandler::OnButtonClick,
                              weak_ptr_factory_.GetWeakPtr());

  std::unique_ptr<Notification> notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, kPacketCaptureNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_DEBUG_PACKET_CAPTURE_STARTED),
      /*message=*/std::u16string(),
      /*display_source=*/std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierPacketCapture,
                                 NotificationCatalogName::kPacketCapture),
      optional_fields,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          callback_wrapper),
      kSystemMenuInfoIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  notification->set_pinned(true);
  return notification;
}

void DebugdNotificationHandler::CloseNotification() {
  if (MessageCenter::Get()->FindVisibleNotificationById(
          kPacketCaptureNotificationId)) {
    MessageCenter::Get()->RemoveNotification(kPacketCaptureNotificationId,
                                             false);
  }
}

void DebugdNotificationHandler::OnButtonClick(std::optional<int> button_index) {
  // Do nothing if the notification body is clicked, not the button.
  if (!button_index)
    return;

  // button_index should be 0 since there's only one button on the notification.
  DCHECK(button_index == 0);

  // Send empty argument to StopPacketCapture function to stop all on-going
  // packet capture operations.
  debug_daemon_client_->StopPacketCapture(std::string());
}

}  // namespace ash
