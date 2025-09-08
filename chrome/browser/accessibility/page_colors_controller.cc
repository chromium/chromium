// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/page_colors_controller.h"

#include "base/containers/fixed_flat_map.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"
#endif

namespace {

static constexpr char kPageColors[] = "settings.a11y.page_colors";
static constexpr char kIsDefaultPageColorsOnHighContrast[] =
    "settings.a11y.is_default_page_colors_on_high_contrast";

ui::NativeTheme* GetNativeTheme() {
#if BUILDFLAG(IS_LINUX)
  // Allow the Linux native theme to update its state for page colors.
  if (auto* linux_ui_theme = ui::GetDefaultLinuxUiTheme()) {
    if (auto* linux_native_theme = linux_ui_theme->GetNativeTheme()) {
      return linux_native_theme;
    }
  }
#endif
  return ui::NativeTheme::GetInstanceForNativeUi();
}

}  // namespace

PageColorsController::PageColorsController(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  theme_observation_.Observe(GetNativeTheme());

  pref_change_registrar_.Init(profile_prefs_);
  pref_change_registrar_.Add(
      prefs::kRequestedPageColors,
      base::BindRepeating(&PageColorsController::RecomputePageColors,
                          weak_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast,
      base::BindRepeating(&PageColorsController::RecomputePageColors,
                          weak_factory_.GetWeakPtr()));

  RecomputePageColors();
}

PageColorsController::~PageColorsController() = default;

// static
void PageColorsController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kRequestedPageColors,
                                base::to_underlying(PageColors::kNoPreference));
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
                                base::to_underlying(requested_page_colors));
    }
  }
  profile_prefs->ClearPref(kPageColors);
  profile_prefs->ClearPref(kIsDefaultPageColorsOnHighContrast);
}

void PageColorsController::OnPreferredContrastChanged(
    ui::NativeTheme* observed_theme) {
  RecomputePageColors();
}

void PageColorsController::SetRequestedPageColors(PageColors page_colors) {
  // Setting the pref will automatically trigger `RecomputePageColors()`.
  profile_prefs_->SetInteger(prefs::kRequestedPageColors,
                             base::to_underlying(page_colors));
}

void PageColorsController::RecomputePageColors() {
  auto* native_theme = GetNativeTheme();

  ui::NativeTheme::PageColors previous_page_colors =
      native_theme->GetPageColors();
  ui::NativeTheme::PageColors current_page_colors =
      CalculatePageColors(*native_theme);

  if (previous_page_colors == current_page_colors) {
    return;
  }

  native_theme->set_page_colors(current_page_colors);
  native_theme->NotifyOnNativeThemeUpdated();
}

ui::NativeTheme::PageColors PageColorsController::CalculatePageColors(
    const ui::NativeTheme& native_theme) {
  const int pref_value =
      profile_prefs_->GetInteger(prefs::kRequestedPageColors);
  PageColors page_colors =
      (pref_value < 0 ||
       pref_value > base::to_underlying(PageColors::kMaxValue))
          ? PageColors::kNoPreference
          : static_cast<PageColors>(pref_value);

  bool only_on_increased_contrast = profile_prefs_->GetBoolean(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast);

  // The used value of Page Colors should be `kNoPreference` if
  // kApplyPageColorsOnlyOnIncreasedContrast is true and the OS is not in an
  // increased contrast mode.
  if (only_on_increased_contrast &&
      native_theme.GetPreferredContrast() !=
          ui::NativeTheme::PreferredContrast::kMore) {
    page_colors = PageColors::kNoPreference;
  }

  auto used_page_colors = ui::NativeTheme::PageColors::kOff;
  if (page_colors != PageColors::kNoPreference) {
    static constexpr auto kColorMap =
        base::MakeFixedFlatMap<PageColors, ui::NativeTheme::PageColors>(
            {{PageColors::kOff, ui::NativeTheme::PageColors::kOff},
             {PageColors::kDusk, ui::NativeTheme::PageColors::kDusk},
             {PageColors::kDesert, ui::NativeTheme::PageColors::kDesert},
             {PageColors::kNightSky, ui::NativeTheme::PageColors::kNightSky},
             {PageColors::kAquatic, ui::NativeTheme::PageColors::kAquatic},
             {PageColors::kWhite, ui::NativeTheme::PageColors::kWhite}});
    used_page_colors = kColorMap.at(page_colors);
  } else if (native_theme.InForcedColorsMode()) {
    used_page_colors = ui::NativeTheme::PageColors::kHighContrast;
  }
  return used_page_colors;
}
