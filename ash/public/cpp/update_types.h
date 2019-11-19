// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_UPDATE_TYPES_H_
#define ASH_PUBLIC_CPP_UPDATE_TYPES_H_

namespace ash {

// Urgency of a pending software update. Sets the system tray update icon color.
// These correspond to values in UpgradeDetector's
// UpgradeNotificationAnnoyanceLevel enum. Their use is platform-specific. On
// Chrome OS, kLow severity is issued when an update is detected. kElevated
// follows after two days, and kHigh two days after that. These time deltas may
// be overridden by administrators via the RelaunchNotificationPeriod policy
// setting.
// TODO(jamescook): UpgradeDetector::UpgradeNotificationAnnoyanceLevel could be
// replaced with this if this moves into a component shared with non-ash chrome.
enum class UpdateSeverity {
  kNone,
  kVeryLow,
  kLow,
  kElevated,
  kHigh,
  kCritical,
};

// The type of update being applied. Sets the string in the system tray.
enum class UpdateType {
  kFlash,
  kSystem,
};

// Notification style for system updates, set by different policies.
enum class NotificationStyle {
  kDefault,
  kAdminRecommended,  // Relaunch Notification policy
  kAdminRequired,     // Relaunch Notification policy
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_UPDATE_TYPES_H_
