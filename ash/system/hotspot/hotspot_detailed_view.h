// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOTSPOT_HOTSPOT_DETAILED_VIEW_H_
#define ASH_SYSTEM_HOTSPOT_HOTSPOT_DETAILED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/hotspot/hotspot_icon_animation_observer.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Button;
class ImageView;
}  // namespace views

namespace ash {

class DetailedViewDelegate;
class HoverHighlightView;
class RoundedContainer;
class Switch;

// This class defines both the interface used to interact with the detailed
// Hotspot page within the quick settings. This class includes the declaration
// for the delegate interface it uses to propagate user interactions.
class ASH_EXPORT HotspotDetailedView : public TrayDetailedView,
                                       public HotspotIconAnimationObserver {
  METADATA_HEADER(HotspotDetailedView, TrayDetailedView)

 public:
  // This class defines the interface that HotspotDetailedView will use to
  // propagate user interactions.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual void OnToggleClicked(bool new_state) = 0;
  };

  HotspotDetailedView(DetailedViewDelegate* detailed_view_delegate,
                      Delegate* delegate);
  HotspotDetailedView(const HotspotDetailedView&) = delete;
  HotspotDetailedView& operator=(const HotspotDetailedView&) = delete;
  ~HotspotDetailedView() override;

  // Update the hotspot detailed view from the given `hotspot_info`.
  void UpdateViewForHotspot(hotspot_config::mojom::HotspotInfoPtr hotspot_info);

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void CreateExtraTitleRowButtons() override;

  // HotspotIconAnimationObserver:
  void HotspotIconChanged() override;

 private:
  friend class HotspotDetailedViewControllerTest;
  friend class HotspotDetailedViewTest;

  // Used for testing. Starts at 1 because view IDs should not be 0.
  enum class HotspotDetailedViewChildId {
    kInfoButton = 1,
    kSettingsButton = 2,
    kEntryRow = 3,
    kHotspotIcon = 4,
    kToggle = 5,
    kExtraIcon = 6,
  };

  // Creates the rounded container, which contains the main on/off toggle.
  void CreateContainer();

  // Attempts to close the quick settings and open the Hotspot subpage.
  void OnSettingsClicked();

  // Handles clicks on the Hotspot toggle button.
  void OnToggleClicked();

  // Handles toggling Hotspot via the UI to `new_state`.
  void ToggleHotspot(bool new_state);

  void UpdateIcon();
  void UpdateToggleState(
      const hotspot_config::mojom::HotspotState& state,
      const hotspot_config::mojom::HotspotAllowStatus& allow_status);
  void UpdateSubText(const hotspot_config::mojom::HotspotInfoPtr& hotspot_info);
  void UpdateExtraIcon(
      const hotspot_config::mojom::HotspotAllowStatus& allow_status);

  hotspot_config::mojom::HotspotState state_ =
      hotspot_config::mojom::HotspotState::kDisabled;
  hotspot_config::mojom::HotspotAllowStatus allow_status_ =
      hotspot_config::mojom::HotspotAllowStatus::kAllowed;

  const raw_ptr<Delegate> delegate_;

  // Owned by views hierarchy.
  raw_ptr<views::Button> settings_button_ = nullptr;
  raw_ptr<RoundedContainer> row_container_ = nullptr;
  raw_ptr<HoverHighlightView> entry_row_ = nullptr;
  raw_ptr<views::ImageView> hotspot_icon_ = nullptr;
  raw_ptr<Switch> toggle_ = nullptr;
  raw_ptr<views::ImageView> extra_icon_ = nullptr;

  base::WeakPtrFactory<HotspotDetailedView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOTSPOT_HOTSPOT_DETAILED_VIEW_H_
