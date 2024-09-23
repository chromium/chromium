// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EOL_EOL_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_EOL_EOL_NOTIFICATION_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/eol/eol_incentive_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "third_party/cros_system_api/dbus/update_engine/dbus-constants.h"
#include "ui/message_center/public/cpp/notification.h"

namespace base {
class Clock;
}  // namespace base

namespace ash {

// EolNotification is created when user logs in. It is used to check current
// EndOfLife date of the device, and show warning notifications accordingly.
class EolNotification final : public message_center::NotificationObserver {
 public:
  // Returns true if the eol notification needs to be displayed.
  static bool ShouldShowEolNotification();

  explicit EolNotification(Profile* profile);

  EolNotification(const EolNotification&) = delete;
  EolNotification& operator=(const EolNotification&) = delete;

  ~EolNotification();

  // Check Eol info from update engine.
  void CheckEolInfo();

  // message_center::NotificationObserver:
  void Close(bool by_user) override;

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

  void OverrideClockForTesting(base::Clock* clock);

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

  // Conditionally shows the appropriate EOL notification.
  void MaybeShowEolNotification(base::Time eol_date);

  // Shows the EOL incentive notification when the correct criteria are met. If
  // the final EOL incentive date has passed and the final incenteve was not
  // shown, then a normal EOL notification is shown.
  void MaybeShowEolIncentiveNotification(
      base::Time eol_date,
      eol_incentive_util::EolIncentiveType incentive_type);

  // Creates the EOL incentive notification.
  void ShowIncentiveNotification(
      base::Time eol_date,
      eol_incentive_util::EolIncentiveType incentive_type);

  // Resets all notification dismissed prefs back to false.
  void ResetDismissedPrefs();

  // Overridden for testing pending EOL notifications.
  raw_ptr<base::Clock, DanglingUntriaged> clock_;

  // Profile which is associated with the EndOfLife notification.
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  // Pref which determines which warning should be displayed to the user.
  std::optional<std::string> dismiss_pref_;

  // Factory of callbacks.
  base::WeakPtrFactory<EolNotification> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EOL_EOL_NOTIFICATION_H_
