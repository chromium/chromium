// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_PROVIDER_IMPL_H_

#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/mojom/color_scheme.mojom-shared.h"
#include "ash/webui/personalization_app/personalization_app_theme_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_source_observer.h"

class Profile;

namespace ash {
class ColorPaletteController;
}  // namespace ash

namespace content {
class WebUI;
}  // namespace content

namespace ash::personalization_app {

class PersonalizationAppThemeProviderImpl
    : public PersonalizationAppThemeProvider,
      public ash::ColorModeObserver,
      public ui::ColorProviderSourceObserver,
      public ash::system::TimezoneSettings::Observer {
 public:
  explicit PersonalizationAppThemeProviderImpl(content::WebUI* web_ui);

  PersonalizationAppThemeProviderImpl(
      const PersonalizationAppThemeProviderImpl&) = delete;
  PersonalizationAppThemeProviderImpl& operator=(
      const PersonalizationAppThemeProviderImpl&) = delete;

  ~PersonalizationAppThemeProviderImpl() override;

  // PersonalizationAppThemeProvider:
  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::ThemeProvider>
          receiver) override;

  // ash::personalization_app::mojom::ThemeProvider:
  void SetThemeObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::ThemeObserver>
          observer) override;

  void SetColorModePref(bool dark_mode_enabled) override;

  void SetColorModeAutoScheduleEnabled(bool enabled) override;

  void IsDarkModeEnabled(IsDarkModeEnabledCallback callback) override;

  void SetColorScheme(ash::style::mojom::ColorScheme color_scheme) override;

  void SetStaticColor(SkColor static_color) override;

  void EnableGeolocationForSystemServices() override;

  void IsColorModeAutoScheduleEnabled(
      IsColorModeAutoScheduleEnabledCallback callback) override;

  void IsGeolocationEnabledForSystemServices(
      IsGeolocationEnabledForSystemServicesCallback callback) override;
  void IsGeolocationUserModifiable(
      IsGeolocationUserModifiableCallback callback) override;

  // ash::ColorModeObserver:
  void OnColorModeChanged(bool dark_mode_enabled) override;

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override;

  void GetColorScheme(GetColorSchemeCallback callback) override;

  void GetStaticColor(GetStaticColorCallback callback) override;

  void GenerateSampleColorSchemes(
      GenerateSampleColorSchemesCallback callback) override;

  // ash::system::TimezoneSettings::Observer
  void TimezoneChanged(const icu::TimeZone& timezone) override;

 private:
  bool IsColorModeAutoScheduleEnabled();

  // Notify webUI the current state of color mode auto scheduler.
  void NotifyColorModeAutoScheduleChanged();

  bool IsGeolocationEnabledForSystemServices();
  bool IsGeolocationUserModifiable();

  // Notify webUI the current state of system geolocation permission. Needed for
  // the color mode auto scheduler.
  void NotifyGeolocationPermissionChanged();

  void OnColorSchemeChanged();

  void OnSampleColorSchemesChanged(
      const std::vector<ash::SampleColorScheme>& sampleColorSchemes);

  void OnStaticColorChanged();

  // Pointer to profile of user that opened personalization SWA. Not owned.
  raw_ptr<Profile> const profile_ = nullptr;

  PrefChangeRegistrar pref_change_registrar_;

  raw_ptr<ColorPaletteController> color_palette_controller_ =
      nullptr;  // owned by Shell

  base::ScopedObservation<ash::DarkLightModeControllerImpl,
                          ash::ColorModeObserver>
      color_mode_observer_{this};

  mojo::Receiver<ash::personalization_app::mojom::ThemeProvider>
      theme_receiver_{this};

  // The ColorProviderSourceObserver notifies whenever the ColorProvider is
  // updated, such as when dark light mode changes or a new wallpaper is
  // added.
  base::ScopedObservation<ui::ColorProviderSource,
                          ui::ColorProviderSourceObserver>
      color_provider_source_observer_{this};

  // Timezone Settings notifies when the timezone is changed.
  base::ScopedObservation<system::TimezoneSettings,
                          system::TimezoneSettings::Observer>
      timezone_settings_observer_{this};

  mojo::Remote<ash::personalization_app::mojom::ThemeObserver>
      theme_observer_remote_;

  base::WeakPtrFactory<PersonalizationAppThemeProviderImpl> weak_factory_{this};
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_PROVIDER_IMPL_H_
