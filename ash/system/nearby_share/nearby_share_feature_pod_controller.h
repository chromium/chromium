// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/nearby_share/nearby_share_controller_impl.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

class NearbyShareDelegate;
class UnifiedSystemTrayController;

// Controller for a feature tile that toggles the high visibility mode of Nearby
// Share.
class ASH_EXPORT NearbyShareFeaturePodController
    : public FeaturePodControllerBase,
      public NearbyShareControllerImpl::Observer {
 public:
  explicit NearbyShareFeaturePodController(
      UnifiedSystemTrayController* tray_controller);
  NearbyShareFeaturePodController(const NearbyShareFeaturePodController&) =
      delete;
  NearbyShareFeaturePodController& operator=(
      const NearbyShareFeaturePodController&) = delete;
  ~NearbyShareFeaturePodController() override;

  // FeaturePodControllerBase:
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;

  // NearbyShareController::Observer
  void OnHighVisibilityEnabledChanged(bool enabled) override;
  void OnVisibilityChanged(
      ::nearby_share::mojom::Visibility visibility) override;

 private:
  void UpdateButton(bool enabled);
  void UpdateQSv2Button();
  void ToggleTileOn();
  void ToggleTileOff();

  base::TimeDelta RemainingHighVisibilityTime() const;

  // Countdown timer fires periodically to update the remaining time until
  // |shutoff_time_| as displayed by the pod button sub-label.
  base::RepeatingTimer countdown_timer_;
  base::TimeTicks shutoff_time_;

  const raw_ptr<UnifiedSystemTrayController> tray_controller_;
  const raw_ptr<NearbyShareDelegate> nearby_share_delegate_;
  const raw_ptr<NearbyShareControllerImpl> nearby_share_controller_;
  raw_ptr<FeatureTile, DanglingUntriaged> tile_ = nullptr;

  base::WeakPtrFactory<NearbyShareFeaturePodController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_FEATURE_POD_CONTROLLER_H_
