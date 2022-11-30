// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_SNAPSHOT_REBOOT_NOTIFICATION_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_SNAPSHOT_REBOOT_NOTIFICATION_H_

#include "ash/components/arc/enterprise/arc_snapshot_reboot_notification.h"
#include "base/callback.h"

namespace arc {
namespace data_snapshotd {

// Fake implementation of ArcSnapshotRebootNotification for tests.
class FakeSnapshotRebootNotification : public ArcSnapshotRebootNotification {
 public:
  FakeSnapshotRebootNotification();
  FakeSnapshotRebootNotification(const FakeSnapshotRebootNotification&) =
      delete;
  FakeSnapshotRebootNotification& operator=(
      const FakeSnapshotRebootNotification&) = delete;

  ~FakeSnapshotRebootNotification() override;

  // ArcSnapshotRebootNotification overrides:
  void SetUserConsentCallback(const base::RepeatingClosure& closure) override;
  void Show() override;
  void Hide() override;

  void Click();
  bool shown() const { return shown_; }

 private:
  base::RepeatingClosure user_consent_callback_;
  bool shown_ = false;
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_SNAPSHOT_REBOOT_NOTIFICATION_H_
