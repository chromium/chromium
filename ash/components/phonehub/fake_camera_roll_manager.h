// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_MANAGER_H_
#define ASH_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_MANAGER_H_

#include "ash/components/phonehub/camera_roll_manager.h"
#include "ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {

class FakeCameraRollManager : public CameraRollManager {
 public:
  FakeCameraRollManager();
  ~FakeCameraRollManager() override;

  void OnCameraRollOnboardingUiDismissed() override;
  void SetIsCameraRollAvailableToBeEnabled(bool can_enable);
  void SetIsCameraRollAccessible(bool accessiable);
  void EnableCameraRollFeatureInSystemSetting() override;
  void SetIsAndroidStorageGranted(bool granted);

  using CameraRollManager::SetCurrentItems;

  using CameraRollManager::ClearCurrentItems;

 private:
  void ComputeAndUpdateUiState() override;
  // CameraRollManager:
  void DownloadItem(
      const proto::CameraRollItemMetadata& item_metadata) override;
  bool has_dismissed_onboarding_dialog_ = false;
  bool is_avaiable_to_be_enabled_ = true;
  bool is_camera_roll_accessible_ = true;
  bool is_refreshing_after_user_opt_in_ = false;
  bool is_android_storage_granted_ = true;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_MANAGER_H_
