// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_DISPLAY_DETAILED_VIEW_H_
#define ASH_SYSTEM_BRIGHTNESS_DISPLAY_DETAILED_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class UnifiedBrightnessSliderController;
class UnifiedSystemTrayController;
class FeaturePodControllerBase;

// The detailed view to show when the drill-in button next to the brightness
// slider is clicked. This view contains a night light feature tile, a dark mode
// feature tile, and a brightness slider.
class ASH_EXPORT DisplayDetailedView : public TrayDetailedView {
  METADATA_HEADER(DisplayDetailedView, TrayDetailedView)

 public:
  DisplayDetailedView(DetailedViewDelegate* delegate,
                      UnifiedSystemTrayController* tray_controller);
  DisplayDetailedView(const DisplayDetailedView&) = delete;
  DisplayDetailedView& operator=(const DisplayDetailedView&) = delete;
  ~DisplayDetailedView() override;

  views::View* GetScrollContentForTest();

 private:
  // TrayDetailedView:
  void CreateExtraTitleRowButtons() override;

  // Callback of the `settings_button_` to open the display system settings
  // page.
  void OnSettingsClicked();

  std::unique_ptr<UnifiedBrightnessSliderController>
      brightness_slider_controller_;
  const raw_ptr<UnifiedSystemTrayController> unified_system_tray_controller_;

  // The vector of `FeaturePodControllerBase`. This is needed to store the
  // controllers of both tiles so that the controllers exist while the page is
  // open.
  std::vector<std::unique_ptr<FeaturePodControllerBase>>
      feature_tile_controllers_;

  // Owned by the views hierarchy.
  raw_ptr<views::Button> settings_button_ = nullptr;

  base::WeakPtrFactory<DisplayDetailedView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_DISPLAY_DETAILED_VIEW_H_
