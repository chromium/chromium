// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_FEATURE_POD_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_FEATURE_POD_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class FeatureTile;
class UnifiedSystemTrayController;

// Controller of a feature pod button that launches screen capture.
class ASH_EXPORT CaptureModeFeaturePodController
    : public FeaturePodControllerBase {
 public:
  explicit CaptureModeFeaturePodController(
      UnifiedSystemTrayController* controller);
  CaptureModeFeaturePodController(const CaptureModeFeaturePodController&) =
      delete;
  CaptureModeFeaturePodController& operator=(
      const CaptureModeFeaturePodController&) = delete;
  ~CaptureModeFeaturePodController() override;

  // Referenced by `UnifiedSystemTrayController` to know whether to construct a
  // Primary or Compact tile.
  static bool CalculateButtonVisibility();

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

 private:
  const raw_ptr<UnifiedSystemTrayController,
                DanglingUntriaged | ExperimentalAsh>
      tray_controller_;

  raw_ptr<FeaturePodButton, ExperimentalAsh> button_ = nullptr;

  base::WeakPtrFactory<CaptureModeFeaturePodController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_FEATURE_POD_CONTROLLER_H_
