// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/page_colors_controller.h"

#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/color/color_provider_key.h"
#include "ui/native_theme/native_theme.h"

namespace {

static constexpr char kPageColors[] = "settings.a11y.page_colors";
static constexpr char kIsDefaultPageColorsOnHighContrast[] =
    "settings.a11y.is_default_page_colors_on_high_contrast";

}  // namespace

PageColorsController::PageColorsController(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  pref_change_registrar_.Init(profile_prefs_);
  pref_change_registrar_.Add(
      prefs::kRequestedPageColors,
      base::BindRepeating(&PageColorsController::PageColorsSettingsChanged,
                          weak_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast,
      base::BindRepeating(&PageColorsController::PageColorsSettingsChanged,
                          weak_factory_.GetWeakPtr()));
}

PageColorsController::~PageColorsController() = default;

// static
void PageColorsController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kRequestedPageColors,
                                std::to_underlying(PageColors::kNoPreference));
  registry->RegisterBooleanPref(prefs::kApplyPageColorsOnlyOnIncreasedContrast,
                                false);
  registry->RegisterListPref(prefs::kPageColorsBlockList);

  // Obsolete prefs for migration.
  registry->RegisterIntegerPref(kPageColors, 0);
  registry->RegisterBooleanPref(kIsDefaultPageColorsOnHighContrast, true);
}

// static
void PageColorsController::MigrateObsoleteProfilePrefs(
    PrefService* profile_prefs) {
  if (profile_prefs->HasPrefPath(kPageColors)) {
    static constexpr auto kPrefMap =
        base::MakeFixedFlatMap<int, PageColors>({{0, PageColors::kOff},
                                                 {1, PageColors::kDusk},
                                                 {2, PageColors::kDesert},
                                                 {3, PageColors::kNightSky},
                                                 {4, PageColors::kWhite},
                                                 {5, PageColors::kNoPreference},
                                                 {6, PageColors::kAquatic}});
    if (const auto it = kPrefMap.find(profile_prefs->GetInteger(kPageColors));
        it != kPrefMap.end()) {
      PageColors requested_page_colors = it->second;
      if (requested_page_colors == PageColors::kOff &&
          profile_prefs->GetBoolean(kIsDefaultPageColorsOnHighContrast)) {
        requested_page_colors = PageColors::kNoPreference;
      }
      profile_prefs->SetInteger(prefs::kRequestedPageColors,
                                std::to_underlying(requested_page_colors));
    }
  }
  profile_prefs->ClearPref(kPageColors);
  profile_prefs->ClearPref(kIsDefaultPageColorsOnHighContrast);
}

void PageColorsController::SetRequestedPageColors(PageColors page_colors) {
  // Setting the pref will automatically trigger
  // `PageColorsSettingsChanged()`.
  profile_prefs_->SetInteger(prefs::kRequestedPageColors,
                             std::to_underlying(page_colors));
}

void PageColorsController::PageColorsSettingsChanged() {
  // Page colors are profile-specific, but we use the web NativeTheme as a
  // process-wide invalidation signal rather than establishing an independent
  // path for this rare event.
  //
  // OS theme changes already notify the web NativeTheme directly, so only
  // profile pref changes need this signal.
  ui::NativeTheme::GetInstanceForWeb()->NotifyOnNativeThemeUpdated();
}

// static
PageColorsController::EffectivePageColors
PageColorsController::GetEffectivePageColors(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  const ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForWeb();
  return {.forced_colors = native_theme->forced_colors(),
          .preferred_color_scheme = native_theme->preferred_color_scheme(),
          .preferred_contrast = native_theme->preferred_contrast()};
#else
  const PrefService* profile_prefs =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs();

  // Get the current color/contrast values from the native UI theme.
  const auto* const native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  ui::ColorProviderKey::ForcedColors forced_colors =
      native_theme->forced_colors();
  ui::NativeTheme::PreferredColorScheme preferred_color_scheme =
      native_theme->preferred_color_scheme();
  ui::NativeTheme::PreferredContrast preferred_contrast =
      native_theme->preferred_contrast();

  // Get the requested page colors.
  const int pref_value = profile_prefs->GetInteger(prefs::kRequestedPageColors);
  PageColors page_colors =
      (pref_value < 0 || pref_value > std::to_underlying(PageColors::kMaxValue))
          ? PageColors::kNoPreference
          : static_cast<PageColors>(pref_value);
  if (preferred_contrast != ui::NativeTheme::PreferredContrast::kMore &&
      profile_prefs->GetBoolean(
          prefs::kApplyPageColorsOnlyOnIncreasedContrast)) {
    page_colors = PageColors::kNoPreference;
  }

  // Explicit Page Colors settings override the native theme.
  if (page_colors != PageColors::kNoPreference) {
    static constexpr auto kColorMap =
        base::MakeFixedFlatMap<PageColors, ui::ColorProviderKey::ForcedColors>(
            {{PageColors::kOff, ui::ColorProviderKey::ForcedColors::kNone},
             {PageColors::kDusk, ui::ColorProviderKey::ForcedColors::kDusk},
             {PageColors::kDesert, ui::ColorProviderKey::ForcedColors::kDesert},
             {PageColors::kNightSky,
              ui::ColorProviderKey::ForcedColors::kNightSky},
             {PageColors::kAquatic,
              ui::ColorProviderKey::ForcedColors::kAquatic},
             {PageColors::kWhite, ui::ColorProviderKey::ForcedColors::kWhite}});
    forced_colors = kColorMap.at(page_colors);
    if (page_colors == PageColors::kOff) {
      preferred_contrast = ui::NativeTheme::PreferredContrast::kNoPreference;
    } else {
      const bool is_dark_theme = page_colors == PageColors::kDusk ||
                                 page_colors == PageColors::kNightSky ||
                                 page_colors == PageColors::kAquatic;
      preferred_color_scheme =
          is_dark_theme ? ui::NativeTheme::PreferredColorScheme::kDark
                        : ui::NativeTheme::PreferredColorScheme::kLight;
      preferred_contrast = ui::NativeTheme::PreferredContrast::kMore;
    }
  }

  return {.forced_colors = forced_colors,
          .preferred_color_scheme = preferred_color_scheme,
          .preferred_contrast = preferred_contrast};
#endif
}
