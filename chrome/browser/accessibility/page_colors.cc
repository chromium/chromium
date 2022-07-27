// Copyright 2022 The Chromium Authors. All rights reserved.
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
    : profile_prefs_(profile_prefs) {}
PageColors::~PageColors() = default;

// static
void PageColors::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kPageColors, ui::NativeTheme::PageColors::kOff,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void PageColors::Init() {
  pref_change_registrar_.Init(profile_prefs_);
  pref_change_registrar_.Add(
      prefs::kPageColors, base::BindRepeating(&PageColors::OnPageColorsChanged,
                                              weak_factory_.GetWeakPtr()));
}

void PageColors::OnPageColorsChanged() {
  ui::NativeTheme::PageColors page_colors =
      static_cast<ui::NativeTheme::PageColors>(
          profile_prefs_->GetInteger(prefs::kPageColors));
  // Validating the page colors variable.
  DCHECK_GE(page_colors, ui::NativeTheme::PageColors::kOff);
  DCHECK_LE(page_colors, ui::NativeTheme::PageColors::kMaxValue);

  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  native_theme->set_page_colors(page_colors);
}