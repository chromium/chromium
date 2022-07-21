// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAMERA_AUTOZOOM_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_CAMERA_AUTOZOOM_FEATURE_POD_CONTROLLER_H_

#include "ash/system/unified/feature_pod_controller_base.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace ash {

// Controller of a feature pod button that toggles autozoom.
class AutozoomFeaturePodController : public FeaturePodControllerBase,
                                     public media::CameraActiveClientObserver {
 public:
  AutozoomFeaturePodController();

  AutozoomFeaturePodController(const AutozoomFeaturePodController&) = delete;
  AutozoomFeaturePodController& operator=(const AutozoomFeaturePodController&) =
      delete;

  ~AutozoomFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

 private:
  void OnToggled();
  void UpdateButton();

  void UpdateButtonVisibility();

  // CameraActiveClientObserver
  void OnActiveClientChange(cros::mojom::CameraClientType type,
                            bool is_active) override;

  FeaturePodButton* button_ = nullptr;

  int active_camera_client_count_ = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAMERA_AUTOZOOM_FEATURE_POD_CONTROLLER_H_
