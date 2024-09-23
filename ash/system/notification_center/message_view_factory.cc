// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/message_view_factory.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/notification_center/views/conversation_notification_view.h"
#include "ash/system/notification_center/views/ongoing_process_view.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "build/chromeos_buildflags.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {

using MessageViewCustomFactoryMap =
    std::map<std::string, MessageViewFactory::CustomMessageViewFactoryFunction>;

base::LazyInstance<MessageViewCustomFactoryMap>::Leaky g_custom_view_factories =
    LAZY_INSTANCE_INITIALIZER;

std::unique_ptr<message_center::MessageView> GetCustomNotificationView(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  MessageViewCustomFactoryMap* factories = g_custom_view_factories.Pointer();
  auto iter = factories->find(notification.custom_view_type());
  DCHECK(iter != factories->end());
  return iter->second.Run(notification, shown_in_popup);
}

}  // namespace

// static
std::unique_ptr<message_center::MessageView> MessageViewFactory::Create(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  switch (notification.type()) {
    case message_center::DEPRECATED_NOTIFICATION_TYPE_BASE_FORMAT:
    case message_center::NOTIFICATION_TYPE_IMAGE:
    case message_center::NOTIFICATION_TYPE_MULTIPLE:
    case message_center::NOTIFICATION_TYPE_SIMPLE:
    case message_center::NOTIFICATION_TYPE_PROGRESS:
      // Rely on default construction after the switch.
      break;
    case message_center::NOTIFICATION_TYPE_CUSTOM:
      return GetCustomNotificationView(notification, shown_in_popup);
    case message_center::NOTIFICATION_TYPE_CONVERSATION:
      return std::make_unique<ConversationNotificationView>(notification);
    default:
      // If the caller asks for an unrecognized kind of view (entirely possible
      // if an application is running on an older version of this code that
      // doesn't have the requested kind of notification template), we'll fall
      // back to a notification instance that will provide at least basic
      // functionality.
      LOG(WARNING) << "Unable to fulfill request for unrecognized or"
                   << "unsupported notification type " << notification.type()
                   << ". Falling back to simple notification type.";
      break;
  }

  // Only pinned system notifications use `OngoingProcessView`.
  if (features::AreOngoingProcessesEnabled() && notification.pinned() &&
      notification.notifier_id().type ==
          message_center::NotifierType::SYSTEM_COMPONENT) {
    return std::make_unique<OngoingProcessView>(notification);
  }

  return std::make_unique<AshNotificationView>(notification, shown_in_popup);
}

// static
void MessageViewFactory::SetCustomNotificationViewFactory(
    const std::string& custom_view_type,
    const CustomMessageViewFactoryFunction& factory_function) {
  MessageViewCustomFactoryMap* factories = g_custom_view_factories.Pointer();
  DCHECK(factories->find(custom_view_type) == factories->end());
  factories->emplace(custom_view_type, factory_function);
}

// static
bool MessageViewFactory::HasCustomNotificationViewFactory(
    const std::string& custom_view_type) {
  MessageViewCustomFactoryMap* factories = g_custom_view_factories.Pointer();
  return factories->find(custom_view_type) != factories->end();
}

// static
void MessageViewFactory::ClearCustomNotificationViewFactory(
    const std::string& custom_view_type) {
  g_custom_view_factories.Get().erase(custom_view_type);
}

}  // namespace ash
