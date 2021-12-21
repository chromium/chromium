// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/camera_roll_opt_in_view.h"

#include <memory>
#include <string>

#include "ash/components/phonehub/camera_roll_manager.h"
#include "ash/components/phonehub/util/histogram_util.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

CameraRollOptInView::CameraRollOptInView(
    phonehub::CameraRollManager* camera_roll_manager)
    : SubFeatureOptInView(PhoneHubViewID::kCameraRollOptInView,
                          IDS_ASH_PHONE_HUB_CAMERA_ROLL_OPT_IN_DESCRIPTION,
                          IDS_ASH_PHONE_HUB_CAMERA_ROLL_OPT_IN_TURN_ON_BUTTON),
      camera_roll_manager_(camera_roll_manager) {}

CameraRollOptInView::~CameraRollOptInView() = default;

void CameraRollOptInView::SetUpButtonPressed() {
  LogCameraRollOptInEvent(phone_hub_metrics::InterstitialScreenEvent::kConfirm);
  camera_roll_manager_->EnableCameraRollFeatureInSystemSetting();
  phonehub::util::LogCameraRollFeatureOptInEntryPoint(
      phonehub::util::CameraRollOptInEntryPoint::kOnboardingDialog);
}

void CameraRollOptInView::DismissButtonPressed() {
  LogCameraRollOptInEvent(phone_hub_metrics::InterstitialScreenEvent::kDismiss);
  camera_roll_manager_->OnCameraRollOnboardingUiDismissed();
}

BEGIN_METADATA(CameraRollOptInView, views::View)
END_METADATA

}  // namespace ash
