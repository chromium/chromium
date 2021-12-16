// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/fake_camera_roll_manager.h"

#include "ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {

FakeCameraRollManager::FakeCameraRollManager() = default;

FakeCameraRollManager::~FakeCameraRollManager() = default;

void FakeCameraRollManager::DownloadItem(
    const proto::CameraRollItemMetadata& item_metadata) {}

void FakeCameraRollManager::OnCameraRollOnboardingUiDismissed() {
  has_dismissed_onboarding_dialog_ = true;
}

void FakeCameraRollManager::SetIsCameraRollAvailableToBeEnabled(
    bool can_enable) {
  is_avaiable_to_be_enabled_ = can_enable;
}

void FakeCameraRollManager::SetIsCameraRollAccessible(bool accessiable) {
  is_camera_roll_accessible_ = accessiable;
}

void FakeCameraRollManager::EnableCameraRollFeatureInSystemSetting() {
  is_refreshing_after_user_opt_in_ = true;
  ComputeAndUpdateUiState();
}

void FakeCameraRollManager::SetIsAndroidStorageGranted(bool granted) {
  is_android_storage_granted_ = granted;
  ComputeAndUpdateUiState();
}

void FakeCameraRollManager::ComputeAndUpdateUiState() {
  if (!is_camera_roll_accessible_) {
    ui_state_ = CameraRollUiState::SHOULD_HIDE;
  } else if (!is_android_storage_granted_) {
    ui_state_ = CameraRollUiState::NO_STORAGE_PERMISSION;
  } else if (is_avaiable_to_be_enabled_) {
    ui_state_ = (has_dismissed_onboarding_dialog_)
                    ? CameraRollUiState::SHOULD_HIDE
                    : CameraRollUiState::CAN_OPT_IN;
  } else if (is_refreshing_after_user_opt_in_) {
    ui_state_ = CameraRollUiState::LOADING_VIEW;
  } else if (current_items().empty()) {
    ui_state_ = CameraRollUiState::SHOULD_HIDE;
  } else {
    ui_state_ = CameraRollUiState::ITEMS_VISIBLE;
  }
  NotifyCameraRollViewUiStateUpdated();
}

}  // namespace phonehub
}  // namespace ash
