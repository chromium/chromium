// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_backlight_toggle_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_slider_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

class UnifiedKeyboardBacklightToggleView
    : public UnifiedSliderView,
      public UnifiedSystemTrayModel::Observer {
 public:
  UnifiedKeyboardBacklightToggleView(
      KeyboardBacklightToggleController* controller,
      UnifiedSystemTrayModel* model,
      bool toggled_on)
      // TODO(b/298085976): Instead of inheriting from `UnifiedSliderView`, this
      // should be a toast created through the ToastManager.
      : UnifiedSliderView(views::Button::PressedCallback(),
                          controller,
                          kUnifiedMenuKeyboardBrightnessIcon,
                          IDS_ASH_STATUS_TRAY_BRIGHTNESS,
                          /*is_togglable=*/false,
                          /*read_only=*/true),
        model_(model) {
    model_->AddObserver(this);

    icon_button_ = AddChildView(std::make_unique<IconButton>(
        views::Button::PressedCallback(), IconButton::Type::kMedium,
        /*icon=*/&kUnifiedMenuKeyboardBrightnessIcon,
        /*accessible_name_id=*/IDS_ASH_STATUS_TRAY_BRIGHTNESS,
        /*is_togglable=*/false,
        /*has_border=*/true));
    icon_button_->SetCanProcessEventsWithinSubtree(/*can_process=*/false);

    toast_label_ =
        AddChildView(std::make_unique<views::Label>(l10n_util::GetStringUTF16(
            toggled_on ? IDS_ASH_STATUS_AREA_TOAST_KBL_ON
                       : IDS_ASH_STATUS_AREA_TOAST_KBL_OFF)));
    toast_label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                          *toast_label_);
    slider()->SetVisible(false);
  }

  UnifiedKeyboardBacklightToggleView(
      const UnifiedKeyboardBacklightToggleView&) = delete;
  UnifiedKeyboardBacklightToggleView& operator=(
      const UnifiedKeyboardBacklightToggleView&) = delete;

  ~UnifiedKeyboardBacklightToggleView() override {
    model_->RemoveObserver(this);
  }

  // UnifiedSystemTrayModel::Observer:
  void OnKeyboardBrightnessChanged(
      power_manager::BacklightBrightnessChange_Cause cause) override {
    DCHECK(toast_label_);
    toast_label_->SetText(l10n_util::GetStringUTF16(
        cause == power_manager::BacklightBrightnessChange_Cause_USER_TOGGLED_OFF
            ? IDS_ASH_STATUS_AREA_TOAST_KBL_OFF
            : IDS_ASH_STATUS_AREA_TOAST_KBL_ON));
  }

 private:
  const raw_ptr<UnifiedSystemTrayModel> model_;

  // Owned by the views hierarchy.
  raw_ptr<views::Label> toast_label_ = nullptr;
  raw_ptr<IconButton> icon_button_ = nullptr;
};

}  // namespace

KeyboardBacklightToggleController::KeyboardBacklightToggleController(
    UnifiedSystemTrayModel* model,
    bool toggled_on)
    : model_(model), toggled_on_(toggled_on) {}

KeyboardBacklightToggleController::~KeyboardBacklightToggleController() =
    default;

std::unique_ptr<UnifiedSliderView>
KeyboardBacklightToggleController::CreateView() {
#if DCHECK_IS_ON()
  DCHECK(!created_view_);
  created_view_ = true;
#endif
  return std::make_unique<UnifiedKeyboardBacklightToggleView>(this, model_,
                                                              toggled_on_);
}

QsSliderCatalogName KeyboardBacklightToggleController::GetCatalogName() {
  return QsSliderCatalogName::kKeyboardBrightness;
}

void KeyboardBacklightToggleController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  NOTREACHED();
}

}  // namespace ash
