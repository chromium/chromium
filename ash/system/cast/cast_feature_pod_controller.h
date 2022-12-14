// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_CAST_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_CAST_CAST_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of cast feature pod button.
class ASH_EXPORT CastFeaturePodController
    : public FeaturePodControllerBase,
      public CastConfigController::Observer {
 public:
  explicit CastFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  CastFeaturePodController(const CastFeaturePodController&) = delete;
  CastFeaturePodController& operator=(const CastFeaturePodController&) = delete;

  ~CastFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  std::unique_ptr<FeatureTile> CreateTile() override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;

  // CastConfigControllerObserver:
  void OnDevicesUpdated(const std::vector<SinkAndRoute>& devices) override;

 private:
  // Updates feature pod button visibility. Used pre-QsRevamp.
  void Update();

  // Updates tile sublabel visibility. Used post-QsRevamp.
  void UpdateSublabelVisibility();

  UnifiedSystemTrayController* const tray_controller_;

  // Owned by views hierarchy.
  FeaturePodButton* button_ = nullptr;
  FeatureTile* tile_ = nullptr;

  base::WeakPtrFactory<CastFeaturePodController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_CAST_FEATURE_POD_CONTROLLER_H_
