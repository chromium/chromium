// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FOCUS_MODE_FOCUS_MODE_FEATURE_POD_CONTROLLER_H_
#define ASH_WM_FOCUS_MODE_FOCUS_MODE_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class FeaturePodButton;
class UnifiedSystemTrayController;

// Controller of the feature pod button that allows users to toggle whether
// Focus Mode is enabled or disabled, and that allows users to navigate to a
// more detailed page with the Focus Mode settings.
class ASH_EXPORT FocusModeFeaturePodController
    : public FeaturePodControllerBase {
 public:
  explicit FocusModeFeaturePodController(
      UnifiedSystemTrayController* tray_controller);
  FocusModeFeaturePodController(const FocusModeFeaturePodController&) = delete;
  FocusModeFeaturePodController& operator=(
      const FocusModeFeaturePodController&) = delete;
  ~FocusModeFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;

 private:
  const raw_ptr<UnifiedSystemTrayController, ExperimentalAsh> tray_controller_;
  raw_ptr<FeaturePodButton, ExperimentalAsh> button_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<FeatureTile, ExperimentalAsh> tile_ =
      nullptr;  // Owned by views hierarchy.

  base::WeakPtrFactory<FocusModeFeaturePodController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_FOCUS_MODE_FOCUS_MODE_FEATURE_POD_CONTROLLER_H_
