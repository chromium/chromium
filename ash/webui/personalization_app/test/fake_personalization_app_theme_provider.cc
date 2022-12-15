// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/test/fake_personalization_app_theme_provider.h"
#include "fake_personalization_app_theme_provider.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash::personalization_app {

FakePersonalizationAppThemeProvider::FakePersonalizationAppThemeProvider(
    content::WebUI* web_ui) {}

FakePersonalizationAppThemeProvider::~FakePersonalizationAppThemeProvider() =
    default;

void FakePersonalizationAppThemeProvider::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::ThemeProvider>
        receiver) {
  theme_receiver_.reset();
  theme_receiver_.Bind(std::move(receiver));
}

void FakePersonalizationAppThemeProvider::SetThemeObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::ThemeObserver>
        observer) {}

void FakePersonalizationAppThemeProvider::SetColorModePref(
    bool dark_mode_enabled) {
  return;
}

void FakePersonalizationAppThemeProvider::SetColorModeAutoScheduleEnabled(
    bool enabled) {
  return;
}

void FakePersonalizationAppThemeProvider::IsDarkModeEnabled(
    IsDarkModeEnabledCallback callback) {
  std::move(callback).Run(/*darkModeEnabled=*/false);
}

void FakePersonalizationAppThemeProvider::IsColorModeAutoScheduleEnabled(
    IsColorModeAutoScheduleEnabledCallback callback) {
  std::move(callback).Run(/*enabled=*/false);
}

void FakePersonalizationAppThemeProvider::SetColorScheme(
    ash::ColorScheme color_scheme) {
  return;
}

void FakePersonalizationAppThemeProvider::SetStaticColor(
    ::SkColor static_color) {
  return;
}

void FakePersonalizationAppThemeProvider::GetColorScheme(
    GetColorSchemeCallback callback) {
  std::move(callback).Run(ash::ColorScheme::kTonalSpot);
}

void FakePersonalizationAppThemeProvider::GetStaticColor(
    GetStaticColorCallback callback) {
  std::move(callback).Run(SK_ColorBLUE);
}
}  // namespace ash::personalization_app
