// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_backlight_toggle_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_slider_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/raw_ptr.h"
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
      UnifiedSystemTrayModel* model)
      : UnifiedSliderView(views::Button::PressedCallback(),
                          controller,
                          kUnifiedMenuKeyboardBrightnessIcon,
                          IDS_ASH_STATUS_TRAY_BRIGHTNESS,
                          true /* readonly*/),
        model_(model) {
    model_->AddObserver(this);

    toast_label_ = AddChildView(std::make_unique<views::Label>());
    toast_label_->SetEnabledColorId(kColorAshTextColorPrimary);
    TrayPopupUtils::SetLabelFontList(toast_label_,
                                     TrayPopupUtils::FontStyle::kPodMenuHeader);
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
  const raw_ptr<UnifiedSystemTrayModel, ExperimentalAsh> model_;
  raw_ptr<views::Label, ExperimentalAsh> toast_label_ = nullptr;
};

}  // namespace

KeyboardBacklightToggleController::KeyboardBacklightToggleController(
    UnifiedSystemTrayModel* model)
    : model_(model) {}

KeyboardBacklightToggleController::~KeyboardBacklightToggleController() =
    default;

std::unique_ptr<UnifiedSliderView>
KeyboardBacklightToggleController::CreateView() {
  DCHECK(!slider_);
  auto slider =
      std::make_unique<UnifiedKeyboardBacklightToggleView>(this, model_);
  slider_ = slider.get();
  return slider;
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
