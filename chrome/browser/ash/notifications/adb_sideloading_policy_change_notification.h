// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_ADB_SIDELOADING_POLICY_CHANGE_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_ADB_SIDELOADING_POLICY_CHANGE_NOTIFICATION_H_

#include <optional>

#include "base/memory/weak_ptr.h"

namespace ash {

// When the administrator changes the ADB sideloading device policy to either
// DISALLOW or DISALLOW_WITH_POWERWASH, the users will be notified of this
// change on next login. In case of a change to DISALLOW a one-time notification
// will be displayed. In case of a change to DISALLOW_WITH_POWERWASH the users
// will be shown two notifications, one informing them that their device will be
// powerwashed at the first reboot after 24 hours and one displayed after at
// least 24 hours informing that the powerwash will happen at the next reboot.
class AdbSideloadingPolicyChangeNotification {
 public:
  enum class Type {
    kNone = 0,
    // Corresponds to the one-time notification informing the user that
    // sideloading has been disallowed.
    kSideloadingDisallowed = 1,
    // Corresponds to the notification informing the user that a powerwash will
    // be performed at the first reboot after 24 hours.
    kPowerwashPlanned = 2,
    // Corresponds to the notification informing the user that a powerwash will
    // be performed after the next reboot.
    kPowerwashOnNextReboot = 3
  };

  AdbSideloadingPolicyChangeNotification();

  // Not copyable or movable
  AdbSideloadingPolicyChangeNotification(
      const AdbSideloadingPolicyChangeNotification&) = delete;
  AdbSideloadingPolicyChangeNotification& operator=(
      const AdbSideloadingPolicyChangeNotification&) = delete;

  virtual ~AdbSideloadingPolicyChangeNotification();

  virtual void Show(Type type);
  void HandleNotificationClick(std::optional<int> button_index);

 private:
  base::WeakPtrFactory<AdbSideloadingPolicyChangeNotification>
      weak_ptr_factory_{this};
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_ADB_SIDELOADING_POLICY_CHANGE_NOTIFICATION_H_
