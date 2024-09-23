// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/unified_keyboard_brightness_slider_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/personalization_entry_point.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/rgb_keyboard/rgb_keyboard_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"

namespace ash {

namespace {

// Only applicable when rgb keyboard is supported.
const SkColor keyboardBrightnessIconBackgroundColor =
    SkColorSetRGB(138, 180, 248);

class UnifiedKeyboardBrightnessView : public UnifiedSliderView,
                                      public UnifiedSystemTrayModel::Observer {
 public:
  UnifiedKeyboardBrightnessView(
      UnifiedKeyboardBrightnessSliderController* controller,
      UnifiedSystemTrayModel* model)
      : UnifiedSliderView(views::Button::PressedCallback(),
                          controller,
                          kUnifiedMenuKeyboardBrightnessIcon,
                          IDS_ASH_STATUS_TRAY_BRIGHTNESS,
                          /*is_togglable=*/false),
        model_(model) {
    if (Shell::Get()->rgb_keyboard_manager()->IsRgbKeyboardSupported()) {
      if (button()) {
        button()->SetBackgroundColor(keyboardBrightnessIconBackgroundColor);
      }
      AddChildView(CreateKeyboardBacklightColorButton());
    }
    model_->AddObserver(this);
    OnKeyboardBrightnessChanged(
        power_manager::BacklightBrightnessChange_Cause_OTHER);
  }

  UnifiedKeyboardBrightnessView(const UnifiedKeyboardBrightnessView&) = delete;
  UnifiedKeyboardBrightnessView& operator=(
      const UnifiedKeyboardBrightnessView&) = delete;

  ~UnifiedKeyboardBrightnessView() override { model_->RemoveObserver(this); }

  // UnifiedSystemTrayModel::Observer:
  void OnKeyboardBrightnessChanged(
      power_manager::BacklightBrightnessChange_Cause cause) override {
    SetSliderValue(
        model_->keyboard_brightness(),
        cause == power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  }

 private:
  std::unique_ptr<views::ImageButton> CreateKeyboardBacklightColorButton() {
    auto button = std::make_unique<IconButton>(
        base::BindRepeating(
            &UnifiedKeyboardBrightnessView::OnKeyboardBacklightColorIconPressed,
            weak_factory_.GetWeakPtr()),
        IconButton::Type::kMedium, &kUnifiedMenuKeyboardBacklightIcon,
        IDS_ASH_STATUS_TRAY_KEYBOARD_BACKLIGHT_ACCESSIBLE_NAME);

    personalization_app::mojom::BacklightColor backlight_color =
        Shell::Get()->keyboard_backlight_color_controller()->GetBacklightColor(
            Shell::Get()->session_controller()->GetActiveAccountId());
    if (backlight_color ==
        personalization_app::mojom::BacklightColor::kRainbow) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      auto* image =
          rb.GetImageSkiaNamed(IDR_SETTINGS_RGB_KEYBOARD_RAINBOW_COLOR_48_PNG);
      button->SetBackgroundImage(*image);
      button->SetIconColor(gfx::kGoogleGrey900);
    } else {
      SkColor color =
          ConvertBacklightColorToIconBackgroundColor(backlight_color);
      button->SetBackgroundColor(color);
      button->SetIconColor(color_utils::GetLuma(color) < 125
                               ? gfx::kGoogleGrey200
                               : gfx::kGoogleGrey900);
    }
    button->SetBorder(views::CreateRoundedRectBorder(
        /*thickness=*/4, /*corner_radius=*/16,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kSeparatorColor)));
    return button;
  }

  void OnKeyboardBacklightColorIconPressed() {
    // Record entry point metric to Personalization Hub.
    base::UmaHistogramEnumeration(
        kPersonalizationEntryPointHistogramName,
        PersonalizationEntryPoint::kKeyboardBrightnessSlider);
    NewWindowDelegate* primary_delegate = NewWindowDelegate::GetPrimary();
    primary_delegate->OpenPersonalizationHub();
    return;
  }

  const raw_ptr<UnifiedSystemTrayModel> model_;

  base::WeakPtrFactory<UnifiedKeyboardBrightnessView> weak_factory_{this};
};

}  // namespace

UnifiedKeyboardBrightnessSliderController::
    UnifiedKeyboardBrightnessSliderController(UnifiedSystemTrayModel* model)
    : model_(model) {}

UnifiedKeyboardBrightnessSliderController::
    ~UnifiedKeyboardBrightnessSliderController() = default;

std::unique_ptr<UnifiedSliderView>
UnifiedKeyboardBrightnessSliderController::CreateView() {
#if DCHECK_IS_ON()
  DCHECK(!created_view_);
  created_view_ = true;
#endif
  return std::make_unique<UnifiedKeyboardBrightnessView>(this, model_);
}

QsSliderCatalogName
UnifiedKeyboardBrightnessSliderController::GetCatalogName() {
  return QsSliderCatalogName::kKeyboardBrightness;
}

void UnifiedKeyboardBrightnessSliderController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  if (reason != views::SliderChangeReason::kByUser) {
    return;
  }

  if (features::IsKeyboardBacklightControlInSettingsEnabled()) {
    KeyboardBrightnessControlDelegate* keyboard_brightness_control_delegate =
        Shell::Get()->keyboard_brightness_control_delegate();
    if (!keyboard_brightness_control_delegate) {
      return;
    }
    const double percent = value * 100;
    keyboard_brightness_control_delegate->HandleSetKeyboardBrightness(
        percent, /*gradual=*/true,
        /*source=*/KeyboardBrightnessChangeSource::kQuickSettings);
    return;
  }

  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(value * 100);
  request.set_transition(
      power_manager::SetBacklightBrightnessRequest_Transition_FAST);
  request.set_cause(
      power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
  chromeos::PowerManagerClient::Get()->SetKeyboardBrightness(
      std::move(request));
}

}  // namespace ash
