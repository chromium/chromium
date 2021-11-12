// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_OPT_IN_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_OPT_IN_VIEW_H_

#include "ash/ash_export.h"
#include "ash/components/phonehub/camera_roll_manager.h"
#include "ash/system/phonehub/sub_feature_opt_in_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

/**
 * Dialog inside camera roll view for user to opt in camera roll feature or
 * dismiss.
 */
class ASH_EXPORT CameraRollOptInView : public SubFeatureOptInView {
 public:
  METADATA_HEADER(CameraRollOptInView);

  CameraRollOptInView(phonehub::CameraRollManager* camera_roll_manager);
  ~CameraRollOptInView() override;
  CameraRollOptInView(CameraRollOptInView&) = delete;
  CameraRollOptInView operator=(CameraRollOptInView&) = delete;

 private:
  // SubFeatureOptInView::
  void SetUpButtonPressed() override;
  void DismissButtonPressed() override;

  phonehub::CameraRollManager* camera_roll_manager_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_OPT_IN_VIEW_H_
