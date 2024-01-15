// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_NOTIFICATION_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_NOTIFICATION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace message_center {
class Notification;
}

class Profile;

namespace enterprise_reporting {

class ExtensionRequestNotification
    : public message_center::NotificationObserver {
 public:
  using ExtensionIds = std::vector<std::string>;
  // Callback when the notification is closed.
  using NotificationCloseCallback = base::OnceCallback<void(bool by_user)>;
  enum NotifyType {
    kApproved = 0,
    kRejected = 1,
    kForceInstalled = 2,
    kNumberOfTypes = 3
  };

  ExtensionRequestNotification(Profile* profile,
                               const NotifyType notify_type,
                               const ExtensionIds& extension_ids);
  ExtensionRequestNotification(const ExtensionRequestNotification&) = delete;
  ExtensionRequestNotification& operator=(const ExtensionRequestNotification&) =
      delete;
  virtual ~ExtensionRequestNotification();

  void Show(NotificationCloseCallback callback);
  void CloseNotification();

 private:
  // message_center::NotificationObserver
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;
  void Close(bool by_user) override;

  std::unique_ptr<message_center::Notification> notification_;

  raw_ptr<Profile> profile_;
  const NotifyType notify_type_ = kApproved;
  const ExtensionIds extension_ids_;
  NotificationCloseCallback callback_;

  base::WeakPtrFactory<ExtensionRequestNotification> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_NOTIFICATION_H_
