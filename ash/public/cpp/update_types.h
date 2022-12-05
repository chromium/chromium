// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_UPDATE_TYPES_H_
#define ASH_PUBLIC_CPP_UPDATE_TYPES_H_

#include "base/time/time.h"

namespace ash {

// Urgency of a pending software update. Sets the system tray update icon color.
// These correspond to values in UpgradeDetector's
// `UpgradeNotificationAnnoyanceLevel` enum. Their use is platform-specific.
// Please refer to `UpgradeDetectorChromeos` for details.
// TODO(jamescook): `UpgradeDetector::UpgradeNotificationAnnoyanceLevel` could
// be replaced with this if this moves into a component shared with non-ash
// chrome.
enum class UpdateSeverity {
  kNone,
  kVeryLow,
  kLow,
  kElevated,
  kGrace,
  kHigh,
  kCritical,
};

// State for deferred system updates.
enum class DeferredUpdateState {
  // No deferred update available.
  kNone,
  // Show deferred update available dialog.
  kShowDialog,
  // Show deferred update available notification.
  kShowNotification
};

// Notification state for system updates, set by policies.
struct RelaunchNotificationState {
  enum {
    kNone,                   // Relaunch is not required.
    kRecommendedNotOverdue,  // Relaunch is recommended but not overdue.
    kRecommendedAndOverdue,  // Relaunch is recommended and overdue.
    kRequired,               // Relaunch is required until
                             // `rounded_time_until_reboot_required`.
  } requirement_type = kNone;

  enum PolicySource {
    kUser,    // Relaunch notifications are triggered by a user policy.
    kDevice,  // Relaunch notifications are triggered by a device policy..
  } policy_source = kUser;

  // The remaining time until the device will restart itself, rounded to the
  // nearest day, hour, minute, or second; depending on how far into the future
  // it is.
  base::TimeDelta rounded_time_until_reboot_required = base::TimeDelta();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_UPDATE_TYPES_H_
