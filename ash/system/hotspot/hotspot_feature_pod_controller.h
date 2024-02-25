// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOTSPOT_HOTSPOT_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_HOTSPOT_HOTSPOT_FEATURE_POD_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/hotspot/hotspot_icon_animation_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

class UnifiedSystemTrayController;

// Controller of the feature tile button that allows users to toggle whether
// Hotspot is enabled or disabled, and also allows users to navigate to a more
// detailed page with a Hotspot info.
class ASH_EXPORT HotspotFeaturePodController
    : public FeaturePodControllerBase,
      public HotspotIconAnimationObserver,
      public hotspot_config::mojom::CrosHotspotConfigObserver {
 public:
  explicit HotspotFeaturePodController(
      UnifiedSystemTrayController* tray_controller);
  HotspotFeaturePodController(const HotspotFeaturePodController&) = delete;
  HotspotFeaturePodController& operator=(const HotspotFeaturePodController&) =
      delete;
  ~HotspotFeaturePodController() override;

  // FeaturePodControllerBase:
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;

  // HotspotIconAnimationObserver:
  void HotspotIconChanged() override;

 private:
  // mojom::CrosHotspotConfigObserver:
  void OnHotspotInfoChanged() override;

  void OnGetHotspotInfo(hotspot_config::mojom::HotspotInfoPtr hotspot_info);
  void UpdateTileState();
  // Enable the hotspot if it is allowed and then show hotspot detailed page.
  void EnableHotspotIfAllowedAndDiveIn();
  void TrackToggleHotspotUMA(
      bool target_toggle_state,
      hotspot_config::mojom::HotspotControlResult operation_result);
  const gfx::VectorIcon& ComputeIcon() const;
  std::u16string ComputeSublabel() const;
  std::u16string ComputeIconTooltip() const;
  std::u16string ComputeTileTooltip() const;

  mojo::Remote<hotspot_config::mojom::CrosHotspotConfig>
      remote_cros_hotspot_config_;
  mojo::Receiver<hotspot_config::mojom::CrosHotspotConfigObserver>
      hotspot_config_observer_receiver_{this};

  hotspot_config::mojom::HotspotInfoPtr hotspot_info_;

  // Owned by views hierarchy.
  raw_ptr<FeatureTile, DanglingUntriaged> tile_ = nullptr;

  raw_ptr<UnifiedSystemTrayController, DanglingUntriaged> tray_controller_;

  base::WeakPtrFactory<HotspotFeaturePodController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOTSPOT_HOTSPOT_FEATURE_POD_CONTROLLER_H_
