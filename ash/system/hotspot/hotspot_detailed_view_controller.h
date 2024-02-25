// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOTSPOT_HOTSPOT_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_HOTSPOT_HOTSPOT_DETAILED_VIEW_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/hotspot/hotspot_detailed_view.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// This class encapsulates the logic to update the detailed Hotspot
// page within the quick settings and translate user interaction with the
// detailed view into Hotspot state changes.
class ASH_EXPORT HotspotDetailedViewController
    : public DetailedViewController,
      public hotspot_config::mojom::CrosHotspotConfigObserver,
      public HotspotDetailedView::Delegate {
 public:
  explicit HotspotDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  HotspotDetailedViewController(const HotspotDetailedViewController&) = delete;
  HotspotDetailedViewController& operator=(
      const HotspotDetailedViewController&) = delete;
  ~HotspotDetailedViewController() override;

 private:
  friend class HotspotDetailedViewControllerTest;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

  // hotspot_config::mojom::CrosHotspotConfigObserver:
  void OnHotspotInfoChanged() override;

  // HotspotDetailedView::Delegate:
  void OnToggleClicked(bool new_state) override;

  void OnGetHotspotInfo(hotspot_config::mojom::HotspotInfoPtr hotspot_info);

  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  mojo::Remote<hotspot_config::mojom::CrosHotspotConfig>
      remote_cros_hotspot_config_;
  mojo::Receiver<hotspot_config::mojom::CrosHotspotConfigObserver>
      cros_hotspot_config_observer_receiver_{this};

  hotspot_config::mojom::HotspotInfoPtr hotspot_info_;

  // Owned by views hierarchy.
  raw_ptr<HotspotDetailedView, DanglingUntriaged> view_ = nullptr;

  raw_ptr<UnifiedSystemTrayController> tray_controller_ = nullptr;

  base::WeakPtrFactory<HotspotDetailedViewController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOTSPOT_HOTSPOT_DETAILED_VIEW_CONTROLLER_H_
