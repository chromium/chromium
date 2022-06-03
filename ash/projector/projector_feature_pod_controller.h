// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_FEATURE_POD_CONTROLLER_H_
#define ASH_PROJECTOR_PROJECTOR_FEATURE_POD_CONTROLLER_H_

#include "ash/projector/model/projector_ui_model.h"
#include "ash/system/unified/feature_pod_controller_base.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of a feature pod button that toggles Projector tools.
class ProjectorFeaturePodController : public FeaturePodControllerBase,
                                      public ProjectorUiModelObserver {
 public:
  explicit ProjectorFeaturePodController(
      UnifiedSystemTrayController* controller);
  ProjectorFeaturePodController(const ProjectorFeaturePodController&) = delete;
  ProjectorFeaturePodController& operator=(
      const ProjectorFeaturePodController&) = delete;
  ~ProjectorFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

  // ProjectorUiModelObserver:
  void OnProjectorBarStateChanged(bool enabled) override;

 private:
  UnifiedSystemTrayController* const tray_controller_;

  FeaturePodButton* button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_FEATURE_POD_CONTROLLER_H_
