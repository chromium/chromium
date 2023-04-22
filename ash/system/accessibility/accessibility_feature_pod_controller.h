// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_POD_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class FeatureTile;
class UnifiedSystemTrayController;

// Controller of accessibility feature pod button.
class ASH_EXPORT AccessibilityFeaturePodController
    : public FeaturePodControllerBase {
 public:
  AccessibilityFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  AccessibilityFeaturePodController(const AccessibilityFeaturePodController&) =
      delete;
  AccessibilityFeaturePodController& operator=(
      const AccessibilityFeaturePodController&) = delete;

  ~AccessibilityFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

 private:
  // Unowned.
  const raw_ptr<UnifiedSystemTrayController, ExperimentalAsh> tray_controller_;

  base::WeakPtrFactory<AccessibilityFeaturePodController> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_POD_CONTROLLER_H_
