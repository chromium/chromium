// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/stub_notification_display_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "url/origin.h"

// static
std::unique_ptr<KeyedService> StubNotificationDisplayService::FactoryForTests(
    content::BrowserContext* context) {
  return std::make_unique<StubNotificationDisplayService>(
      Profile::FromBrowserContext(context));
}

StubNotificationDisplayService::StubNotificationDisplayService(Profile* profile)
    : NotificationDisplayServiceImpl(profile), profile_(profile) {}

StubNotificationDisplayService::~StubNotificationDisplayService() = default;

void StubNotificationDisplayService::SetNotificationAddedClosure(
    base::RepeatingClosure closure) {
  notification_added_closure_ = std::move(closure);
}

void StubNotificationDisplayService::SetNotificationClosedClosure(
    base::RepeatingClosure closure) {
  notification_closed_closure_ = std::move(closure);
}

std::vector<message_center::Notification>
StubNotificationDisplayService::GetDisplayedNotificationsForType(
    NotificationHandler::Type type) const {
  std::vector<message_center::Notification> notifications;
  for (const auto& data : notifications_) {
    if (data.type != type)
      continue;

    notifications.push_back(data.notification);
  }

  return notifications;
}

std::optional<message_center::Notification>
StubNotificationDisplayService::GetNotification(
    const std::string& notification_id) {
  auto iter = base::ranges::find(
      notifications_, notification_id,
      [](const NotificationData& data) { return data.notification.id(); });

  if (iter == notifications_.end())
    return std::nullopt;

  return iter->notification;
}

const NotificationCommon::Metadata*
StubNotificationDisplayService::GetMetadataForNotification(
    const message_center::Notification& notification) {
  auto iter = base::ranges::find(
      notifications_, notification.id(),
      [](const NotificationData& data) { return data.notification.id(); });

  if (iter == notifications_.end())
    return nullptr;

  return iter->metadata.get();
}

void StubNotificationDisplayService::SimulateClick(
    NotificationHandler::Type notification_type,
    const std::string& notification_id,
    std::optional<int> action_index,
    std::optional<std::u16string> reply) {
  auto iter = FindNotification(notification_type, notification_id);
  if (iter == notifications_.end())
    return;

  NotificationHandler* handler = GetNotificationHandler(notification_type);
  if (notification_type == NotificationHandler::Type::TRANSIENT) {
    DCHECK(!handler);

    auto* delegate = iter->notification.delegate();
    if (delegate)
      delegate->Click(action_index, reply);
    return;
  }

  DCHECK(handler);
  base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
  handler->OnClick(profile_, iter->notification.origin_url(), notification_id,
                   action_index, reply, run_loop.QuitClosure());
  run_loop.Run();
}

void StubNotificationDisplayService::SimulateSettingsClick(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  auto iter = FindNotification(notification_type, notification_id);
  if (iter == notifications_.end())
    return;

  NotificationHandler* handler = GetNotificationHandler(notification_type);
  if (notification_type == NotificationHandler::Type::TRANSIENT) {
    DCHECK(!handler);
    if (iter->notification.delegate())
      iter->notification.delegate()->SettingsClick();
  } else {
    DCHECK(handler);
    handler->OpenSettings(profile_, iter->notification.origin_url());
  }
}

void StubNotificationDisplayService::RemoveNotification(
    NotificationHandler::Type notification_type,
    const std::string& notification_id,
    bool by_user,
    bool silent) {
  auto iter = FindNotification(notification_type, notification_id);
  if (iter == notifications_.end())
    return;
  NotificationData data = std::move(*iter);
  notifications_.erase(iter);

  if (!silent) {
    NotificationHandler* handler = GetNotificationHandler(notification_type);
    if (notification_type == NotificationHandler::Type::TRANSIENT) {
      DCHECK(!handler);
      if (data.notification.delegate())
        data.notification.delegate()->Close(by_user);
    } else {
      base::RunLoop run_loop;
      handler->OnClose(profile_, data.notification.origin_url(),
                       notification_id, by_user, run_loop.QuitClosure());
      run_loop.Run();
    }
  }
}

void StubNotificationDisplayService::RemoveAllNotifications(
    NotificationHandler::Type notification_type,
    bool by_user) {
  NotificationHandler* handler = GetNotificationHandler(notification_type);
  DCHECK_NE(!!handler,
            notification_type == NotificationHandler::Type::TRANSIENT);
  for (auto iter = notifications_.begin(); iter != notifications_.end();) {
    if (iter->type == notification_type) {
      NotificationData data = std::move(*iter);
      iter = notifications_.erase(iter);
      if (handler) {
        base::RunLoop run_loop;
        handler->OnClose(profile_, data.notification.origin_url(),
                         data.notification.id(), by_user,
                         run_loop.QuitClosure());
        run_loop.Run();
      } else if (data.notification.delegate()) {
        data.notification.delegate()->Close(by_user);
      }
    } else {
      iter++;
    }
  }
}

void StubNotificationDisplayService::SetProcessNotificationOperationDelegate(
    const ProcessNotificationOperationCallback& delegate) {
  process_notification_operation_delegate_ = delegate;
}

void StubNotificationDisplayService::Display(
    NotificationHandler::Type notification_type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  // This mimics notification replacement behaviour; the Close() method on a
  // notification's delegate is not meant to be invoked in this situation.
  RemoveNotification(notification_type, notification.id(), false /* by_user */,
                     true /* silent */);

  NotificationHandler* handler = GetNotificationHandler(notification_type);
  if (notification_type == NotificationHandler::Type::TRANSIENT) {
    CHECK(!handler);
    CHECK(notification.delegate());
  } else {
    handler->OnShow(profile_, notification.id());
  }

  notifications_.emplace_back(notification_type, notification,
                              std::move(metadata));

  if (notification_added_closure_)
    notification_added_closure_.Run();
}

void StubNotificationDisplayService::Close(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  // Close the notification silently only for non-transient notifications,
  // because some tests of transient (non-web/extension) notifications rely on
  // the close event being dispatched, e.g. tests in WebUsbDetectorTest.
  RemoveNotification(
      notification_type, notification_id, false /* by_user */,
      notification_type != NotificationHandler::Type::TRANSIENT /* silent */);

  if (notification_closed_closure_)
    notification_closed_closure_.Run();
}

void StubNotificationDisplayService::GetDisplayed(
    DisplayedNotificationsCallback callback) {
  std::set<std::string> notifications;

  for (const auto& notification_data : notifications_) {
    notifications.insert(notification_data.notification.id());
  }

  std::move(callback).Run(std::move(notifications),
                          true /* supports_synchronization */);
}

void StubNotificationDisplayService::GetDisplayedForOrigin(
    const GURL& origin,
    DisplayedNotificationsCallback callback) {
  std::set<std::string> notifications;

  for (const auto& notification_data : notifications_) {
    if (url::IsSameOriginWith(notification_data.notification.origin_url(),
                              origin)) {
      notifications.insert(notification_data.notification.id());
    }
  }

  std::move(callback).Run(std::move(notifications),
                          true /* supports_synchronization */);
}

void StubNotificationDisplayService::ProcessNotificationOperation(
    NotificationOperation operation,
    NotificationHandler::Type notification_type,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    const std::optional<bool>& by_user) {
  if (process_notification_operation_delegate_) {
    process_notification_operation_delegate_.Run(operation, notification_type,
                                                 origin, notification_id,
                                                 action_index, reply, by_user);
    return;
  }

  NotificationDisplayServiceImpl::ProcessNotificationOperation(
      operation, notification_type, origin, notification_id, action_index,
      reply, by_user);
}

StubNotificationDisplayService::NotificationData::NotificationData(
    NotificationHandler::Type type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata)
    : type(type), notification(notification), metadata(std::move(metadata)) {}

StubNotificationDisplayService::NotificationData::NotificationData(
    NotificationData&& other)
    : type(other.type),
      notification(std::move(other.notification)),
      metadata(std::move(other.metadata)) {}

StubNotificationDisplayService::NotificationData::~NotificationData() {}

StubNotificationDisplayService::NotificationData&
StubNotificationDisplayService::NotificationData::operator=(
    NotificationData&& other) {
  type = other.type;
  notification = std::move(other.notification);
  metadata = std::move(other.metadata);
  return *this;
}

std::vector<StubNotificationDisplayService::NotificationData>::iterator
StubNotificationDisplayService::FindNotification(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  return base::ranges::find_if(
      notifications_,
      [notification_type, &notification_id](const NotificationData& data) {
        return data.type == notification_type &&
               data.notification.id() == notification_id;
      });
}
