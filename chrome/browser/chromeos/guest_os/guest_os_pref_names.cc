// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/guest_os/guest_os_pref_names.h"

#include "components/prefs/pref_registry_simple.h"

namespace guest_os {
namespace prefs {

// TODO(crbug.com/946273): Remove crostini.shared_paths and migration code after
// M77.
const char kCrostiniSharedPaths[] = "crostini.shared_paths";
// Dictionary of filesystem paths mapped to the list of VMs that the paths are
// shared with.
const char kGuestOSPathsSharedToVms[] = "guest_os.paths_shared_to_vms";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kCrostiniSharedPaths);
  registry->RegisterDictionaryPref(kGuestOSPathsSharedToVms);
}

}  // namespace prefs
}  // namespace guest_os
