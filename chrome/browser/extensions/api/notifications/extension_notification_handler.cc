// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notifications/extension_notification_handler.h"

#include "base/callback.h"
#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper_factory.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/notifications.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/constants.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace extensions {

namespace notifications = api::notifications;

namespace {

std::unique_ptr<base::ListValue> CreateBaseEventArgs(
    const std::string& extension_id,
    const std::string& scoped_notification_id) {
  // Unscope the notification id before returning it.
  size_t index_of_separator = extension_id.length() + 1;
  DCHECK_LT(index_of_separator, scoped_notification_id.length());
  std::string unscoped_notification_id =
      scoped_notification_id.substr(index_of_separator);

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->AppendString(unscoped_notification_id);
  return args;
}

}  // namespace

ExtensionNotificationHandler::ExtensionNotificationHandler() = default;

ExtensionNotificationHandler::~ExtensionNotificationHandler() = default;

// static
std::string ExtensionNotificationHandler::GetExtensionId(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIs(extensions::kExtensionScheme))
    return "";
  return url.GetOrigin().host_piece().as_string();
}

void ExtensionNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  EventRouter::UserGestureState gesture =
      by_user ? EventRouter::USER_GESTURE_ENABLED
              : EventRouter::USER_GESTURE_NOT_ENABLED;
  std::string extension_id(GetExtensionId(GURL(origin)));
  DCHECK(!extension_id.empty());

  std::unique_ptr<base::ListValue> args(
      CreateBaseEventArgs(extension_id, notification_id));
  args->AppendBoolean(by_user);
  SendEvent(profile, extension_id, events::NOTIFICATIONS_ON_CLOSED,
            api::notifications::OnClosed::kEventName, gesture, std::move(args));

  ExtensionNotificationDisplayHelper* display_helper =
      ExtensionNotificationDisplayHelperFactory::GetForProfile(profile);
  if (display_helper)
    display_helper->EraseDataForNotificationId(notification_id);

  std::move(completed_closure).Run();
}

void ExtensionNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  DCHECK(!reply.has_value());

  std::string extension_id(GetExtensionId(GURL(origin)));
  std::unique_ptr<base::ListValue> args(
      CreateBaseEventArgs(extension_id, notification_id));
  if (action_index.has_value())
    args->AppendInteger(action_index.value());
  events::HistogramValue histogram_value =
      action_index.has_value() ? events::NOTIFICATIONS_ON_BUTTON_CLICKED
                               : events::NOTIFICATIONS_ON_CLICKED;
  const std::string& event_name =
      action_index.has_value() ? api::notifications::OnButtonClicked::kEventName
                               : api::notifications::OnClicked::kEventName;

  SendEvent(profile, extension_id, histogram_value, event_name,
            EventRouter::USER_GESTURE_ENABLED, std::move(args));

  std::move(completed_closure).Run();
}

void ExtensionNotificationHandler::DisableNotifications(Profile* profile,
                                                        const GURL& origin) {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, origin.host());
  NotifierStateTrackerFactory::GetForProfile(profile)->SetNotifierEnabled(
      notifier_id, false /* enabled */);
}

void ExtensionNotificationHandler::SendEvent(
    Profile* profile,
    const std::string& extension_id,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    EventRouter::UserGestureState user_gesture,
    std::unique_ptr<base::ListValue> args) {
  if (extension_id.empty())
    return;

  EventRouter* event_router = EventRouter::Get(profile);
  if (!event_router)
    return;

  std::unique_ptr<Event> event(
      new Event(histogram_value, event_name, std::move(args)));
  event->user_gesture = user_gesture;
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

}  // namespace extensions
