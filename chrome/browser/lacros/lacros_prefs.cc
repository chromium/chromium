// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_prefs.h"

#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace lacros_prefs {

const char kShowedExperimentalBannerPref[] =
    "lacros.showed_experimental_banner";

const char kPrimaryProfileFirstRunFinished[] =
    "lacros.primary_profile_first_run_finished";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kShowedExperimentalBannerPref,
                                /*default_value=*/false);
  registry->RegisterBooleanPref(kPrimaryProfileFirstRunFinished,
                                /*default_value=*/false);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Ordinarily, this preference is registered by Ash, but it is used by
  // browser settings. It could reasonably move to a browser-specific
  // location with suitable #ifdefs.
  registry->RegisterBooleanPref(::prefs::kSettingsShowOSBanner, true);

  // The following two prefs are used in imageWriterPrivate extension api
  // implementation code, which are supported in Lacros.
  registry->RegisterBooleanPref(::prefs::kExternalStorageDisabled, false);
  registry->RegisterBooleanPref(::prefs::kExternalStorageReadOnly, false);
}

}  // namespace lacros_prefs
