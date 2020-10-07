// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include <math.h>

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_features.h"
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

// TODO(minch): Let colors can be live updated on color mode/theme changes.
// Restart the chrome browser to let the color mode/theme changes take effect.
void AttemptRestartChrome() {
  Shell::Get()->session_controller()->AttemptRestartChrome();
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
  registry->RegisterBooleanPref(
      prefs::kColorModeThemed, kDefaultColorModeThemed,
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
  pref_change_registrar_->Add(
      prefs::kColorModeThemed,
      base::BindRepeating(&AshColorProvider::NotifyColorModeThemedPrefChange,
                          base::Unretained(this)));

  // Immediately tell all the observers to load this user's saved preferences.
  NotifyDarkModeEnabledPrefChange();
  NotifyColorModeThemedPrefChange();
}

SkColor AshColorProvider::GetShieldLayerColor(ShieldLayerType type) const {
  constexpr int kAlphas[] = {kAlpha20, kAlpha40, kAlpha60, kAlpha80, kAlpha90};
  DCHECK_LT(static_cast<size_t>(type), base::size(kAlphas));
  return SkColorSetA(GetBackgroundColor(), kAlphas[static_cast<int>(type)]);
}

SkColor AshColorProvider::GetBaseLayerColor(BaseLayerType type) const {
  constexpr int kAlphas[] = {kAlpha20, kAlpha40, kAlpha60,
                             kAlpha80, kAlpha90, 0xFF};
  DCHECK_LT(static_cast<size_t>(type), base::size(kAlphas));
  return SkColorSetA(GetBackgroundColor(), kAlphas[static_cast<int>(type)]);
}

SkColor AshColorProvider::GetControlsLayerColor(ControlsLayerType type) const {
  constexpr SkColor kLightColors[] = {SkColorSetA(SK_ColorBLACK, 0x24),
                                      gfx::kGoogleBlue600,
                                      SkColorSetA(SK_ColorBLACK, 0x0D),
                                      gfx::kGoogleRed600,
                                      gfx::kGoogleYellow600,
                                      gfx::kGoogleGreen600,
                                      SkColorSetA(gfx::kGoogleBlue600, 0x3D),
                                      gfx::kGoogleBlue600};
  constexpr SkColor kDarkColors[] = {SkColorSetA(SK_ColorWHITE, 0x24),
                                     gfx::kGoogleBlue300,
                                     SkColorSetA(SK_ColorWHITE, 0x1A),
                                     gfx::kGoogleRed300,
                                     gfx::kGoogleYellow300,
                                     gfx::kGoogleGreen300,
                                     SkColorSetA(gfx::kGoogleBlue300, 0x3D),
                                     gfx::kGoogleBlue300};
  DCHECK(base::size(kLightColors) == base::size(kDarkColors));
  static_assert(
      base::size(kLightColors) == base::size(kDarkColors),
      "Size of kLightColors should equal to the size of kDarkColors.");
  const size_t index = static_cast<size_t>(type);
  DCHECK_LT(index, base::size(kLightColors));
  return IsDarkModeEnabled() ? kDarkColors[index] : kLightColors[index];
}

SkColor AshColorProvider::GetContentLayerColor(ContentLayerType type) const {
  const bool is_dark_mode = IsDarkModeEnabled();
  switch (type) {
    case ContentLayerType::kSeparatorColor:
      return is_dark_mode ? SkColorSetA(SK_ColorWHITE, 0x24)
                          : SkColorSetA(SK_ColorBLACK, 0x24);
    case ContentLayerType::kTextColorPrimary:
      return cros_colors::ResolveColor(ColorName::kTextColorPrimary,
                                       is_dark_mode);
    case ContentLayerType::kTextColorSecondary:
      return cros_colors::ResolveColor(ColorName::kTextColorSecondary,
                                       is_dark_mode);
    case ContentLayerType::kTextColorAlert:
      return cros_colors::ResolveColor(ColorName::kTextColorAlert,
                                       is_dark_mode);
    case ContentLayerType::kTextColorWarning:
      return cros_colors::ResolveColor(ColorName::kTextColorWarning,
                                       is_dark_mode);
    case ContentLayerType::kTextColorPositive:
      return cros_colors::ResolveColor(ColorName::kTextColorPositive,
                                       is_dark_mode);
    case ContentLayerType::kIconColorPrimary:
      return cros_colors::ResolveColor(ColorName::kIconColorPrimary,
                                       is_dark_mode);
    case ContentLayerType::kIconColorSecondary:
      return gfx::kGoogleGrey500;
    case ContentLayerType::kIconColorAlert:
      return cros_colors::ResolveColor(ColorName::kIconColorAlert,
                                       is_dark_mode);
    case ContentLayerType::kIconColorWarning:
      return cros_colors::ResolveColor(ColorName::kIconColorWarning,
                                       is_dark_mode);
    case ContentLayerType::kIconColorPositive:
      return cros_colors::ResolveColor(ColorName::kIconColorPositive,
                                       is_dark_mode);
    case ContentLayerType::kIconColorProminent:
    case ContentLayerType::kSliderThumbColorEnabled:
      return cros_colors::ResolveColor(ColorName::kIconColorProminent,
                                       is_dark_mode);
    case ContentLayerType::kButtonLabelColor:
    case ContentLayerType::kButtonIconColor:
      return is_dark_mode ? gfx::kGoogleGrey200 : gfx::kGoogleGrey700;
    case ContentLayerType::kButtonLabelColorBlue:
      return is_dark_mode ? gfx::kGoogleBlue300 : gfx::kGoogleBlue600;
    case ContentLayerType::kButtonLabelColorPrimary:
    case ContentLayerType::kButtonIconColorPrimary:
      return is_dark_mode ? gfx::kGoogleGrey900 : gfx::kGoogleGrey200;
    case ContentLayerType::kSliderThumbColorDisabled:
      return is_dark_mode ? gfx::kGoogleGrey600 : gfx::kGoogleGrey600;
    case ContentLayerType::kAppStateIndicatorColor:
      return is_dark_mode ? gfx::kGoogleGrey200 : gfx::kGoogleGrey700;
    case ContentLayerType::kAppStateIndicatorColorInactive:
      return GetDisabledColor(
          GetContentLayerColor(ContentLayerType::kAppStateIndicatorColor));
    case ContentLayerType::kShelfHandleColor: {
      return is_dark_mode ? SkColorSetA(SK_ColorWHITE, 0x24)
                          : SkColorSetA(SK_ColorBLACK, 0x24);
    }
  }
}

AshColorProvider::RippleAttributes AshColorProvider::GetRippleAttributes(
    SkColor bg_color) const {
  if (bg_color == gfx::kPlaceholderColor)
    bg_color = GetBackgroundColor();

  const bool is_dark = color_utils::IsDark(bg_color);
  const SkColor base_color = is_dark ? SK_ColorWHITE : SK_ColorBLACK;
  const float opacity =
      is_dark ? kLightInkRippleOpacity : kDarkInkRippleOpacity;
  return RippleAttributes(base_color, opacity, opacity);
}

SkColor AshColorProvider::GetBackgroundColor() const {
  return IsThemed() ? GetBackgroundThemedColor() : GetBackgroundDefaultColor();
}

void AshColorProvider::DecoratePillButton(views::LabelButton* button,
                                          ButtonType type,
                                          const gfx::VectorIcon& icon) {
  DCHECK_EQ(ButtonType::kPillButtonWithIcon, type);
  DCHECK(!icon.is_empty());
  SkColor enabled_icon_color =
      GetContentLayerColor(ContentLayerType::kButtonIconColor);
  button->SetImage(views::Button::STATE_NORMAL,
                   gfx::CreateVectorIcon(icon, enabled_icon_color));
  button->SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(icon, GetDisabledColor(enabled_icon_color)));

  SkColor enabled_text_color =
      GetContentLayerColor(ContentLayerType::kButtonLabelColor);
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
                                           int button_size,
                                           const gfx::VectorIcon& icon) {
  DCHECK_EQ(ButtonType::kCloseButtonWithSmallBase, type);
  DCHECK(!icon.is_empty());
  SkColor enabled_icon_color =
      GetContentLayerColor(ContentLayerType::kButtonIconColor);
  button->SetImage(views::Button::STATE_NORMAL,
                   gfx::CreateVectorIcon(icon, enabled_icon_color));

  // Add a rounded rect background. The rounding will be half the button size so
  // it is a circle.
  SkColor icon_background_color = AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
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
  if (!features::IsDarkLightModeEnabled() && override_light_mode_as_default_)
    return false;

  if (!active_user_pref_service_)
    return kDefaultDarkModeEnabled;
  return active_user_pref_service_->GetBoolean(prefs::kDarkModeEnabled);
}

bool AshColorProvider::IsThemed() const {
  if (!active_user_pref_service_)
    return kDefaultColorModeThemed;
  return active_user_pref_service_->GetBoolean(prefs::kColorModeThemed);
}

void AshColorProvider::ToggleColorMode() {
  DCHECK(active_user_pref_service_);
  active_user_pref_service_->SetBoolean(prefs::kDarkModeEnabled,
                                        !IsDarkModeEnabled());
  active_user_pref_service_->CommitPendingWrite();

  AttemptRestartChrome();
}

void AshColorProvider::UpdateColorModeThemed(bool is_themed) {
  if (is_themed == IsThemed())
    return;

  DCHECK(active_user_pref_service_);
  active_user_pref_service_->SetBoolean(prefs::kColorModeThemed, is_themed);
  active_user_pref_service_->CommitPendingWrite();

  AttemptRestartChrome();
}

SkColor AshColorProvider::GetLoginBackgroundBaseColor() const {
  return IsDarkModeEnabled() ? login_constants::kDefaultBaseColor
                             : login_constants::kLightModeBaseColor;
}

SkColor AshColorProvider::GetBackgroundDefaultColor() const {
  return IsDarkModeEnabled() ? kBackgroundColorDefaultDark
                             : kBackgroundColorDefaultLight;
}

SkColor AshColorProvider::GetBackgroundThemedColor() const {
  const SkColor default_color = GetBackgroundDefaultColor();
  WallpaperControllerImpl* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  if (!wallpaper_controller)
    return default_color;

  const bool is_dark_mode = IsDarkModeEnabled();
  color_utils::LumaRange luma_range = is_dark_mode
                                          ? color_utils::LumaRange::DARK
                                          : color_utils::LumaRange::LIGHT;
  SkColor muted_color =
      wallpaper_controller->GetProminentColor(color_utils::ColorProfile(
          luma_range, color_utils::SaturationRange::MUTED));
  if (muted_color == kInvalidWallpaperColor)
    return default_color;

  return color_utils::GetResultingPaintColor(
      SkColorSetA(is_dark_mode ? SK_ColorBLACK : SK_ColorWHITE,
                  is_dark_mode ? kDarkBackgroundBlendAlpha
                               : kLightBackgroundBlendAlpha),
      muted_color);
}

void AshColorProvider::NotifyDarkModeEnabledPrefChange() {
  const bool is_enabled = IsDarkModeEnabled();
  for (auto& observer : observers_)
    observer.OnColorModeChanged(is_enabled);
}

void AshColorProvider::NotifyColorModeThemedPrefChange() {
  const bool is_themed = IsThemed();
  for (auto& observer : observers_)
    observer.OnColorModeThemed(is_themed);
}

}  // namespace ash
