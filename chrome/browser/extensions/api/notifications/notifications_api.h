// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_

#include <string>

#include "chrome/common/extensions/api/notifications.h"
#include "extensions/browser/extension_function.h"
#include "ui/message_center/public/cpp/notification_types.h"

class Profile;

namespace message_center {
class Notification;
}

namespace extensions {

class ExtensionNotificationDisplayHelper;

class NotificationsApiFunction : public ExtensionFunction {
 public:
  // Whether the current extension and channel allow the API. Public for
  // testing.
  bool IsNotificationsApiAvailable();

 protected:
  NotificationsApiFunction();
  ~NotificationsApiFunction() override;

  bool CreateNotification(const std::string& id,
                          api::notifications::NotificationOptions* options,
                          std::string* error);
  bool UpdateNotification(const std::string& id,
                          api::notifications::NotificationOptions* options,
                          message_center::Notification* notification,
                          std::string* error);

  bool IsNotificationsApiEnabled() const;

  bool AreExtensionNotificationsAllowed() const;

  Profile* GetProfile() const;

  // Returns the display helper that should be used for interacting with the
  // common notification system.
  ExtensionNotificationDisplayHelper* GetDisplayHelper() const;

  // Returns true if the API function is still allowed to run even when the
  // notifications for a notifier have been disabled.
  virtual bool CanRunWhileDisabled() const;

  // Called inside of Run().
  virtual ResponseAction RunNotificationsApi() = 0;

  // ExtensionFunction:
  ResponseAction Run() override;

  message_center::NotificationType MapApiTemplateTypeToType(
      api::notifications::TemplateType type);
};

class NotificationsCreateFunction : public NotificationsApiFunction {
 public:
  NotificationsCreateFunction();

  // NotificationsApiFunction:
  ResponseAction RunNotificationsApi() override;

 protected:
  ~NotificationsCreateFunction() override;

 private:
  std::optional<api::notifications::Create::Params> params_;

  DECLARE_EXTENSION_FUNCTION("notifications.create", NOTIFICATIONS_CREATE)
};

class NotificationsUpdateFunction : public NotificationsApiFunction {
 public:
  NotificationsUpdateFunction();

  // NotificationsApiFunction:
  ResponseAction RunNotificationsApi() override;

 protected:
  ~NotificationsUpdateFunction() override;

 private:
  std::optional<api::notifications::Update::Params> params_;

  DECLARE_EXTENSION_FUNCTION("notifications.update", NOTIFICATIONS_UPDATE)
};

class NotificationsClearFunction : public NotificationsApiFunction {
 public:
  NotificationsClearFunction();

  // NotificationsApiFunction:
  ResponseAction RunNotificationsApi() override;

 protected:
  ~NotificationsClearFunction() override;

 private:
  std::optional<api::notifications::Clear::Params> params_;

  DECLARE_EXTENSION_FUNCTION("notifications.clear", NOTIFICATIONS_CLEAR)
};

class NotificationsGetAllFunction : public NotificationsApiFunction {
 public:
  NotificationsGetAllFunction();

  // NotificationsApiFunction:
  ResponseAction RunNotificationsApi() override;

 protected:
  ~NotificationsGetAllFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notifications.getAll", NOTIFICATIONS_GET_ALL)
};

class NotificationsGetPermissionLevelFunction
    : public NotificationsApiFunction {
 public:
  NotificationsGetPermissionLevelFunction();

  // NotificationsApiFunction:
  bool CanRunWhileDisabled() const override;
  ResponseAction RunNotificationsApi() override;

 protected:
  ~NotificationsGetPermissionLevelFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notifications.getPermissionLevel",
                             NOTIFICATIONS_GETPERMISSIONLEVEL)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_
