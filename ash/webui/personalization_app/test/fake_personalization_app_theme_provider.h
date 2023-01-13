// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_THEME_PROVIDER_H_
#define ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_THEME_PROVIDER_H_

#include "ash/webui/personalization_app/personalization_app_theme_provider.h"

#include <stdint.h>

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash::personalization_app {

class FakePersonalizationAppThemeProvider
    : public PersonalizationAppThemeProvider {
 public:
  explicit FakePersonalizationAppThemeProvider(content::WebUI* web_ui);

  FakePersonalizationAppThemeProvider(
      const FakePersonalizationAppThemeProvider&) = delete;
  FakePersonalizationAppThemeProvider& operator=(
      const FakePersonalizationAppThemeProvider&) = delete;

  ~FakePersonalizationAppThemeProvider() override;

  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::ThemeProvider>
          receiver) override;

  void SetThemeObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::ThemeObserver>
          observer) override;

  void SetColorModePref(bool dark_mode_enabled) override;

  void SetColorModeAutoScheduleEnabled(bool enabled) override;

  void SetColorScheme(ash::ColorScheme color_scheme) override;

  void SetStaticColor(::SkColor static_color) override;

  void GetColorScheme(GetColorSchemeCallback callback) override;

  void GetStaticColor(GetStaticColorCallback callback) override;

  void GenerateSampleColorSchemes(
      GenerateSampleColorSchemesCallback callback) override;

  void IsDarkModeEnabled(IsDarkModeEnabledCallback callback) override;

  void IsColorModeAutoScheduleEnabled(
      IsColorModeAutoScheduleEnabledCallback callback) override;

 private:
  mojo::Receiver<ash::personalization_app::mojom::ThemeProvider>
      theme_receiver_{this};
};

}  // namespace ash::personalization_app

#endif  // ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_THEME_PROVIDER_H_
