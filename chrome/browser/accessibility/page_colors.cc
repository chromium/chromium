// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/page_colors.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ui/native_theme/native_theme.h"

PageColors::PageColors(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
}
PageColors::~PageColors() = default;

// static
void PageColors::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kPageColors, ui::NativeTheme::PageColors::kOff,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if BUILDFLAG(IS_WIN)
  registry->RegisterBooleanPref(
      prefs::kIsDefaultPageColorsOnHighContrast, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // BUILDFLAG(IS_WIN)
}

void PageColors::Init() {
  pref_change_registrar_.Init(profile_prefs_);
  pref_change_registrar_.Add(
      prefs::kPageColors, base::BindRepeating(&PageColors::OnPageColorsChanged,
                                              weak_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast,
      base::BindRepeating(&PageColors::OnPageColorsChanged,
                          weak_factory_.GetWeakPtr()));
#if BUILDFLAG(IS_WIN)
  profile_prefs_->SetBoolean(prefs::kApplyPageColorsOnlyOnIncreasedContrast,
                             true);
#endif  // BUILDFLAG(IS_WIN)
  OnPageColorsChanged();
}

void PageColors::OnPageColorsChanged() {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  ui::NativeTheme::PageColors previous_page_colors =
      native_theme->GetPageColors();
  ui::NativeTheme::PageColors current_page_colors = CalculatePageColors();

  if (previous_page_colors == current_page_colors)
    return;

#if BUILDFLAG(IS_WIN)
  if (native_theme->UserHasContrastPreference()) {
    // When a user turns page colors 'Off' while high contrast is enabled at the
    // OS level, the default of Page Colors changes from 'HighContrast' to
    // 'Off'.
    profile_prefs_->SetBoolean(
        prefs::kIsDefaultPageColorsOnHighContrast,
        current_page_colors != ui::NativeTheme::PageColors::kOff);
  }
#endif  // BUILDFLAG(IS_WIN)
  native_theme->set_page_colors(current_page_colors);
}

ui::NativeTheme::PageColors PageColors::CalculatePageColors() {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  ui::NativeTheme::PageColors page_colors =
      static_cast<ui::NativeTheme::PageColors>(
          profile_prefs_->GetInteger(prefs::kPageColors));
  // Validating the page colors variable.
  DCHECK_GE(page_colors, ui::NativeTheme::PageColors::kOff);
  DCHECK_LE(page_colors, ui::NativeTheme::PageColors::kMaxValue);

  bool only_on_increased_contrast = profile_prefs_->GetBoolean(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast);

  ui::NativeTheme::PageColors used_page_colors = page_colors;
  // The used value of Page Colors should be 'Off' if
  // kApplyPageColorsOnlyOnIncreasedContrast is true and the OS is not in an
  // increased contrast mode.
  if (only_on_increased_contrast &&
      !native_theme->UserHasContrastPreference()) {
    used_page_colors = ui::NativeTheme::PageColors::kOff;
  }

  return used_page_colors;
}

void PageColors::OnPreferredContrastChanged() {
#if BUILDFLAG(IS_WIN)
  ui::NativeTheme::PageColors page_colors =
      static_cast<ui::NativeTheme::PageColors>(
          profile_prefs_->GetInteger(prefs::kPageColors));

  // Validating the page colors variable.
  DCHECK_GE(page_colors, ui::NativeTheme::PageColors::kOff);
  DCHECK_LE(page_colors, ui::NativeTheme::PageColors::kMaxValue);

  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  if (native_theme->UserHasContrastPreference()) {
    // If increased contrast just got turned on and page colors is 'Off', the
    // used value of Page Colors should be set to the default value which is
    // either 'Off' or 'High Contrast'.
    if (page_colors == ui::NativeTheme::PageColors::kOff) {
      ui::NativeTheme::PageColors default_page_colors_on_high_contrast =
          profile_prefs_->GetBoolean(prefs::kIsDefaultPageColorsOnHighContrast)
              ? ui::NativeTheme::PageColors::kHighContrast
              : ui::NativeTheme::PageColors::kOff;
      profile_prefs_->SetInteger(prefs::kPageColors,
                                 default_page_colors_on_high_contrast);
    }
  } else {
    // If increased contrast just got turned off and page colors was 'High
    // Contrast', the used value of Page Colors should be 'Off'.
    if (page_colors == ui::NativeTheme::PageColors::kHighContrast)
      profile_prefs_->SetInteger(prefs::kPageColors,
                                 ui::NativeTheme::PageColors::kOff);
  }
#endif  // BUILDFLAG(IS_WIN)
  OnPageColorsChanged();
}
