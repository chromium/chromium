// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_PROVIDER_IMPL_H_

#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/webui/personalization_app/personalization_app_theme_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebUI;
}  // namespace content

namespace ash {
namespace personalization_app {

class PersonalizationAppThemeProviderImpl
    : public PersonalizationAppThemeProvider,
      ash::ColorModeObserver {
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

  void IsColorModeAutoScheduleEnabled(
      IsColorModeAutoScheduleEnabledCallback callback) override;

  // ash::ColorModeObserver:
  void OnColorModeChanged(bool dark_mode_enabled) override;

 private:
  bool IsColorModeAutoScheduleEnabled();

  // Notify webUI the current state of color mode auto scheduler.
  void NotifyColorModeAutoScheduleChanged();

  // Pointer to profile of user that opened personalization SWA. Not owned.
  raw_ptr<Profile> const profile_ = nullptr;

  PrefChangeRegistrar pref_change_registrar_;

  base::ScopedObservation<ash::ColorProvider, ash::ColorModeObserver>
      color_mode_observer_{this};

  mojo::Receiver<ash::personalization_app::mojom::ThemeProvider>
      theme_receiver_{this};

  mojo::Remote<ash::personalization_app::mojom::ThemeObserver>
      theme_observer_remote_;
};

}  // namespace personalization_app
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_PROVIDER_IMPL_H_
