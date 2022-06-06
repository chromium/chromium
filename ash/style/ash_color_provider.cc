// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include <math.h>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login_status.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_mode_controller.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/known_user.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

namespace ash {

using ColorName = cros_styles::ColorName;

namespace {

// Opacity of the light/dark indrop.
constexpr float kLightInkDropOpacity = 0.08f;
constexpr float kDarkInkDropOpacity = 0.06f;

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
constexpr int kAlpha95 = 242;  // 95%

// Alpha value that is used to calculate themed color. Please see function
// GetBackgroundThemedColor() about how the themed color is calculated.
constexpr int kDarkBackgroundBlendAlpha = 127;   // 50%
constexpr int kLightBackgroundBlendAlpha = 127;  // 50%

// An array of OOBE screens which currently support dark theme.
// In the future additional screens will be added. Eventually all screens
// will support it and this array will not be needed anymore.
constexpr OobeDialogState kStatesSupportingDarkTheme[] = {
    OobeDialogState::MARKETING_OPT_IN, OobeDialogState::THEME_SELECTION};

AshColorProvider* g_instance = nullptr;

// Get the corresponding ColorName for |type|. ColorName is an enum in
// cros_styles.h file that is generated from cros_colors.json5, which
// includes the color IDs and colors that will be used by ChromeOS WebUI.
ColorName TypeToColorName(AshColorProvider::ContentLayerType type) {
  switch (type) {
    case AshColorProvider::ContentLayerType::kTextColorPrimary:
      return ColorName::kTextColorPrimary;
    case AshColorProvider::ContentLayerType::kTextColorSecondary:
      return ColorName::kTextColorSecondary;
    case AshColorProvider::ContentLayerType::kTextColorAlert:
      return ColorName::kTextColorAlert;
    case AshColorProvider::ContentLayerType::kTextColorWarning:
      return ColorName::kTextColorWarning;
    case AshColorProvider::ContentLayerType::kTextColorPositive:
      return ColorName::kTextColorPositive;
    case AshColorProvider::ContentLayerType::kIconColorPrimary:
      return ColorName::kIconColorPrimary;
    case AshColorProvider::ContentLayerType::kIconColorAlert:
      return ColorName::kIconColorAlert;
    case AshColorProvider::ContentLayerType::kIconColorWarning:
      return ColorName::kIconColorWarning;
    case AshColorProvider::ContentLayerType::kIconColorPositive:
      return ColorName::kIconColorPositive;
    default:
      DCHECK_EQ(AshColorProvider::ContentLayerType::kIconColorProminent, type);
      return ColorName::kIconColorProminent;
  }
}

// Get the color from cros_styles.h header file that is generated from
// cros_colors.json5. Colors there will also be used by ChromeOS WebUI.
SkColor ResolveColor(AshColorProvider::ContentLayerType type,
                     bool use_dark_color) {
  return cros_styles::ResolveColor(TypeToColorName(type), use_dark_color);
}

// Refresh colors of the system on the current color mode. Not only the SysUI,
// but also all the other components like WebUI. And since AshColorProvider is
// kind of NativeTheme of ChromeOS. This will trigger View::OnThemeChanged to
// live update the colors. The colors live update can happen when color mode
// changes or wallpaper changes. It is needed when wallpaper changes as the
// background color is calculated from current wallpaper.
void RefreshColorsOnColorMode(bool is_dark_mode_enabled) {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  native_theme->set_use_dark_colors(is_dark_mode_enabled);
  native_theme->NotifyOnNativeThemeUpdated();

  auto* native_theme_web = ui::NativeTheme::GetInstanceForWeb();
  native_theme_web->set_preferred_color_scheme(
      is_dark_mode_enabled ? ui::NativeTheme::PreferredColorScheme::kDark
                           : ui::NativeTheme::PreferredColorScheme::kLight);
  native_theme_web->NotifyOnNativeThemeUpdated();
}

}  // namespace

AshColorProvider::AshColorProvider() {
  DCHECK(!g_instance);
  g_instance = this;

  // May be null in unit tests.
  if (Shell::HasInstance()) {
    auto* shell = Shell::Get();
    shell->session_controller()->AddObserver(this);
    shell->login_screen_controller()->data_dispatcher()->AddObserver(this);
    shell->wallpaper_controller()->AddObserver(this);
  }

  cros_styles::SetDebugColorsEnabled(base::FeatureList::IsEnabled(
      ash::features::kSemanticColorsDebugOverride));
}

AshColorProvider::~AshColorProvider() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;

  // May be null in unit tests.
  if (Shell::HasInstance()) {
    auto* shell = Shell::Get();
    shell->session_controller()->RemoveObserver(this);
    auto* login_screen_controller = shell->login_screen_controller();
    auto* data_dispatcher = login_screen_controller
                                ? login_screen_controller->data_dispatcher()
                                : nullptr;
    if (data_dispatcher)
      data_dispatcher->RemoveObserver(this);

    shell->wallpaper_controller()->RemoveObserver(this);
  }

  cros_styles::SetDebugColorsEnabled(false);
  cros_styles::SetDarkModeEnabled(false);
}

// static
AshColorProvider* AshColorProvider::Get() {
  return g_instance;
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

// TODO(minch): Moving prefs related logic to DarkModeController instead. To
// keep AshColorProvider only a provider of colors. This will benefit its
// migration to ui/color/color_provider as well (crbug/1292244).
// static
void AshColorProvider::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDarkModeEnabled,
                                kDefaultDarkModeEnabled);
}

void AshColorProvider::OnActiveUserPrefServiceChanged(PrefService* prefs) {
  if (!features::IsDarkLightModeEnabled())
    return;

  active_user_pref_service_ = prefs;
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  pref_change_registrar_->Add(
      prefs::kDarkModeEnabled,
      base::BindRepeating(&AshColorProvider::NotifyColorModeChanges,
                          base::Unretained(this)));

  // Immediately tell all the observers to load this user's saved preferences.
  NotifyColorModeChanges();
}

void AshColorProvider::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (!features::IsDarkLightModeEnabled())
    return;
  if (state != session_manager::SessionState::OOBE &&
      state != session_manager::SessionState::LOGIN_PRIMARY) {
    oobe_state_ = OobeDialogState::HIDDEN;
  }
  RefreshColorsOnColorMode(IsDarkModeEnabled());
}

SkColor AshColorProvider::GetShieldLayerColor(ShieldLayerType type) const {
  return GetShieldLayerColorImpl(type, /*inverted=*/false);
}

SkColor AshColorProvider::GetBaseLayerColor(BaseLayerType type) const {
  return GetBaseLayerColorImpl(type, /*inverted=*/false);
}

SkColor AshColorProvider::GetControlsLayerColor(ControlsLayerType type) const {
  return GetControlsLayerColorImpl(type, IsDarkModeEnabled());
}

SkColor AshColorProvider::GetContentLayerColor(ContentLayerType type) const {
  return GetContentLayerColorImpl(type, IsDarkModeEnabled());
}

SkColor AshColorProvider::GetActiveDialogTitleBarColor() const {
  return cros_styles::ResolveColor(cros_styles::ColorName::kDialogTitleBarColor,
                                   IsDarkModeEnabled());
}

SkColor AshColorProvider::GetInactiveDialogTitleBarColor() const {
  // TODO(wenbojie): Use a different inactive color in future.
  return GetActiveDialogTitleBarColor();
}

std::pair<SkColor, float> AshColorProvider::GetInkDropBaseColorAndOpacity(
    SkColor background_color) const {
  if (background_color == gfx::kPlaceholderColor)
    background_color = GetBackgroundColor();

  const bool is_dark = color_utils::IsDark(background_color);
  const SkColor base_color = is_dark ? SK_ColorWHITE : SK_ColorBLACK;
  const float opacity = is_dark ? kLightInkDropOpacity : kDarkInkDropOpacity;
  return std::make_pair(base_color, opacity);
}

std::pair<SkColor, float>
AshColorProvider::GetInvertedInkDropBaseColorAndOpacity(
    SkColor background_color) const {
  if (background_color == gfx::kPlaceholderColor)
    background_color = GetBackgroundColor();

  const bool is_light = !color_utils::IsDark(background_color);
  const SkColor base_color = is_light ? SK_ColorWHITE : SK_ColorBLACK;
  const float opacity = is_light ? kLightInkDropOpacity : kDarkInkDropOpacity;
  return std::make_pair(base_color, opacity);
}

SkColor AshColorProvider::GetInvertedShieldLayerColor(
    ShieldLayerType type) const {
  return GetShieldLayerColorImpl(type, /*inverted=*/true);
}

SkColor AshColorProvider::GetInvertedBaseLayerColor(BaseLayerType type) const {
  return GetBaseLayerColorImpl(type, /*inverted=*/true);
}

SkColor AshColorProvider::GetInvertedControlsLayerColor(
    ControlsLayerType type) const {
  return GetControlsLayerColorImpl(type, !IsDarkModeEnabled());
}

SkColor AshColorProvider::GetInvertedContentLayerColor(
    ContentLayerType type) const {
  return GetContentLayerColorImpl(type, !IsDarkModeEnabled());
}

SkColor AshColorProvider::GetBackgroundColor() const {
  return GetBackgroundThemedColorImpl(GetBackgroundDefaultColor(),
                                      IsDarkModeEnabled());
}

SkColor AshColorProvider::GetInvertedBackgroundColor() const {
  return GetBackgroundThemedColorImpl(GetInvertedBackgroundDefaultColor(),
                                      !IsDarkModeEnabled());
}

SkColor AshColorProvider::GetBackgroundColorInMode(bool use_dark_color) const {
  return cros_styles::ResolveColor(cros_styles::ColorName::kBgColor,
                                   use_dark_color);
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

  if (features::IsDarkLightModeEnabled()) {
    if (is_dark_mode_enabled_in_oobe_for_testing_.has_value())
      return is_dark_mode_enabled_in_oobe_for_testing_.value();

    if (oobe_state_ != OobeDialogState::HIDDEN) {
      if (active_user_pref_service_) {
        const PrefService::Preference* pref =
            active_user_pref_service_->FindPreference(
                prefs::kDarkModeScheduleType);
        // Managed users do not see the theme selection screen, so to avoid
        // confusion they should always see light colors during OOBE
        if (pref->IsManaged() || pref->IsRecommended())
          return false;

        if (!active_user_pref_service_->GetBoolean(prefs::kDarkModeEnabled))
          return false;
      }
      return base::Contains(kStatesSupportingDarkTheme, oobe_state_);
    }

    // On the login screen use the preference of the focused pod's user if they
    // had the preference stored in the known_user and the pod is focused.
    if (!active_user_pref_service_ &&
        is_dark_mode_enabled_for_focused_pod_.has_value()) {
      return is_dark_mode_enabled_for_focused_pod_.value();
    }
  }

  // Keep the color mode as DARK in login screen or when dark/light mode feature
  // is not enabled.
  if (!active_user_pref_service_ || !features::IsDarkLightModeEnabled())
    return true;

  return active_user_pref_service_->GetBoolean(prefs::kDarkModeEnabled);
}

void AshColorProvider::SetDarkModeEnabledForTest(bool enabled) {
  DCHECK(features::IsDarkLightModeEnabled());
  if (oobe_state_ != OobeDialogState::HIDDEN) {
    auto closure = GetNotifyOnDarkModeChangeClosure();
    is_dark_mode_enabled_in_oobe_for_testing_ = enabled;
    return;
  }
  if (IsDarkModeEnabled() != enabled) {
    ToggleColorMode();
  }
}

void AshColorProvider::OnOobeDialogStateChanged(OobeDialogState state) {
  auto closure = GetNotifyOnDarkModeChangeClosure();
  oobe_state_ = state;
}

void AshColorProvider::OnFocusPod(const AccountId& account_id) {
  auto closure = GetNotifyOnDarkModeChangeClosure();

  if (!account_id.is_valid()) {
    is_dark_mode_enabled_for_focused_pod_.reset();
    return;
  }
  is_dark_mode_enabled_for_focused_pod_ =
      user_manager::KnownUser(ash::Shell::Get()->local_state())
          .FindBoolPath(account_id, prefs::kDarkModeEnabled);
}

void AshColorProvider::OnWallpaperColorsChanged() {
  if (!features::IsDarkLightModeEnabled())
    return;

  RefreshColorsOnColorMode(IsDarkModeEnabled());
}

void AshColorProvider::ToggleColorMode() {
  DCHECK(active_user_pref_service_);
  active_user_pref_service_->SetBoolean(prefs::kDarkModeEnabled,
                                        !IsDarkModeEnabled());
  active_user_pref_service_->CommitPendingWrite();
  NotifyColorModeChanges();

  DarkModeController::Get()->ToggledByUser();
}

SkColor AshColorProvider::GetShieldLayerColorImpl(ShieldLayerType type,
                                                  bool inverted) const {
  constexpr int kAlphas[] = {kAlpha20, kAlpha40, kAlpha60,
                             kAlpha80, kAlpha90, kAlpha95};
  DCHECK_LT(static_cast<size_t>(type), std::size(kAlphas));
  return SkColorSetA(
      inverted ? GetInvertedBackgroundColor() : GetBackgroundColor(),
      kAlphas[static_cast<int>(type)]);
}

SkColor AshColorProvider::GetBaseLayerColorImpl(BaseLayerType type,
                                                bool inverted) const {
  constexpr int kAlphas[] = {kAlpha20, kAlpha40, kAlpha60, kAlpha80,
                             kAlpha90, kAlpha95, 0xFF};
  DCHECK_LT(static_cast<size_t>(type), std::size(kAlphas));
  return SkColorSetA(
      inverted ? GetInvertedBackgroundColor() : GetBackgroundColor(),
      kAlphas[static_cast<int>(type)]);
}

SkColor AshColorProvider::GetControlsLayerColorImpl(ControlsLayerType type,
                                                    bool use_dark_color) const {
  switch (type) {
    case ControlsLayerType::kHairlineBorderColor:
      return use_dark_color ? SkColorSetA(SK_ColorWHITE, 0x24)
                            : SkColorSetA(SK_ColorBLACK, 0x24);
    case ControlsLayerType::kControlBackgroundColorActive:
      return use_dark_color ? gfx::kGoogleBlue300 : gfx::kGoogleBlue600;
    case ControlsLayerType::kControlBackgroundColorInactive:
      return use_dark_color ? SkColorSetA(SK_ColorWHITE, 0x1A)
                            : SkColorSetA(SK_ColorBLACK, 0x0D);
    case ControlsLayerType::kControlBackgroundColorAlert:
      return use_dark_color ? gfx::kGoogleRed300 : gfx::kGoogleRed600;
    case ControlsLayerType::kControlBackgroundColorWarning:
      return use_dark_color ? gfx::kGoogleYellow300 : gfx::kGoogleYellow600;
    case ControlsLayerType::kControlBackgroundColorPositive:
      return use_dark_color ? gfx::kGoogleGreen300 : gfx::kGoogleGreen600;
    case ControlsLayerType::kFocusAuraColor:
      return use_dark_color ? SkColorSetA(gfx::kGoogleBlue300, 0x3D)
                            : SkColorSetA(gfx::kGoogleBlue600, 0x3D);
    case ControlsLayerType::kFocusRingColor:
      return use_dark_color ? gfx::kGoogleBlue300 : gfx::kGoogleBlue600;
    case ControlsLayerType::kHighlightColor1:
      return use_dark_color ? SkColorSetA(SK_ColorWHITE, 0x14)
                            : SkColorSetA(SK_ColorWHITE, 0x4C);
    case ControlsLayerType::kBorderColor1:
      return use_dark_color ? GetBaseLayerColor(BaseLayerType::kTransparent80)
                            : SkColorSetA(SK_ColorBLACK, 0x0F);
    case ControlsLayerType::kHighlightColor2:
      return use_dark_color ? SkColorSetA(SK_ColorWHITE, 0x0F)
                            : SkColorSetA(SK_ColorWHITE, 0x33);
    case ControlsLayerType::kBorderColor2:
      return use_dark_color ? GetBaseLayerColor(BaseLayerType::kTransparent60)
                            : SkColorSetA(SK_ColorBLACK, 0x0F);
  }
}

SkColor AshColorProvider::GetContentLayerColorImpl(ContentLayerType type,
                                                   bool use_dark_color) const {
  switch (type) {
    case ContentLayerType::kSeparatorColor:
    case ContentLayerType::kShelfHandleColor:
      return use_dark_color ? SkColorSetA(SK_ColorWHITE, 0x24)
                            : SkColorSetA(SK_ColorBLACK, 0x24);
    case ContentLayerType::kIconColorSecondary:
      return gfx::kGoogleGrey500;
    case ContentLayerType::kIconColorSecondaryBackground:
      return use_dark_color ? gfx::kGoogleGrey100 : gfx::kGoogleGrey800;
    case ContentLayerType::kScrollBarColor:
    case ContentLayerType::kSliderColorInactive:
    case ContentLayerType::kRadioColorInactive:
      return use_dark_color ? gfx::kGoogleGrey200 : gfx::kGoogleGrey700;
    case ContentLayerType::kSwitchKnobColorInactive:
      return use_dark_color ? gfx::kGoogleGrey400 : SK_ColorWHITE;
    case ContentLayerType::kSwitchTrackColorInactive:
      return GetSecondToneColor(use_dark_color ? gfx::kGoogleGrey200
                                               : gfx::kGoogleGrey700);
    case ContentLayerType::kButtonLabelColorBlue:
    case ContentLayerType::kTextColorURL:
    case ContentLayerType::kSliderColorActive:
    case ContentLayerType::kRadioColorActive:
    case ContentLayerType::kSwitchKnobColorActive:
    case ContentLayerType::kProgressBarColorForeground:
      return use_dark_color ? gfx::kGoogleBlue300 : gfx::kGoogleBlue600;
    case ContentLayerType::kProgressBarColorBackground:
    case ContentLayerType::kCaptureRegionColor:
      return SkColorSetA(
          use_dark_color ? gfx::kGoogleBlue300 : gfx::kGoogleBlue600, 0x4C);
    case ContentLayerType::kSwitchTrackColorActive:
      return GetSecondToneColor(GetContentLayerColorImpl(
          ContentLayerType::kSwitchKnobColorActive, use_dark_color));
    case ContentLayerType::kButtonLabelColorPrimary:
    case ContentLayerType::kButtonIconColorPrimary:
    case ContentLayerType::kBatteryBadgeColor:
      return use_dark_color ? gfx::kGoogleGrey900 : gfx::kGoogleGrey200;
    case ContentLayerType::kAppStateIndicatorColorInactive:
      return GetDisabledColor(GetContentLayerColorImpl(
          ContentLayerType::kAppStateIndicatorColor, use_dark_color));
    case ContentLayerType::kCurrentDeskColor:
      return use_dark_color ? SK_ColorWHITE : SK_ColorBLACK;
    case ContentLayerType::kSwitchAccessInnerStrokeColor:
      return gfx::kGoogleBlue300;
    case ContentLayerType::kSwitchAccessOuterStrokeColor:
      return gfx::kGoogleBlue900;
    case ContentLayerType::kHighlightColorHover:
      return use_dark_color ? SkColorSetA(SK_ColorWHITE, 0x0D)
                            : SkColorSetA(SK_ColorBLACK, 0x14);
    case ContentLayerType::kAppStateIndicatorColor:
    case ContentLayerType::kButtonIconColor:
    case ContentLayerType::kButtonLabelColor:
      return use_dark_color ? gfx::kGoogleGrey200 : gfx::kGoogleGrey900;
    case ContentLayerType::kBatterySystemInfoBackgroundColor:
      return use_dark_color ? gfx::kGoogleGreen300 : gfx::kGoogleGreen600;
    case ContentLayerType::kBatterySystemInfoIconColor:
      return use_dark_color ? gfx::kGoogleGrey900 : gfx::kGoogleGrey200;
    default:
      return ResolveColor(type, use_dark_color);
  }
}

SkColor AshColorProvider::GetBackgroundDefaultColor() const {
  return GetBackgroundColorInMode(IsDarkModeEnabled());
}

SkColor AshColorProvider::GetInvertedBackgroundDefaultColor() const {
  return GetBackgroundColorInMode(!IsDarkModeEnabled());
}

SkColor AshColorProvider::GetBackgroundThemedColorImpl(
    SkColor default_color,
    bool use_dark_color) const {
  // May be null in unit tests.
  if (!Shell::HasInstance())
    return default_color;
  WallpaperControllerImpl* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  if (!wallpaper_controller)
    return default_color;

  color_utils::LumaRange luma_range = use_dark_color
                                          ? color_utils::LumaRange::DARK
                                          : color_utils::LumaRange::LIGHT;
  SkColor muted_color =
      wallpaper_controller->GetProminentColor(color_utils::ColorProfile(
          luma_range, color_utils::SaturationRange::MUTED));
  if (muted_color == kInvalidWallpaperColor)
    return default_color;

  return color_utils::GetResultingPaintColor(
      SkColorSetA(use_dark_color ? SK_ColorBLACK : SK_ColorWHITE,
                  use_dark_color ? kDarkBackgroundBlendAlpha
                                 : kLightBackgroundBlendAlpha),
      muted_color);
}

void AshColorProvider::NotifyColorModeChanges() {
  const bool is_enabled = IsDarkModeEnabled();
  cros_styles::SetDarkModeEnabled(is_enabled);
  for (auto& observer : observers_)
    observer.OnColorModeChanged(is_enabled);

  RefreshColorsOnColorMode(IsDarkModeEnabled());
}

base::ScopedClosureRunner AshColorProvider::GetNotifyOnDarkModeChangeClosure() {
  return base::ScopedClosureRunner(
      // Unretained is safe here because GetNotifyOnDarkModeChangeClosure is a
      // private function and callback should be called on going out of scope of
      // the calling method.
      base::BindOnce(&AshColorProvider::NotifyIfDarkModeChanged,
                     base::Unretained(this), IsDarkModeEnabled()));
}

void AshColorProvider::NotifyIfDarkModeChanged(bool old_is_dark_mode_enabled) {
  if (old_is_dark_mode_enabled == IsDarkModeEnabled())
    return;
  NotifyColorModeChanges();
}

}  // namespace ash
