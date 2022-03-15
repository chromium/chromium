// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_FAKE_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_
#define ASH_COMPONENTS_PHONEHUB_FAKE_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_

#include "ash/components/phonehub/multidevice_feature_access_manager.h"

namespace ash {
namespace phonehub {

class FakeMultideviceFeatureAccessManager
    : public MultideviceFeatureAccessManager {
 public:
  explicit FakeMultideviceFeatureAccessManager(
      AccessStatus notification_access_status =
          AccessStatus::kAvailableButNotGranted,
      AccessStatus camera_roll_access_status =
          AccessStatus::kAvailableButNotGranted,
      AccessStatus apps_access_status = AccessStatus::kAvailableButNotGranted,
      AccessProhibitedReason reason = AccessProhibitedReason::kWorkProfile);
  ~FakeMultideviceFeatureAccessManager() override;

  using MultideviceFeatureAccessManager::IsSetupOperationInProgress;

  void SetNotificationAccessStatusInternal(
      AccessStatus notification_access_status,
      AccessProhibitedReason reason) override;
  AccessStatus GetNotificationAccessStatus() const override;
  void SetNotificationSetupOperationStatus(
      NotificationAccessSetupOperation::Status new_status);
  AccessProhibitedReason GetNotificationAccessProhibitedReason() const override;

  bool HasMultideviceFeatureSetupUiBeenDismissed() const override;
  void DismissSetupRequiredUi() override;
  void ResetHasMultideviceFeatureSetupUiBeenDismissed();

  void SetCameraRollAccessStatusInternal(
      AccessStatus camera_roll_access_status) override;
  AccessStatus GetCameraRollAccessStatus() const override;
  AccessStatus GetAppsAccessStatus() const override;

  // Test-only.
  void SetAppsAccessStatusInternal(AccessStatus apps_access_status);

 private:
  AccessStatus notification_access_status_;
  AccessStatus camera_roll_access_status_;
  AccessStatus apps_access_status_;
  AccessProhibitedReason access_prohibited_reason_;
  bool has_notification_setup_ui_been_dismissed_ = false;
};

}  // namespace phonehub
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
namespace phonehub {
using ::ash::phonehub::FakeMultideviceFeatureAccessManager;
}
}  // namespace chromeos

#endif  // ASH_COMPONENTS_PHONEHUB_FAKE_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_
