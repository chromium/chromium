// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/unified_brightness_view.h"

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

// The maximum index of `kBrightnessLevelIcons`.
const int kBrightnessLevels =
    std::size(UnifiedBrightnessView::kBrightnessLevelIcons) - 1;

// Get vector icon reference that corresponds to the given brightness level.
// `level` is between 0.0 to 1.0.
const gfx::VectorIcon& GetBrightnessIconForLevel(float level) {
  int index = static_cast<int>(std::ceil(level * kBrightnessLevels));
  DCHECK(index >= 0 && index <= kBrightnessLevels);
  return *UnifiedBrightnessView::kBrightnessLevelIcons[index];
}

}  // namespace

UnifiedBrightnessView::UnifiedBrightnessView(
    UnifiedBrightnessSliderController* controller,
    scoped_refptr<UnifiedSystemTrayModel> model)
    : UnifiedSliderView(views::Button::PressedCallback(),
                        controller,
                        kUnifiedMenuBrightnessIcon,
                        IDS_ASH_STATUS_TRAY_BRIGHTNESS),
      model_(model),
      controller_(controller) {
  if (features::IsQsRevampEnabled()) {
    AddChildView(std::make_unique<IconButton>(
        views::Button::PressedCallback(), IconButton::Type::kMedium,
        &kUnifiedMenuNightLightOffIcon,
        IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_BUTTON_LABEL,
        /*is_togglable=*/true,
        /*has_border=*/true));
    AddChildView(std::make_unique<IconButton>(
        views::Button::PressedCallback(),
        features::IsQsRevampEnabled() ? IconButton::Type::kMediumFloating
                                      : IconButton::Type::kMedium,
        &kQuickSettingsRightArrowIcon,
        IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_SETTINGS_TOOLTIP));
  } else {
    button()->SetEnabled(false);
    // The button is set to disabled but wants to keep the color for an enabled
    // icon.
    button()->SetImageModel(
        views::Button::STATE_DISABLED,
        ui::ImageModel::FromVectorIcon(kUnifiedMenuBrightnessIcon,
                                       kColorAshButtonIconColor));
  }

  model_->AddObserver(this);
  OnDisplayBrightnessChanged(false /* by_user */);
}

UnifiedBrightnessView::~UnifiedBrightnessView() {
  model_->RemoveObserver(this);
}

void UnifiedBrightnessView::OnDisplayBrightnessChanged(bool by_user) {
  float level = model_->display_brightness();
  float slider_level = slider()->GetValue();

  // If level is less than `kMinBrightnessPercent`, use the slider value as
  // `level` so that when the slider is at 0 point, the icon for the slider is
  // `kUnifiedMenuBrightnessLowIcon`. Otherwise `level` will remain to be
  // `kMinBrightnessPercent` and the icon cannot be updated.
  if (level * 100 <= controller_->kMinBrightnessPercent) {
    level = slider_level;
  }

  if (features::IsQsRevampEnabled()) {
    slider_icon()->SetImage(ui::ImageModel::FromVectorIcon(
        GetBrightnessIconForLevel(level),
        cros_tokens::kCrosSysSystemOnPrimaryContainer, kQsSliderIconSize));
  }

  SetSliderValue(level, by_user);
}

BEGIN_METADATA(UnifiedBrightnessView, views::View)
END_METADATA

}  // namespace ash
