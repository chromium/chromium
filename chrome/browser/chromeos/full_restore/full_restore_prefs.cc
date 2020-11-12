// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_prefs.h"

#include "ash/public/cpp/ash_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace full_restore {

// An integer pref to define whether restore apps and web pages on startup.
// Refer to |RestoreOption|.
const char kRestoreAppsAndPagesPrefName[] = "settings.restore_apps_and_pages";

// TODO(https://crbug.com/1131969): Add the restore user preference init, and
// modificaiton implementation.

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  if (ash::features::IsFullRestoreEnabled()) {
    registry->RegisterIntegerPref(
        kRestoreAppsAndPagesPrefName, static_cast<int>(RestoreOption::kAlways),
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  }
}

}  // namespace full_restore
}  // namespace chromeos
