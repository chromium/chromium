// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_CHROME_PERSONALIZATION_APP_THEME_PROVIDER_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_CHROME_PERSONALIZATION_APP_THEME_PROVIDER_H_

#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/webui/personalization_app/personalization_app_theme_provider.h"
#include "base/scoped_observation.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebUI;
}  // namespace content

class ChromePersonalizationAppThemeProvider
    : public ash::PersonalizationAppThemeProvider,
      ash::ColorModeObserver {
 public:
  explicit ChromePersonalizationAppThemeProvider(content::WebUI* web_ui);

  ChromePersonalizationAppThemeProvider(
      const ChromePersonalizationAppThemeProvider&) = delete;
  ChromePersonalizationAppThemeProvider& operator=(
      const ChromePersonalizationAppThemeProvider&) = delete;

  ~ChromePersonalizationAppThemeProvider() override;

  // PersonalizationAppThemeProvider:
  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::ThemeProvider>
          receiver) override;

  // ash::personalization_app::mojom::ThemeProvider:
  void SetThemeObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::ThemeObserver>
          observer) override;

  void SetColorModePref(bool dark_mode_enabled) override;

  // ash::ColorModeObserver:
  void OnColorModeChanged(bool dark_mode_enabled) override;

 private:
  content::WebUI* const web_ui_ = nullptr;

  // Pointer to profile of user that opened personalization SWA. Not owned.
  Profile* const profile_ = nullptr;

  base::ScopedObservation<ash::ColorProvider, ash::ColorModeObserver>
      color_mode_observer_{this};

  mojo::Receiver<ash::personalization_app::mojom::ThemeProvider>
      theme_receiver_{this};

  mojo::Remote<ash::personalization_app::mojom::ThemeObserver>
      theme_observer_remote_;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_CHROME_PERSONALIZATION_APP_THEME_PROVIDER_H_
