// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/prefs/ntp_prefs.h"

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ntp_prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kNtpPrevNavigationTime, base::Time());
  registry->RegisterTimePref(kNtpLastModuleStalenessUpdate, base::Time());
  registry->RegisterDictionaryPref(kNtpModuleStalenessCountDict);
  registry->RegisterDictionaryPref(kNtpModulesAutoRemovalDisabledDict);
  registry->RegisterBooleanPref(kNtpAnimatedDoodlesEnabled, true);
  registry->RegisterBooleanPref(kNtpDoodleMuralsEnabled, true);
  registry->RegisterDictionaryPref(kNtpWidgetPlatformPresets);
}

void ResetProfilePrefs(PrefService* prefs) {
  prefs->SetTime(kNtpLastModuleStalenessUpdate, base::Time());
  prefs->SetDict(kNtpModuleStalenessCountDict, base::DictValue());
  prefs->SetDict(kNtpModulesAutoRemovalDisabledDict, base::DictValue());
  prefs->SetBoolean(kNtpAnimatedDoodlesEnabled, true);
  prefs->SetBoolean(kNtpDoodleMuralsEnabled, true);
  prefs->SetDict(kNtpWidgetPlatformPresets, base::DictValue());
  prefs->SetDict(prefs::kContextMenuAnimationState, base::DictValue());
}

}  // namespace ntp_prefs
