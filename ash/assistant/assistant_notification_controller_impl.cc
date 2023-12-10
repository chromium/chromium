// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_notification_controller_impl.h"

#include <memory>
#include <utility>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/assistant_notification_expiry_monitor.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kNotifierId[] = "assistant";

// Helpers ---------------------------------------------------------------------

std::unique_ptr<message_center::Notification> CreateSystemNotification(
    const message_center::NotifierId& notifier_id,
    const assistant::AssistantNotification& notification) {
  const std::u16string title = base::UTF8ToUTF16(notification.title);
  const std::u16string message = base::UTF8ToUTF16(notification.message);
  const std::u16string display_source =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_NOTIFICATION_DISPLAY_SOURCE);

  message_center::RichNotificationData data;
  for (const auto& button : notification.buttons)
    data.buttons.emplace_back(base::UTF8ToUTF16(button.label));

  std::unique_ptr<message_center::Notification> system_notification =
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification.client_id,
          title, message, display_source, GURL(), notifier_id, data,
          /*delegate=*/nullptr, chromeos::kNotificationAssistantIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  system_notification->set_renotify(notification.renotify);
  system_notification->set_pinned(notification.is_pinned);

  switch (notification.priority) {
    case assistant::AssistantNotificationPriority::kLow:
      system_notification->set_priority(message_center::LOW_PRIORITY);
      break;
    case assistant::AssistantNotificationPriority::kDefault:
      system_notification->set_priority(message_center::DEFAULT_PRIORITY);
      break;
    case assistant::AssistantNotificationPriority::kHigh:
      system_notification->set_priority(message_center::HIGH_PRIORITY);
      break;
  }

  return system_notification;
}

message_center::NotifierId GetNotifierId() {
  return message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
      NotificationCatalogName::kAssistantNotification);
}

bool IsValidActionUrl(const GURL& action_url) {
  return action_url.is_valid() && (action_url.SchemeIsHTTPOrHTTPS() ||
                                   assistant::util::IsDeepLinkUrl(action_url));
}

}  // namespace

// AssistantNotificationControllerImpl
// ---------------------------------------------

AssistantNotificationControllerImpl::AssistantNotificationControllerImpl()
    : expiry_monitor_(this), notifier_id_(GetNotifierId()) {
  model_.AddObserver(this);
  message_center::MessageCenter::Get()->AddObserver(this);
}

AssistantNotificationControllerImpl::~AssistantNotificationControllerImpl() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
  model_.RemoveObserver(this);
}

void AssistantNotificationControllerImpl::SetAssistant(
    assistant::Assistant* assistant) {
  receiver_.reset();

  assistant_ = assistant;

  if (assistant)
    receiver_.Bind(assistant_->GetPendingNotificationDelegate());
}

// AssistantNotificationController --------------------------------------

void AssistantNotificationControllerImpl::RemoveNotificationById(
    const std::string& id,
    bool from_server) {
  model_.RemoveNotificationById(id, from_server);
}

void AssistantNotificationControllerImpl::SetQuietMode(bool enabled) {
  message_center::MessageCenter::Get()->SetQuietMode(enabled);
}

// NotificationDelegate ------------------------------------------------------

void AssistantNotificationControllerImpl::AddOrUpdateNotification(
    AssistantNotification notification) {
  model_.AddOrUpdateNotification(std::move(notification));
}

void AssistantNotificationControllerImpl::RemoveNotificationByGroupingKey(
    const std::string& grouping_key,
    bool from_server) {
  model_.RemoveNotificationsByGroupingKey(grouping_key, from_server);
}

void AssistantNotificationControllerImpl::RemoveAllNotifications(
    bool from_server) {
  model_.RemoveAllNotifications(from_server);
}

// AssistantNotificationModelObserver ------------------------------------------

void AssistantNotificationControllerImpl::OnNotificationAdded(
    const AssistantNotification& notification) {
  // Do not show system notifications if the setting is disabled.
  if (!AssistantState::Get()->notification_enabled().value_or(true))
    return;

  message_center::MessageCenter::Get()->AddNotification(
      CreateSystemNotification(notifier_id_, notification));
}

void AssistantNotificationControllerImpl::OnNotificationUpdated(
    const AssistantNotification& notification) {
  // Do not show system notifications if the setting is disabled.
  if (!AssistantState::Get()->notification_enabled().value_or(true))
    return;

  message_center::MessageCenter::Get()->UpdateNotification(
      notification.client_id,
      CreateSystemNotification(notifier_id_, notification));
}

void AssistantNotificationControllerImpl::OnNotificationRemoved(
    const AssistantNotification& notification,
    bool from_server) {
  // Remove the notification from the message center.
  message_center::MessageCenter::Get()->RemoveNotification(
      notification.client_id, /*by_user=*/false);

  // Dismiss the notification on the server to sync across devices.
  if (!from_server && AssistantState::Get()->assistant_status() ==
                          assistant::AssistantStatus::READY) {
    assistant_->DismissNotification(notification);
  }
}

void AssistantNotificationControllerImpl::OnAllNotificationsRemoved(
    bool from_server) {
  message_center::MessageCenter::Get()->RemoveNotificationsForNotifierId(
      notifier_id_);
}

// message_center::MessageCenterObserver ---------------------------------------

void AssistantNotificationControllerImpl::OnNotificationClicked(
    const std::string& id,
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  const AssistantNotification* notification = model_.GetNotificationById(id);
  if (!notification)
    return;

  const auto& action_url =
      button_index.has_value()
          ? notification->buttons[button_index.value()].action_url
          : notification->action_url;

  // Open the action url if it is valid.
  if (IsValidActionUrl(action_url)) {
    // NOTE: We copy construct a new GURL as our |notification| may be destroyed
    // during the OpenUrl() sequence leaving |action_url| in a bad state.
    AssistantController::Get()->OpenUrl(GURL(action_url));

    const bool remove_notification =
        button_index.has_value() ? notification->buttons[button_index.value()]
                                       .remove_notification_on_click
                                 : notification->remove_on_click;

    if (remove_notification)
      model_.RemoveNotificationById(id, /*from_server=*/false);
    return;
  }

  if (!notification->from_server)
    return;

  // If the notification is from the server, we retrieve the notification
  // payload using the following indexing scheme:
  //
  // Index:  |    [0]    |   [1]    |   [2]    | ...
  // -------------------------------------------------
  // Action: | Top Level | Button 1 | Button 2 | ...
  const int action_index = button_index.value_or(-1) + 1;
  if (AssistantState::Get()->assistant_status() ==
      assistant::AssistantStatus::READY) {
    assistant_->RetrieveNotification(*notification, action_index);
  }
}

void AssistantNotificationControllerImpl::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  // Update our notification model to remain in sync w/ Message Center.
  if (model_.GetNotificationById(notification_id))
    model_.RemoveNotificationById(notification_id, /*from_server=*/false);
}

}  // namespace ash
