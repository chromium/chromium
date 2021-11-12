// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_ACCESS_MANAGER_H_
#define ASH_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_ACCESS_MANAGER_H_

#include "ash/components/phonehub/notification_access_manager.h"

namespace ash {
namespace phonehub {

class FakeNotificationAccessManager : public NotificationAccessManager {
 public:
  explicit FakeNotificationAccessManager(
      AccessStatus access_status = AccessStatus::kAvailableButNotGranted);
  ~FakeNotificationAccessManager() override;

  using NotificationAccessManager::IsSetupOperationInProgress;

  void SetAccessStatusInternal(AccessStatus access_status) override;
  void SetNotificationSetupOperationStatus(
      NotificationAccessSetupOperation::Status new_status);

  // NotificationAccessManager:
  AccessStatus GetAccessStatus() const override;
  bool HasNotificationSetupUiBeenDismissed() const override;
  void DismissSetupRequiredUi() override;

  void ResetHasNotificationSetupUiBeenDismissed();

 private:
  AccessStatus access_status_;
  bool has_notification_setup_ui_been_dismissed_ = false;
};

}  // namespace phonehub
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
namespace phonehub {
using ::ash::phonehub::FakeNotificationAccessManager;
}
}  // namespace chromeos

#endif  // ASH_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_ACCESS_MANAGER_H_
