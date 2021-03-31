// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"

#include "components/prefs/pref_registry_simple.h"

namespace guest_os {
namespace prefs {

// Dictionary of filesystem paths mapped to the list of VMs that the paths are
// shared with.
const char kGuestOSPathsSharedToVms[] = "guest_os.paths_shared_to_vms";

const char kGuestOsRegistry[] = "crostini.registry";
// Keys for the |kGuestOsRegistry| Dictionary stored in prefs for each app.
const char kAppDesktopFileIdKey[] = "desktop_file_id";
const char kAppVmTypeKey[] = "vm_type";
const char kAppVmNameKey[] = "vm_name";
const char kAppContainerNameKey[] = "container_name";
const char kAppCommentKey[] = "comment";
const char kAppExtensionsKey[] = "extensions";
const char kAppMimeTypesKey[] = "mime_types";
const char kAppKeywordsKey[] = "keywords";
const char kAppExecKey[] = "exec";
const char kAppExecutableFileNameKey[] = "executable_file_name";
const char kAppNameKey[] = "name";
const char kAppNoDisplayKey[] = "no_display";
const char kAppScaledKey[] = "scaled";
const char kAppPackageIdKey[] = "package_id";
const char kAppStartupWMClassKey[] = "startup_wm_class";
const char kAppStartupNotifyKey[] = "startup_notify";
const char kAppInstallTimeKey[] = "install_time";
const char kAppLastLaunchTimeKey[] = "last_launch_time";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kGuestOSPathsSharedToVms);

  registry->RegisterDictionaryPref(kGuestOsRegistry);
}

}  // namespace prefs
}  // namespace guest_os
