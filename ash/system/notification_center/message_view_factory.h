// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_VIEW_FACTORY_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_VIEW_FACTORY_H_

#include "ash/ash_export.h"

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

namespace message_center {
class MessageView;
class Notification;
}  // namespace message_center

namespace ash {

// Creates appropriate MessageViews for notifications depending on the
// notification type.
class ASH_EXPORT MessageViewFactory {
 public:
  // A function that creates MessageView for a NOTIFICATION_TYPE_CUSTOM
  // notification.
  using CustomMessageViewFactoryFunction =
      base::RepeatingCallback<std::unique_ptr<message_center::MessageView>(
          const message_center::Notification&,
          bool shown_in_popup)>;

  // Create appropriate MessageViews based on the given notification.
  static std::unique_ptr<message_center::MessageView> Create(
      const message_center::Notification& notification,
      bool shown_in_popup);

  // Sets the function that will be invoked to create a custom notification view
  // for a specific |custom_view_type|. This should be a repeating callback.
  // It's an error to attempt to show a custom notification without first having
  // called this function. The |custom_view_type| on the notification should
  // also match the type used here.
  static void SetCustomNotificationViewFactory(
      const std::string& custom_view_type,
      const CustomMessageViewFactoryFunction& factory_function);
  static bool HasCustomNotificationViewFactory(
      const std::string& custom_view_type);
  static void ClearCustomNotificationViewFactory(
      const std::string& custom_view_type);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_VIEW_FACTORY_H_
