// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_CONTROLLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/accessibility/page_colors.mojom-shared.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/native_theme/native_theme.h"

class PrefRegistrySimple;
class PrefService;

// The enum is defined in chrome/browser/accessibility/page_colors.mojom so
// that C++ and the WebUI TypeScript share a single definition. Aliased here so
// that call sites can keep using the unqualified name.
using PageColors = page_colors::mojom::PageColors;

// Manages the page colors feature, which allows overriding the web theme's
// forced colors.
class PageColorsController : public KeyedService {
 public:
  struct EffectivePageColors {
    ui::ColorProviderKey::ForcedColors forced_colors;
    ui::NativeTheme::PreferredColorScheme preferred_color_scheme;
    ui::NativeTheme::PreferredContrast preferred_contrast;
  };

  explicit PageColorsController(PrefService* profile_prefs);
  PageColorsController(const PageColorsController&) = delete;
  PageColorsController& operator=(const PageColorsController&) = delete;
  ~PageColorsController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void MigrateObsoleteProfilePrefs(PrefService* profile_prefs);
  static EffectivePageColors GetEffectivePageColors(
      const PrefService& profile_prefs);

  // Sets the profile's requested Page Colors value. The effective value is
  // resolved when each owning WebContents computes its web preferences.
  void SetRequestedPageColors(PageColors page_colors);

 private:
  void PageColorsSettingsChanged();

  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<PrefService> profile_prefs_;
  base::WeakPtrFactory<PageColorsController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_CONTROLLER_H_
