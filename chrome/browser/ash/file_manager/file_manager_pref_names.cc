// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_pref_names.h"

#include "components/prefs/pref_registry_simple.h"

namespace file_manager::prefs {

// A boolean pref to enable or disable /usr/sbin/smbfs verbose logging.
const char kSmbfsEnableVerboseLogging[] = "smbfs.enable_verbose_logging";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kSmbfsEnableVerboseLogging, false);
}

}  // namespace file_manager::prefs
