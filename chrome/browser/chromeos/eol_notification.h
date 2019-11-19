// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EOL_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_EOL_NOTIFICATION_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/update_engine_client.h"
#include "third_party/cros_system_api/dbus/update_engine/dbus-constants.h"
#include "ui/message_center/public/cpp/notification.h"

namespace base {
class Clock;
}  // namespace base

namespace chromeos {

// EolNotification is created when user logs in. It is used to check current
// EndOfLife date of the device, and show warning notifications accordingly.
class EolNotification final : public message_center::NotificationObserver {
 public:
  // Returns true if the eol notification needs to be displayed.
  static bool ShouldShowEolNotification();

  explicit EolNotification(Profile* profile);
  ~EolNotification();

  // Check Eol info from update engine.
  void CheckEolInfo();

  // message_center::NotificationObserver:
  void Close(bool by_user) override;

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override;

 private:
  friend class EolNotificationTest;

  // Buttons that appear in the notification.  This is exposed for testing
  // purposes only and should never be directly used.
  enum ButtonIndex {
    BUTTON_MORE_INFO = 0,
    BUTTON_DISMISS,
    BUTTON_SIZE = BUTTON_DISMISS
  };

  // Attempts to create the notification.  If |now| is equal or greater than
  // |eol_date|, the final update notification is displayed, otherwise the first
  // or second warning notification displaying month and year of End of Life is
  // shown.
  void CreateNotification(base::Time eol_date, base::Time now);

  // Callback invoked when |GetEolInfo()| has finished.
  // - EolInfo eol_info: the End of Life info.
  void OnEolInfo(UpdateEngineClient::EolInfo eol_info);

  // Overridden for testing pending EOL notifications.
  base::Clock* clock_;

  // Profile which is associated with the EndOfLife notification.
  Profile* const profile_;

  // Pref which determines which warning should be displayed to the user.
  base::Optional<std::string> dismiss_pref_;

  // Factory of callbacks.
  base::WeakPtrFactory<EolNotification> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EolNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EOL_NOTIFICATION_H_
