// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_
#define ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/unified/unified_slider_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class UnifiedBrightnessSliderController;

// View of a slider that can change display brightness. It observes current
// brightness level from UnifiedSystemTrayModel.
class ASH_EXPORT UnifiedBrightnessView
    : public UnifiedSliderView,
      public UnifiedSystemTrayModel::Observer {
 public:
  METADATA_HEADER(UnifiedBrightnessView);

  UnifiedBrightnessView(UnifiedBrightnessSliderController* controller,
                        scoped_refptr<UnifiedSystemTrayModel> model);

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

 private:
  scoped_refptr<UnifiedSystemTrayModel> model_;
  UnifiedBrightnessSliderController* const controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_
