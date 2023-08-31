// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/quickoffice/quickoffice_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace quickoffice {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      quickoffice::kQuickOfficeForceFileDownloadEnabled, true);
}

}  // namespace quickoffice
