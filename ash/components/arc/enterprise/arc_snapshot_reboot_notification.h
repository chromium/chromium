// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ENTERPRISE_ARC_SNAPSHOT_REBOOT_NOTIFICATION_H_
#define ASH_COMPONENTS_ARC_ENTERPRISE_ARC_SNAPSHOT_REBOOT_NOTIFICATION_H_

#include "base/callback_forward.h"

namespace arc {
namespace data_snapshotd {

class ArcSnapshotRebootNotification {
 public:
  virtual ~ArcSnapshotRebootNotification() = default;

  // Sets a user consent callback |callback| to be invoked once the user
  // consents to reboot the device.
  virtual void SetUserConsentCallback(
      const base::RepeatingClosure& callback) = 0;

  // Shows a snapshot reboot notification.
  virtual void Show() = 0;

  // Hides a snapshot reboot notification.
  virtual void Hide() = 0;
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ENTERPRISE_ARC_SNAPSHOT_REBOOT_NOTIFICATION_H_
