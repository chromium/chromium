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
#include "ui/native_theme/native_theme_observer.h"

class PrefRegistrySimple;
class PrefService;

// Values in this enum are stored in prefs, so if you change them you may need
// to add migration code.
enum class PageColors {
  // Web theme forced colors state and CSS color values match the UI theme.
  kNoPreference = 0,

  // Web theme disables forced colors regardless of UI theme. The web theme
  // preferred color scheme tracks that of the UI theme.
  kOff = 1,

  // Web theme enables forces colors regardless of the UI theme. CSS color
  // values are set corresponding to one of the below themes.
  kDusk = 2,      // Mimics Win 11 "Dusk"
  kDesert = 3,    // Mimics Win 11 "Desert"
  kNightSky = 4,  // Mimics Win 11 "Night Sky"
  kAquatic = 5,   // Mimics Win 11 "Aquatic"
  kWhite = 6,     // Mimics Win 10 "High Contrast White"

  kMaxValue = kWhite,
};

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
  static void MigrateObsoleteProfilePrefs(PrefService* profile_prefs);

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // Requests that the web theme base its forced colors on `page_colors`,
  // subject to relevant prefs and native theme state.
  void SetRequestedPageColors(PageColors page_colors);

 private:
  // Updates the web theme's forced colors and other state based on relevant
  // prefs. If anything changed, notifies the web theme's observers.
  void RecomputePageColors();

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observation_{this};
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<PrefService> profile_prefs_;
  base::WeakPtrFactory<PageColorsController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_CONTROLLER_H_
