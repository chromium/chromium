// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_theme_util.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif

namespace glic {

SkColor GetGlicBackgroundColor(Profile* profile,
                               const ui::ColorProvider& color_provider) {
#if !BUILDFLAG(IS_ANDROID)
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile);
  bool use_dark = theme_service->BrowserUsesDarkColors();

  ui::ColorProviderKey key;
  key.color_mode = use_dark ? ui::ColorProviderKey::ColorMode::kDark
                            : ui::ColorProviderKey::ColorMode::kLight;

  const ui::ColorProvider* explicit_color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(key);

  return explicit_color_provider->GetColor(kColorGlicBackground);
#else
  return color_provider.GetColor(kColorGlicBackground);
#endif
}

}  // namespace glic
