// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_NOTIFICATION_H_

#include <string_view>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

// Class that is responsible for showing the Extended Updates notification and
// acts as the delegate that handles interactions.
class ExtendedUpdatesNotification
    : public message_center::NotificationDelegate {
 public:
  // Maps notification buttons to their ordered indices.
  enum class IndexedButton : int {
    kSetUp = 0,
    kLearnMore = 1,
  };

  static constexpr std::string_view kNotificationId =
      "ash.extended_updates.available";
  static constexpr NotificationHandler::Type kNotificationType =
      NotificationHandler::Type::TRANSIENT;

  // Shows the notification.
  static void Show(Profile* profile);
  static void Show(scoped_refptr<ExtendedUpdatesNotification> delegate);

  // Returns true if Extended Updates notification was dismissed by the user.
  static bool IsNotificationDismissed(Profile* profile);

  explicit ExtendedUpdatesNotification(Profile* profile);
  ExtendedUpdatesNotification(const ExtendedUpdatesNotification&) = delete;
  ExtendedUpdatesNotification& operator=(const ExtendedUpdatesNotification&) =
      delete;

  // message_center::NotificationDelegate overrides.
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 protected:
  // Ref-counted class requires protected destructor.
  ~ExtendedUpdatesNotification() override;

  virtual void ShowExtendedUpdatesDialog();
  virtual void OpenLearnMoreUrl();

 private:
  Profile* profile() { return profile_.get(); }

  void SubscribeToDeviceSettingsChanges();
  void OnDeviceSettingsChanged();

  base::WeakPtr<Profile> profile_;

  base::CallbackListSubscription settings_change_subscription_;

  base::WeakPtrFactory<ExtendedUpdatesNotification> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_NOTIFICATION_H_
