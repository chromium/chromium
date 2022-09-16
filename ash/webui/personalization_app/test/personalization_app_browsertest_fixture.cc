// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/test/personalization_app_browsertest_fixture.h"

#include <memory>

#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_ambient_provider.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_keyboard_backlight_provider.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_theme_provider.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_user_provider.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_wallpaper_provider.h"
#include "chrome/test/base/mojo_web_ui_browser_test.h"

namespace ash::personalization_app {

std::unique_ptr<content::WebUIController>
TestPersonalizationAppWebUIProvider::NewWebUI(content::WebUI* web_ui,
                                              const GURL& url) {
  auto ambient_provider =
      std::make_unique<FakePersonalizationAppAmbientProvider>(web_ui);
  auto keyboard_backlight_provider =
      std::make_unique<FakePersonalizationAppKeyboardBacklightProvider>(web_ui);
  auto theme_provider =
      std::make_unique<FakePersonalizationAppThemeProvider>(web_ui);
  auto wallpaper_provider =
      std::make_unique<FakePersonalizationAppWallpaperProvider>(web_ui);
  auto user_provider =
      std::make_unique<FakePersonalizationAppUserProvider>(web_ui);
  return std::make_unique<PersonalizationAppUI>(
      web_ui, std::move(ambient_provider),
      std::move(keyboard_backlight_provider), std::move(theme_provider),
      std::move(user_provider), std::move(wallpaper_provider));
}

void PersonalizationAppBrowserTestFixture::SetUpOnMainThread() {
  MojoWebUIBrowserTest::SetUpOnMainThread();
  test_factory_.AddFactoryOverride(kChromeUIPersonalizationAppHost,
                                   &test_web_ui_provider_);
}

}  // namespace ash::personalization_app
