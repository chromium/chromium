// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace guest_os::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kGuestOSPathsSharedToVms);
  registry->RegisterDictionaryPref(kGuestOsMimeTypes);
  registry->RegisterDictionaryPref(kGuestOsRegistry);
  registry->RegisterListPref(kGuestOsContainers);
  registry->RegisterDictionaryPref(
      kGuestOsTerminalSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(kGuestOsUSBNotificationEnabled, true);
  registry->RegisterBooleanPref(kGuestOsUSBPersistentPassthroughEnabled, true);
  registry->RegisterDictionaryPref(kGuestOsUSBPersistentPassthroughDevices);
}

}  // namespace guest_os::prefs
