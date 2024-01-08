// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UPGRADE_AVAILABLE_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UPGRADE_AVAILABLE_NOTIFICATION_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

namespace crostini {

extern const char kNotifierCrostiniUpgradeAvailable[];

// Reasons the notification may be closed. These are used in histograms so do
// not remove/reorder entries. Only add at the end just before kMaxValue. Also
// remember to update the enum listing in
// tools/metrics/histograms/histograms.xml.
enum class CrostiniUpgradeAvailableNotificationClosed {
  // The notification was dismissed but not by the user (either automatically
  // or because the device was unplugged).
  kUnknown,
  // The user closed the notification via the close box.
  kByUser,
  // The user clicked on the Upgrade button of the notification.
  kUpgradeButton,
  // The user clicked on the body of the notification.
  kNotificationBody,
  // Maximum value for the enum.
  kMaxValue = kNotificationBody,
};

class CrostiniUpgradeAvailableNotification {
 public:
  explicit CrostiniUpgradeAvailableNotification(Profile* profile,
                                                base::OnceClosure closure);
  virtual ~CrostiniUpgradeAvailableNotification();

  static std::unique_ptr<CrostiniUpgradeAvailableNotification> Show(
      Profile* profile,
      base::OnceClosure closure);

  void ForceRedisplay();
  void UpgradeDialogShown();

  message_center::Notification* Get() { return notification_.get(); }

 private:
  raw_ptr<Profile> profile_;  // Not owned.
  std::unique_ptr<message_center::Notification> notification_;
  base::WeakPtrFactory<CrostiniUpgradeAvailableNotification> weak_ptr_factory_{
      this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UPGRADE_AVAILABLE_NOTIFICATION_H_
