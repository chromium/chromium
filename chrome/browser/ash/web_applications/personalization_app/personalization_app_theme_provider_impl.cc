// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_theme_provider_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/style/color_palette_controller.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace ash::personalization_app {

PersonalizationAppThemeProviderImpl::PersonalizationAppThemeProviderImpl(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  if (ash::features::IsJellyEnabled()) {
    color_palette_controller_ = ColorPaletteController::Create();
  }
}

PersonalizationAppThemeProviderImpl::~PersonalizationAppThemeProviderImpl() =
    default;

void PersonalizationAppThemeProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::ThemeProvider>
        receiver) {
  theme_receiver_.reset();
  theme_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppThemeProviderImpl::SetThemeObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::ThemeObserver>
        observer) {
  // May already be bound if user refreshes page.
  theme_observer_remote_.reset();
  theme_observer_remote_.Bind(std::move(observer));
  if (!color_mode_observer_.IsObserving())
    color_mode_observer_.Observe(ash::DarkLightModeControllerImpl::Get());
  // Call it once to get the current color mode.
  OnColorModeChanged(
      ash::DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());

  // Listen to |ash::prefs::kDarkModeScheduleType| changes.
  if (!pref_change_registrar_.IsObserved(ash::prefs::kDarkModeScheduleType)) {
    pref_change_registrar_.Add(
        ash::prefs::kDarkModeScheduleType,
        base::BindRepeating(&PersonalizationAppThemeProviderImpl::
                                NotifyColorModeAutoScheduleChanged,
                            base::Unretained(this)));
  }
  // Call once to get the initial status.
  NotifyColorModeAutoScheduleChanged();
  if (ash::features::IsJellyEnabled()) {
    // TODO(b/261505637): Observe changes to the color prefs.
    OnStaticColorChanged(color_palette_controller_->static_color());
    OnColorSchemeChanged(color_palette_controller_->color_scheme());
  }
}

void PersonalizationAppThemeProviderImpl::SetColorModePref(
    bool dark_mode_enabled) {
  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  if (dark_light_mode_controller->IsDarkModeEnabled() != dark_mode_enabled) {
    LogPersonalizationTheme(dark_mode_enabled ? ColorMode::kDark
                                              : ColorMode::kLight);
    dark_light_mode_controller->ToggleColorMode();
  }
}

void PersonalizationAppThemeProviderImpl::SetColorModeAutoScheduleEnabled(
    bool enabled) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  if (enabled)
    LogPersonalizationTheme(ColorMode::kAuto);
  const ScheduleType schedule_type =
      enabled ? ScheduleType::kSunsetToSunrise : ScheduleType::kNone;
  pref_service->SetInteger(ash::prefs::kDarkModeScheduleType,
                           static_cast<int>(schedule_type));
}

void PersonalizationAppThemeProviderImpl::IsDarkModeEnabled(
    IsDarkModeEnabledCallback callback) {
  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  std::move(callback).Run(dark_light_mode_controller->IsDarkModeEnabled());
}

void PersonalizationAppThemeProviderImpl::IsColorModeAutoScheduleEnabled(
    IsColorModeAutoScheduleEnabledCallback callback) {
  std::move(callback).Run(IsColorModeAutoScheduleEnabled());
}

void PersonalizationAppThemeProviderImpl::OnColorModeChanged(
    bool dark_mode_enabled) {
  DCHECK(theme_observer_remote_.is_bound());
  theme_observer_remote_->OnColorModeChanged(dark_mode_enabled);
}

void PersonalizationAppThemeProviderImpl::OnColorSchemeChanged(
    ColorScheme color_scheme) {
  DCHECK(theme_observer_remote_.is_bound());
  theme_observer_remote_->OnColorSchemeChanged(color_scheme);
}

void PersonalizationAppThemeProviderImpl::OnStaticColorChanged(
    absl::optional<SkColor> color) {
  DCHECK(theme_observer_remote_.is_bound());
  theme_observer_remote_->OnStaticColorChanged(color);
}

bool PersonalizationAppThemeProviderImpl::IsColorModeAutoScheduleEnabled() {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  const auto schedule_type = static_cast<ScheduleType>(
      pref_service->GetInteger(ash::prefs::kDarkModeScheduleType));
  return schedule_type == ScheduleType::kSunsetToSunrise;
}

void PersonalizationAppThemeProviderImpl::NotifyColorModeAutoScheduleChanged() {
  DCHECK(theme_observer_remote_.is_bound());
  theme_observer_remote_->OnColorModeAutoScheduleChanged(
      IsColorModeAutoScheduleEnabled());
}

void PersonalizationAppThemeProviderImpl::GetColorScheme(
    GetColorSchemeCallback callback) {
  if (!ash::features::IsJellyEnabled()) {
    theme_receiver_.ReportBadMessage(
        "Cannot call GetColorScheme without Jelly enabled.");
    return;
  }
  std::move(callback).Run(color_palette_controller_->color_scheme());
}

void PersonalizationAppThemeProviderImpl::SetColorScheme(
    ColorScheme color_scheme) {
  if (!ash::features::IsJellyEnabled()) {
    theme_receiver_.ReportBadMessage(
        "Cannot call SetColorScheme without Jelly enabled.");
    return;
  }
  color_palette_controller_->SetColorScheme(color_scheme, base::DoNothing());
  OnColorSchemeChanged(color_scheme);
}

void PersonalizationAppThemeProviderImpl::GetStaticColor(
    GetStaticColorCallback callback) {
  if (!ash::features::IsJellyEnabled()) {
    theme_receiver_.ReportBadMessage(
        "Cannot call GetStaticColor without Jelly enabled.");
    return;
  }
  std::move(callback).Run(color_palette_controller_->static_color());
}

void PersonalizationAppThemeProviderImpl::SetStaticColor(SkColor static_color) {
  if (!ash::features::IsJellyEnabled()) {
    theme_receiver_.ReportBadMessage(
        "Cannot call SetStaticColor without Jelly enabled.");
    return;
  }
  color_palette_controller_->SetStaticColor(static_color, base::DoNothing());
  // TODO(b/261505637): Remove and use pref listeners once the prefs are
  // available.
  OnStaticColorChanged(static_color);
  OnColorSchemeChanged(color_palette_controller_->color_scheme());
}
}  // namespace ash::personalization_app
