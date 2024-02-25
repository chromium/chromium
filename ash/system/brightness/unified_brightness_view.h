// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_
#define ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/unified/unified_slider_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {
// View of a slider that can change display brightness. It observes current
// brightness level from UnifiedSystemTrayModel.
class ASH_EXPORT UnifiedBrightnessView
    : public UnifiedSliderView,
      public UnifiedSystemTrayModel::Observer {
  METADATA_HEADER(UnifiedBrightnessView, UnifiedSliderView)

 public:
  UnifiedBrightnessView(UnifiedBrightnessSliderController* controller,
                        scoped_refptr<UnifiedSystemTrayModel> model,
                        std::optional<views::Button::PressedCallback>
                            detailed_button_callback = std::nullopt);
  UnifiedBrightnessView(const UnifiedBrightnessView&) = delete;
  UnifiedBrightnessView& operator=(const UnifiedBrightnessView&) = delete;
  ~UnifiedBrightnessView() override;

  // UnifiedSystemTrayModel::Observer:
  void OnDisplayBrightnessChanged(bool by_user) override;

  // References to the icons that correspond to different brightness levels.
  // Used in the `QuickSettingsSlider`. Defined as a public member to be used in
  // tests.
  static constexpr const gfx::VectorIcon* kBrightnessLevelIcons[] = {
      &kUnifiedMenuBrightnessLowIcon,     // Low brightness.
      &kUnifiedMenuBrightnessMediumIcon,  // Medium brightness.
      &kUnifiedMenuBrightnessHighIcon,    // High brightness.
  };

  // The maximum index of `kBrightnessLevelIcons`.
  static constexpr int kBrightnessLevels = std::size(kBrightnessLevelIcons) - 1;

  IconButton* more_button() { return more_button_; }

  IconButton* night_light_button() { return night_light_button_; }

 private:
  friend class UnifiedBrightnessViewTest;

  // Get vector icon reference that corresponds to the given brightness level.
  // `level` is between 0.0 to 1.0.
  const gfx::VectorIcon& GetBrightnessIconForLevel(float level);

  // Callback called when `night_light_button_` is pressed.
  void OnNightLightButtonPressed();

  // Updates the icon and tooltip of `night_light_button_`.
  void UpdateNightLightButton();

  // UnifiedSliderView::
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  scoped_refptr<UnifiedSystemTrayModel> model_;
  const raw_ptr<NightLightControllerImpl> night_light_controller_;
  // Owned by the views hierarchy.
  raw_ptr<IconButton> night_light_button_ = nullptr;
  raw_ptr<IconButton> more_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_
