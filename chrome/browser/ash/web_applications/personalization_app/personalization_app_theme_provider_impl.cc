// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_theme_provider_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace personalization_app {

PersonalizationAppThemeProviderImpl::PersonalizationAppThemeProviderImpl(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      ash::prefs::kDarkModeScheduleType,
      base::BindRepeating(&PersonalizationAppThemeProviderImpl::
                              NotifyColorModeAutoScheduleChanged,
                          base::Unretained(this)));
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
    color_mode_observer_.Observe(ash::AshColorProvider::Get());
  // Call it once to get the current color mode.
  OnColorModeChanged(ash::ColorProvider::Get()->IsDarkModeEnabled());
  // Call it once to get the status of auto mode.
  NotifyColorModeAutoScheduleChanged();
}

void PersonalizationAppThemeProviderImpl::SetColorModePref(
    bool dark_mode_enabled) {
  auto* color_provider = ash::AshColorProvider::Get();
  if (color_provider->IsDarkModeEnabled() != dark_mode_enabled)
    color_provider->ToggleColorMode();
}

void PersonalizationAppThemeProviderImpl::SetColorModeAutoScheduleEnabled(
    bool enabled) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  ash::ScheduledFeature::ScheduleType schedule_type =
      enabled ? ash::ScheduledFeature::ScheduleType::kSunsetToSunrise
              : ash::ScheduledFeature::ScheduleType::kNone;
  pref_service->SetInteger(ash::prefs::kDarkModeScheduleType, schedule_type);
}

void PersonalizationAppThemeProviderImpl::IsDarkModeEnabled(
    IsDarkModeEnabledCallback callback) {
  auto* color_provider = ash::AshColorProvider::Get();
  std::move(callback).Run(color_provider->IsDarkModeEnabled());
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

bool PersonalizationAppThemeProviderImpl::IsColorModeAutoScheduleEnabled() {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  auto schedule_type = static_cast<ash::ScheduledFeature::ScheduleType>(
      pref_service->GetInteger(ash::prefs::kDarkModeScheduleType));
  return schedule_type == ash::ScheduledFeature::ScheduleType::kSunsetToSunrise;
}

void PersonalizationAppThemeProviderImpl::NotifyColorModeAutoScheduleChanged() {
  DCHECK(theme_observer_remote_.is_bound());
  theme_observer_remote_->OnColorModeAutoScheduleChanged(
      IsColorModeAutoScheduleEnabled());
}

}  // namespace personalization_app
}  // namespace ash
