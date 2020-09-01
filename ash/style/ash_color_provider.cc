// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include <math.h>

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/dark_mode/color_mode_observer.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/chromeos/colors/cros_colors.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

using ColorName = cros_colors::ColorName;

namespace {

// Opacity of the light/dark ink ripple.
constexpr float kLightInkRippleOpacity = 0.08f;
constexpr float kDarkInkRippleOpacity = 0.06f;

// The disabled color is always 38% opacity of the enabled color.
constexpr float kDisabledColorOpacity = 0.38f;

// Color of second tone is always 30% opacity of the color of first tone.
constexpr float kSecondToneOpacity = 0.3f;

// Different alpha values that can be used by Shield and Base layers.
constexpr int kAlpha20 = 51;   // 20%
constexpr int kAlpha40 = 102;  // 40%
constexpr int kAlpha60 = 153;  // 60%
constexpr int kAlpha80 = 204;  // 80%
constexpr int kAlpha90 = 230;  // 90%

// Alpha value that is used to calculate themed color. Please see function
// GetBackgroundThemedColor() about how the themed color is calculated.
constexpr int kDarkBackgroundBlendAlpha = 127;   // 50%
constexpr int kLightBackgroundBlendAlpha = 191;  // 75%

// The default background color that can be applied on any layer.
constexpr SkColor kBackgroundColorDefaultLight = SK_ColorWHITE;
constexpr SkColor kBackgroundColorDefaultDark = gfx::kGoogleGrey900;

// The spacing between a pill button's icon and label, if it has both.
constexpr int kPillButtonImageLabelSpacingDp = 8;

bool IsLightMode(AshColorProvider::AshColorMode color_mode) {
  return color_mode == AshColorProvider::AshColorMode::kLight;
}

}  // namespace

AshColorProvider::AshColorProvider() {
  Shell::Get()->session_controller()->AddObserver(this);
}

AshColorProvider::~AshColorProvider() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
AshColorProvider* AshColorProvider::Get() {
  return Shell::Get()->ash_color_provider();
}

// static
SkColor AshColorProvider::GetDisabledColor(SkColor enabled_color) {
  return SkColorSetA(enabled_color, std::round(SkColorGetA(enabled_color) *
                                               kDisabledColorOpacity));
}

// static
SkColor AshColorProvider::GetSecondToneColor(SkColor color_of_first_tone) {
  return SkColorSetA(
      color_of_first_tone,
      std::round(SkColorGetA(color_of_first_tone) * kSecondToneOpacity));
}

// static
void AshColorProvider::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kDarkModeEnabled, kDefaultDarkModeEnabled,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void AshColorProvider::OnActiveUserPrefServiceChanged(PrefService* prefs) {
  active_user_pref_service_ = prefs;
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  pref_change_registrar_->Add(
      prefs::kDarkModeEnabled,
      base::BindRepeating(&AshColorProvider::NotifyDarkModeEnabledPrefChange,
                          base::Unretained(this)));

  // Immediately tell all the observers to load this user's saved preferences.
  NotifyDarkModeEnabledPrefChange();
}

SkColor AshColorProvider::GetLoginBackgroundBaseColor() const {
  return IsDarkModeEnabled() ? login_constants::kDefaultBaseColor
                             : login_constants::kLightModeBaseColor;
}

SkColor AshColorProvider::DeprecatedGetShieldLayerColor(
    ShieldLayerType type,
    SkColor default_color) const {
  if (color_mode_ == AshColorMode::kDefault)
    return default_color;

  return GetShieldLayerColorImpl(type, color_mode_);
}

SkColor AshColorProvider::GetShieldLayerColor(
    ShieldLayerType type,
    AshColorMode given_color_mode) const {
  AshColorMode color_mode =
      color_mode_ != AshColorMode::kDefault ? color_mode_ : given_color_mode;
  DCHECK(color_mode != AshColorMode::kDefault);
  return GetShieldLayerColorImpl(type, color_mode);
}

SkColor AshColorProvider::DeprecatedGetBaseLayerColor(
    BaseLayerType type,
    SkColor default_color) const {
  if (color_mode_ == AshColorMode::kDefault)
    return default_color;

  return GetBaseLayerColorImpl(type, color_mode_);
}

SkColor AshColorProvider::GetBaseLayerColor(
    BaseLayerType type,
    AshColorMode given_color_mode) const {
  AshColorMode color_mode =
      color_mode_ != AshColorMode::kDefault ? color_mode_ : given_color_mode;
  DCHECK(color_mode != AshColorMode::kDefault);
  return GetBaseLayerColorImpl(type, color_mode);
}

SkColor AshColorProvider::DeprecatedGetControlsLayerColor(
    ControlsLayerType type,
    SkColor default_color) const {
  if (color_mode_ == AshColorMode::kDefault)
    return default_color;

  return GetControlsLayerColorImpl(type, color_mode_);
}

SkColor AshColorProvider::GetControlsLayerColor(
    ControlsLayerType type,
    AshColorMode given_color_mode) const {
  AshColorMode color_mode =
      color_mode_ != AshColorMode::kDefault ? color_mode_ : given_color_mode;
  DCHECK(color_mode != AshColorMode::kDefault);
  return GetControlsLayerColorImpl(type, color_mode);
}

SkColor AshColorProvider::DeprecatedGetContentLayerColor(
    ContentLayerType type,
    SkColor default_color) const {
  if (color_mode_ == AshColorMode::kDefault)
    return default_color;

  return GetContentLayerColorImpl(type, color_mode_);
}

SkColor AshColorProvider::GetContentLayerColor(
    ContentLayerType type,
    AshColorMode given_color_mode) const {
  AshColorMode color_mode =
      color_mode_ != AshColorMode::kDefault ? color_mode_ : given_color_mode;
  DCHECK(color_mode != AshColorMode::kDefault);
  return GetContentLayerColorImpl(type, color_mode);
}

AshColorProvider::RippleAttributes AshColorProvider::GetRippleAttributes(
    SkColor bg_color) const {
  const bool is_dark = color_utils::IsDark(bg_color);
  const SkColor base_color = is_dark ? SK_ColorWHITE : SK_ColorBLACK;
  const float opacity =
      is_dark ? kLightInkRippleOpacity : kDarkInkRippleOpacity;
  return RippleAttributes(base_color, opacity, opacity);
}

SkColor AshColorProvider::GetBackgroundColor(AshColorMode color_mode) const {
  DCHECK(color_mode == AshColorProvider::AshColorMode::kLight ||
         color_mode == AshColorProvider::AshColorMode::kDark);
  return is_themed_ ? GetBackgroundThemedColor(color_mode)
                    : GetBackgroundDefaultColor(color_mode);
}

SkColor AshColorProvider::GetInkDropBaseColor(
    AshColorMode given_color_mode) const {
  AshColorMode color_mode =
      color_mode_ != AshColorMode::kDefault ? color_mode_ : given_color_mode;
  return color_mode == AshColorMode::kLight ? SK_ColorBLACK : SK_ColorWHITE;
}

float AshColorProvider::GetInkDropVisibleOpacity() const {
  return 0.2f;
}

void AshColorProvider::DecoratePillButton(views::LabelButton* button,
                                          ButtonType type,
                                          AshColorMode given_color_mode,
                                          const gfx::VectorIcon& icon) {
  DCHECK_EQ(ButtonType::kPillButtonWithIcon, type);
  DCHECK(!icon.is_empty());
  SkColor enabled_icon_color = GetContentLayerColor(
      ContentLayerType::kButtonIconColor, given_color_mode);
  button->SetImage(views::Button::STATE_NORMAL,
                   gfx::CreateVectorIcon(icon, enabled_icon_color));
  button->SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(icon, GetDisabledColor(enabled_icon_color)));

  SkColor enabled_text_color = GetContentLayerColor(
      ContentLayerType::kButtonLabelColor, given_color_mode);
  button->SetEnabledTextColors(enabled_text_color);
  button->SetTextColor(views::Button::STATE_DISABLED,
                       GetDisabledColor(enabled_text_color));
  button->SetImageLabelSpacing(kPillButtonImageLabelSpacingDp);

  // TODO(sammiequon): Add a default rounded rect background. It should probably
  // be optional as some buttons still require customization. At that point we
  // should package the parameters of this function into a struct.
}

void AshColorProvider::DecorateCloseButton(views::ImageButton* button,
                                           ButtonType type,
                                           AshColorMode given_color_mode,
                                           int button_size,
                                           const gfx::VectorIcon& icon) {
  DCHECK_EQ(ButtonType::kCloseButtonWithSmallBase, type);
  DCHECK(!icon.is_empty());
  SkColor enabled_icon_color = GetContentLayerColor(
      ContentLayerType::kButtonIconColor, given_color_mode);
  button->SetImage(views::Button::STATE_NORMAL,
                   gfx::CreateVectorIcon(icon, enabled_icon_color));

  // Add a rounded rect background. The rounding will be half the button size so
  // it is a circle.
  SkColor icon_background_color = AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80, given_color_mode);
  button->SetBackground(
      CreateBackgroundFromPainter(views::Painter::CreateSolidRoundRectPainter(
          icon_background_color, button_size / 2)));

  // TODO(sammiequon): Add background blur as per spec. Background blur is quite
  // heavy, and we may have many close buttons showing at a time. They'll be
  // added separately so its easier to monitor performance.
}

void AshColorProvider::AddObserver(ColorModeObserver* observer) {
  observers_.AddObserver(observer);
}

void AshColorProvider::RemoveObserver(ColorModeObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool AshColorProvider::IsDarkModeEnabled() const {
  if (!active_user_pref_service_)
    return kDefaultDarkModeEnabled;
  return active_user_pref_service_->GetBoolean(prefs::kDarkModeEnabled);
}

void AshColorProvider::Toggle() {
  DCHECK(active_user_pref_service_);
  active_user_pref_service_->SetBoolean(prefs::kDarkModeEnabled,
                                        !IsDarkModeEnabled());
  active_user_pref_service_->CommitPendingWrite();
}

SkColor AshColorProvider::GetShieldLayerColorImpl(
    ShieldLayerType type,
    AshColorMode color_mode) const {
  const int kAlphas[] = {kAlpha20, kAlpha40, kAlpha60, kAlpha80, kAlpha90};
  DCHECK_LT(static_cast<size_t>(type), base::size(kAlphas));
  return SkColorSetA(GetBackgroundColor(color_mode),
                     kAlphas[static_cast<int>(type)]);
}

SkColor AshColorProvider::GetBaseLayerColorImpl(BaseLayerType type,
                                                AshColorMode color_mode) const {
  const int kAlphas[] = {kAlpha20, kAlpha40, kAlpha60, kAlpha80,
                         kAlpha90, 0xFF,     0xFF};
  DCHECK_LT(static_cast<size_t>(type), base::size(kAlphas));
  const int transparent_alpha = kAlphas[static_cast<int>(type)];

  switch (type) {
    case BaseLayerType::kTransparent20:
    case BaseLayerType::kTransparent40:
    case BaseLayerType::kTransparent60:
    case BaseLayerType::kTransparent80:
    case BaseLayerType::kTransparent90:
      return SkColorSetA(GetBackgroundColor(color_mode), transparent_alpha);
    case BaseLayerType::kOpaque:
      return GetBackgroundColor(color_mode);
  }
}

SkColor AshColorProvider::GetControlsLayerColorImpl(
    ControlsLayerType type,
    AshColorMode color_mode) const {
  SkColor light_color, dark_color;
  switch (type) {
    case ControlsLayerType::kHairlineBorderColor:
      light_color = SkColorSetA(SK_ColorBLACK, 0x24);  // 14%
      dark_color = SkColorSetA(SK_ColorWHITE, 0x24);
      break;
    case ControlsLayerType::kControlBackgroundColorInactive:
      light_color = SkColorSetA(SK_ColorBLACK, 0x0D);  // 5%
      dark_color = SkColorSetA(SK_ColorWHITE, 0x1A);   // 10%
      break;
    case ControlsLayerType::kControlBackgroundColorActive:
    case ControlsLayerType::kFocusRingColor:
      light_color = gfx::kGoogleBlue600;
      dark_color = gfx::kGoogleBlue300;
      break;
    case ControlsLayerType::kControlBackgroundColorAlert:
      light_color = gfx::kGoogleRed600;
      dark_color = gfx::kGoogleRed300;
      break;
    case ControlsLayerType::kControlBackgroundColorWarning:
      light_color = gfx::kGoogleYellow600;
      dark_color = gfx::kGoogleYellow300;
      break;
    case ControlsLayerType::kControlBackgroundColorPositive:
      light_color = gfx::kGoogleGreen600;
      dark_color = gfx::kGoogleGreen300;
      break;
  }
  return IsLightMode(color_mode) ? light_color : dark_color;
}

SkColor AshColorProvider::GetContentLayerColorImpl(
    ContentLayerType type,
    AshColorMode color_mode) const {
  SkColor light_color, dark_color;
  switch (type) {
    case ContentLayerType::kSeparatorColor:
      light_color = SkColorSetA(SK_ColorBLACK, 0x24);  // 14%
      dark_color = SkColorSetA(SK_ColorWHITE, 0x24);
      break;
    case ContentLayerType::kTextColorPrimary:
      return cros_colors::ResolveColor(ColorName::kTextColorPrimary,
                                       color_mode);
    case ContentLayerType::kTextColorSecondary:
      return cros_colors::ResolveColor(ColorName::kTextColorSecondary,
                                       color_mode);
    case ContentLayerType::kTextColorAlert:
      return cros_colors::ResolveColor(ColorName::kTextColorAlert, color_mode);
      break;
    case ContentLayerType::kTextColorWarning:
      return cros_colors::ResolveColor(ColorName::kTextColorWarning,
                                       color_mode);
      break;
    case ContentLayerType::kTextColorPositive:
      return cros_colors::ResolveColor(ColorName::kTextColorPositive,
                                       color_mode);
      break;
    case ContentLayerType::kIconColorPrimary:
      return cros_colors::ResolveColor(ColorName::kIconColorPrimary,
                                       color_mode);
    case ContentLayerType::kIconColorSecondary:
      light_color = dark_color = gfx::kGoogleGrey500;
      break;
    case ContentLayerType::kIconColorAlert:
      return cros_colors::ResolveColor(ColorName::kIconColorAlert, color_mode);
      break;
    case ContentLayerType::kIconColorWarning:
      return cros_colors::ResolveColor(ColorName::kIconColorWarning,
                                       color_mode);
      break;
    case ContentLayerType::kIconColorPositive:
      return cros_colors::ResolveColor(ColorName::kIconColorPositive,
                                       color_mode);
      break;
    case ContentLayerType::kIconColorProminent:
    case ContentLayerType::kSliderThumbColorEnabled:
      return cros_colors::ResolveColor(ColorName::kIconColorProminent,
                                       color_mode);
    case ContentLayerType::kButtonLabelColor:
    case ContentLayerType::kButtonIconColor:
      light_color = gfx::kGoogleGrey700;
      dark_color = gfx::kGoogleGrey200;
      break;
    case ContentLayerType::kButtonLabelColorPrimary:
    case ContentLayerType::kButtonIconColorPrimary:
      light_color = gfx::kGoogleGrey900;
      dark_color = gfx::kGoogleGrey200;
      break;
    case ContentLayerType::kSliderThumbColorDisabled:
      light_color = gfx::kGoogleGrey600;
      dark_color = gfx::kGoogleGrey600;
      break;
    case ContentLayerType::kSystemMenuIconColor:
      light_color = gfx::kGoogleGrey700;
      dark_color = gfx::kGoogleGrey200;
      break;
    case ContentLayerType::kSystemMenuIconColorToggled:
      light_color = gfx::kGoogleGrey200;
      dark_color = gfx::kGoogleGrey900;
      break;
    case ContentLayerType::kAppStateIndicatorColor:
      light_color = gfx::kGoogleGrey700;
      dark_color = gfx::kGoogleGrey200;
      break;
    case ContentLayerType::kAppStateIndicatorColorInactive:
      return GetDisabledColor(GetContentLayerColorImpl(
          ContentLayerType::kAppStateIndicatorColor, color_mode));
  }
  return IsLightMode(color_mode) ? light_color : dark_color;
}

SkColor AshColorProvider::GetBackgroundDefaultColor(
    AshColorMode color_mode) const {
  DCHECK(color_mode == AshColorProvider::AshColorMode::kLight ||
         color_mode == AshColorProvider::AshColorMode::kDark);
  return IsLightMode(color_mode) ? kBackgroundColorDefaultLight
                                 : kBackgroundColorDefaultDark;
}

SkColor AshColorProvider::GetBackgroundThemedColor(
    AshColorMode color_mode) const {
  DCHECK(color_mode == AshColorProvider::AshColorMode::kLight ||
         color_mode == AshColorProvider::AshColorMode::kDark);
  const SkColor default_color = GetBackgroundDefaultColor(color_mode);
  WallpaperControllerImpl* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  if (!wallpaper_controller)
    return default_color;

  color_utils::LumaRange luma_range = IsLightMode(color_mode)
                                          ? color_utils::LumaRange::LIGHT
                                          : color_utils::LumaRange::DARK;
  SkColor muted_color =
      wallpaper_controller->GetProminentColor(color_utils::ColorProfile(
          luma_range, color_utils::SaturationRange::MUTED));
  if (muted_color == kInvalidWallpaperColor)
    return default_color;

  return color_utils::GetResultingPaintColor(
      SkColorSetA(IsLightMode(color_mode) ? SK_ColorWHITE : SK_ColorBLACK,
                  IsLightMode(color_mode) ? kLightBackgroundBlendAlpha
                                          : kDarkBackgroundBlendAlpha),
      muted_color);
}

void AshColorProvider::NotifyDarkModeEnabledPrefChange() {
  const bool is_enabled = IsDarkModeEnabled();
  for (auto& observer : observers_)
    observer.OnColorModeChanged(is_enabled);
}

}  // namespace ash
