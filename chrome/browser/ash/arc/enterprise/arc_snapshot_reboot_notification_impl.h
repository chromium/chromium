// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ENTERPRISE_ARC_SNAPSHOT_REBOOT_NOTIFICATION_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_ENTERPRISE_ARC_SNAPSHOT_REBOOT_NOTIFICATION_IMPL_H_

#include "ash/components/arc/enterprise/arc_snapshot_reboot_notification.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {
namespace data_snapshotd {

// The actual implementation of notification shown to the user when there is a
// need to reboot a device to update ARC data snapshot.
class ArcSnapshotRebootNotificationImpl : public ArcSnapshotRebootNotification {
 public:
  ArcSnapshotRebootNotificationImpl();
  ArcSnapshotRebootNotificationImpl(const ArcSnapshotRebootNotificationImpl&) =
      delete;
  ~ArcSnapshotRebootNotificationImpl() override;

  ArcSnapshotRebootNotificationImpl& operator=(
      const ArcSnapshotRebootNotificationImpl&) = delete;

  // ArcSnapshotRebootNotificationImpl overrides:
  void SetUserConsentCallback(const base::RepeatingClosure& callback) override;
  void Show() override;
  void Hide() override;

  static std::string get_notification_id_for_testing();
  static int get_restart_button_id_for_testing();

 private:
  void HandleClick(absl::optional<int> button_index);

  base::RepeatingClosure user_consent_callback_;
  bool shown_ = false;

  base::WeakPtrFactory<ArcSnapshotRebootNotificationImpl> weak_ptr_factory_{
      this};
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ENTERPRISE_ARC_SNAPSHOT_REBOOT_NOTIFICATION_IMPL_H_
