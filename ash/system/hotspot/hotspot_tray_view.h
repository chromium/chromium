// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOTSPOT_HOTSPOT_TRAY_VIEW_H_
#define ASH_SYSTEM_HOTSPOT_HOTSPOT_TRAY_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/hotspot/hotspot_icon_animation_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// The icon in the system tray notifying users that their Chromebook hotspot is
// turned on.
class ASH_EXPORT HotspotTrayView
    : public TrayItemView,
      public SessionObserver,
      public HotspotIconAnimationObserver,
      public hotspot_config::mojom::CrosHotspotConfigObserver {
  METADATA_HEADER(HotspotTrayView, TrayItemView)

 public:
  explicit HotspotTrayView(Shelf* shelf);

  HotspotTrayView(const HotspotTrayView&) = delete;
  HotspotTrayView& operator=(const HotspotTrayView&) = delete;

  ~HotspotTrayView() override;

  std::u16string GetAccessibleNameString() const;

  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;

 private:
  // views::TrayItemView:
  void HandleLocaleChange() override;
  void OnThemeChanged() override;
  void UpdateLabelOrImageViewColor(bool active) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // mojom::CrosHotspotConfigObserver:
  void OnHotspotInfoChanged() override;

  // HotspotIconAnimationObserver:
  void HotspotIconChanged() override;

  void OnGetHotspotInfo(hotspot_config::mojom::HotspotInfoPtr hotspot_info);

  void UpdateIconImage();
  void UpdateIconVisibilityAndTooltip();
  void UpdateAccessibleName();

  // The tooltip and accessible name string used for the icon.
  std::u16string tooltip_;

  hotspot_config::mojom::HotspotState state_ =
      hotspot_config::mojom::HotspotState::kDisabled;

  mojo::Remote<hotspot_config::mojom::CrosHotspotConfig>
      remote_cros_hotspot_config_;
  mojo::Receiver<hotspot_config::mojom::CrosHotspotConfigObserver>
      hotspot_config_observer_receiver_{this};

  base::WeakPtrFactory<HotspotTrayView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOTSPOT_HOTSPOT_TRAY_VIEW_H_
