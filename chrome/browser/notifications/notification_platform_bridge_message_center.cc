// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_message_center.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/profile_notification.h"
#include "chrome/common/notifications/notification_operation.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// A NotificationDelegate that passes through click actions to the notification
// display service (and on to the appropriate handler). This is a temporary
// class to ease the transition from NotificationDelegate to
// NotificationHandler.
// TODO(estade): also handle other NotificationDelegate actions as needed.
class PassThroughDelegate : public message_center::NotificationDelegate {
 public:
  PassThroughDelegate(Profile* profile,
                      const message_center::Notification& notification,
                      NotificationHandler::Type notification_type)
      : profile_(profile),
        notification_(notification),
        notification_type_(notification_type) {
    DCHECK_NE(notification_type, NotificationHandler::Type::TRANSIENT);
  }
  PassThroughDelegate(const PassThroughDelegate&) = delete;
  PassThroughDelegate& operator=(const PassThroughDelegate&) = delete;

  void SettingsClick() override {
    NotificationDisplayServiceImpl::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationOperation::kSettings, notification_type_,
            notification_.origin_url(), notification_.id(), std::nullopt,
            std::nullopt, std::nullopt /* by_user */);
  }

  void DisableNotification() override {
    NotificationDisplayServiceImpl::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationOperation::kDisablePermission, notification_type_,
            notification_.origin_url(), notification_.id(),
            std::nullopt /* action_index */, std::nullopt /* reply */,
            std::nullopt /* by_user */);
  }

  void Close(bool by_user) override {
    NotificationDisplayServiceImpl::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationOperation::kClose, notification_type_,
            notification_.origin_url(), notification_.id(),
            std::nullopt /* action_index */, std::nullopt /* reply */, by_user);
  }

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    NotificationDisplayServiceImpl::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationOperation::kClick, notification_type_,
            notification_.origin_url(), notification_.id(), button_index, reply,
            std::nullopt /* by_user */);
  }

 protected:
  ~PassThroughDelegate() override = default;

 private:
  raw_ptr<Profile> profile_;
  message_center::Notification notification_;
  NotificationHandler::Type notification_type_;
};

}  // namespace

// static
NotificationPlatformBridgeMessageCenter*
NotificationPlatformBridgeMessageCenter::Get() {
  static base::NoDestructor<NotificationPlatformBridgeMessageCenter> instance;
  return instance.get();
}

NotificationPlatformBridgeMessageCenter::
    NotificationPlatformBridgeMessageCenter() = default;

NotificationPlatformBridgeMessageCenter::
    ~NotificationPlatformBridgeMessageCenter() = default;

void NotificationPlatformBridgeMessageCenter::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> /* metadata */) {
  NotificationUIManager* ui_manager =
      g_browser_process->notification_ui_manager();
  if (!ui_manager)
    return;  // The process is shutting down.

  if (notification.delegate() ||
      notification_type == NotificationHandler::Type::TRANSIENT) {
    ui_manager->Add(notification, profile);
    return;
  }

  // If there's no delegate, replace it with a PassThroughDelegate so clicks
  // go back to the appropriate handler.
  message_center::Notification notification_with_delegate(notification);
  notification_with_delegate.set_delegate(base::WrapRefCounted(
      new PassThroughDelegate(profile, notification, notification_type)));
  ui_manager->Add(notification_with_delegate, profile);
}

void NotificationPlatformBridgeMessageCenter::Close(
    Profile* profile,
    const std::string& notification_id) {
  NotificationUIManager* ui_manager =
      g_browser_process->notification_ui_manager();
  if (!ui_manager)
    return;  // the process is shutting down

  ui_manager->CancelById(notification_id,
                         ProfileNotification::GetProfileID(profile));
}

void NotificationPlatformBridgeMessageCenter::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  std::set<std::string> displayed_notifications;
  NotificationUIManager* ui_manager =
      g_browser_process->notification_ui_manager();
  if (ui_manager) {
    displayed_notifications = ui_manager->GetAllIdsByProfile(
        ProfileNotification::GetProfileID(profile));
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(displayed_notifications),
                     true /* supports_synchronization */));
}

void NotificationPlatformBridgeMessageCenter::GetDisplayedForOrigin(
    Profile* profile,
    const GURL& origin,
    GetDisplayedNotificationsCallback callback) const {
  std::set<std::string> displayed_notifications;
  NotificationUIManager* ui_manager =
      g_browser_process->notification_ui_manager();
  if (ui_manager) {
    displayed_notifications = ui_manager->GetAllIdsByProfileAndOrigin(
        ProfileNotification::GetProfileID(profile), origin);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(displayed_notifications),
                     true /* supports_synchronization */));
}

void NotificationPlatformBridgeMessageCenter::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(true /* success */);
}

void NotificationPlatformBridgeMessageCenter::DisplayServiceShutDown(
    Profile* profile) {}
