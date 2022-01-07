// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/chrome_personalization_app_theme_provider.h"

#include "ash/style/ash_color_provider.h"
#include "chrome/browser/profiles/profile.h"

ChromePersonalizationAppThemeProvider::ChromePersonalizationAppThemeProvider(
    content::WebUI* web_ui)
    : web_ui_(web_ui), profile_(Profile::FromWebUI(web_ui_)) {}

ChromePersonalizationAppThemeProvider::
    ~ChromePersonalizationAppThemeProvider() = default;

void ChromePersonalizationAppThemeProvider::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::ThemeProvider>
        receiver) {
  theme_receiver_.reset();
  theme_receiver_.Bind(std::move(receiver));
}

void ChromePersonalizationAppThemeProvider::SetThemeObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::ThemeObserver>
        observer) {
  // May already be bound if user refreshes page.
  theme_observer_remote_.reset();
  theme_observer_remote_.Bind(std::move(observer));
  if (!color_mode_observer_.IsObserving())
    color_mode_observer_.Observe(ash::AshColorProvider::Get());
  // Call it once to get the current color mode.
  OnColorModeChanged(ash::ColorProvider::Get()->IsDarkModeEnabled());
}

void ChromePersonalizationAppThemeProvider::OnColorModeChanged(
    bool dark_mode_enabled) {
  DCHECK(theme_observer_remote_.is_bound());
  theme_observer_remote_->OnColorModeChanged(dark_mode_enabled);
}

void ChromePersonalizationAppThemeProvider::SetColorModePref(
    bool dark_mode_enabled) {
  auto* color_provider = ash::AshColorProvider::Get();
  if (color_provider->IsDarkModeEnabled() != dark_mode_enabled)
    color_provider->ToggleColorMode();
}
