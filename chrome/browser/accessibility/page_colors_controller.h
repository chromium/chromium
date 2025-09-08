// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_CONTROLLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/native_theme/native_theme.h"

class PrefRegistrySimple;
class PrefService;

// Manages the page colors feature, which allows overriding the web theme's
// forced colors.
class PageColorsController : public KeyedService,
                             public ui::NativeThemeObserver {
 public:
  explicit PageColorsController(PrefService* profile_prefs);
  PageColorsController(const PageColorsController&) = delete;
  PageColorsController& operator=(const PageColorsController&) = delete;
  ~PageColorsController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // ui::NativeThemeObserver:
  void OnPreferredContrastChanged(ui::NativeTheme* observed_theme) override;

 private:
  // Handles when page colors preferences change.
  void OnPageColorsChanged();

  // This function makes use of different states such as the kPageColors,
  // kApplyPageColorsOnlyOnIncreasedContrast and OS increased contrast state to
  // calculate the used page colors.
  ui::NativeTheme::PageColors CalculatePageColors(
      const ui::NativeTheme& native_theme);

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observation_{this};
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<PrefService> profile_prefs_;
  base::WeakPtrFactory<PageColorsController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_CONTROLLER_H_
