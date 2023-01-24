// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAMERA_AUTOZOOM_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_CAMERA_AUTOZOOM_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/camera/autozoom_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class FeaturePodButton;
class FeatureTile;

// Controller of a feature pod button that toggles autozoom.
class ASH_EXPORT AutozoomFeaturePodController : public FeaturePodControllerBase,
                                                public AutozoomObserver {
 public:
  AutozoomFeaturePodController();

  AutozoomFeaturePodController(const AutozoomFeaturePodController&) = delete;
  AutozoomFeaturePodController& operator=(const AutozoomFeaturePodController&) =
      delete;

  ~AutozoomFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

  // AutozoomObserver:
  void OnAutozoomStateChanged(
      cros::mojom::CameraAutoFramingState state) override;
  void OnAutozoomControlEnabledChanged(bool enabled) override;

 private:
  void UpdateButton(cros::mojom::CameraAutoFramingState state);

  void UpdateButtonVisibility();

  void UpdateTileVisibility();

  FeaturePodButton* button_ = nullptr;
  FeatureTile* tile_ = nullptr;

  base::WeakPtrFactory<AutozoomFeaturePodController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAMERA_AUTOZOOM_FEATURE_POD_CONTROLLER_H_
