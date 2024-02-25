// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_POD_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class FeatureTile;
class UnifiedSystemTrayController;

// Controller of accessibility feature tile.
// TODO(b/251724646):  rename to `AccessibilityFeatureTileController` same for
// the other feature pod controllers.
class ASH_EXPORT AccessibilityFeaturePodController
    : public FeaturePodControllerBase,
      public AccessibilityObserver {
 public:
  explicit AccessibilityFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  AccessibilityFeaturePodController(const AccessibilityFeaturePodController&) =
      delete;
  AccessibilityFeaturePodController& operator=(
      const AccessibilityFeaturePodController&) = delete;

  ~AccessibilityFeaturePodController() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // FeaturePodControllerBase:
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

 private:
  // Updates `tile_` state to reflect the current accessibility features state.
  // The `tile_` is toggled if any features are enabled and a sublabel is
  // displayed with details for the enabled features.
  void UpdateTileStateIfExists();

  // Unowned.
  const raw_ptr<UnifiedSystemTrayController, DanglingUntriaged>
      tray_controller_;

  // Owned by views hierarchy.
  raw_ptr<FeatureTile, DanglingUntriaged> tile_ = nullptr;

  base::WeakPtrFactory<AccessibilityFeaturePodController> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_POD_CONTROLLER_H_
