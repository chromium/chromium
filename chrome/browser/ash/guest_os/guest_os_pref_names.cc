// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace guest_os::prefs {

// Dictionary of filesystem paths mapped to the list of VMs that the paths are
// shared with.
const char kGuestOSPathsSharedToVms[] = "guest_os.paths_shared_to_vms";

// Mapping of file extension to mime types for each VM/Container.
// Use of root 'crostini' is for historic reasons / backwards compatibility.
const char kGuestOsMimeTypes[] = "crostini.mime_types";

// Registry of installed app for each VM/container.
const char kGuestOsRegistry[] = "crostini.registry";

// GuestOsRegistry and GuestId
const char kVmTypeKey[] = "vm_type";
const char kVmNameKey[] = "vm_name";
const char kContainerNameKey[] = "container_name";

// Keys for the |kGuestOsRegistry| Dictionary stored in prefs for each app.
const char kAppDesktopFileIdKey[] = "desktop_file_id";
const char kAppCommentKey[] = "comment";
const char kAppExtensionsKey[] = "extensions";
const char kAppMimeTypesKey[] = "mime_types";
const char kAppKeywordsKey[] = "keywords";
const char kAppExecKey[] = "exec";
const char kAppExecutableFileNameKey[] = "executable_file_name";
const char kAppNameKey[] = "name";
const char kAppNoDisplayKey[] = "no_display";
const char kAppTerminalKey[] = "terminal";
const char kAppScaledKey[] = "scaled";
const char kAppPackageIdKey[] = "package_id";
const char kAppStartupWMClassKey[] = "startup_wm_class";
const char kAppStartupNotifyKey[] = "startup_notify";
const char kAppInstallTimeKey[] = "install_time";
const char kAppLastLaunchTimeKey[] = "last_launch_time";

// GuestId
const char kGuestOsContainers[] = "crostini.containers";
const char kContainerCreateOptions[] = "crostini_create_options";
const char kContainerOsVersionKey[] = "container_os_version";
const char kContainerOsPrettyNameKey[] = "container_os_pretty_name";
// SkColor used to assign badges to apps associated with this container.
const char kContainerColorKey[] = "badge_color";
const char kTerminalSupportedKey[] = "terminal_supported";
const char kTerminalLabel[] = "terminal_label";
const char kContainerSharedVmDevicesKey[] = "container_shared_vm_devices";
const char kBruschettaConfigId[] = "bruschetta_config_id";

// Terminal
const char kGuestOsTerminalSettings[] = "crostini.terminal_settings";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kGuestOSPathsSharedToVms);
  registry->RegisterDictionaryPref(kGuestOsMimeTypes);
  registry->RegisterDictionaryPref(kGuestOsRegistry);
  registry->RegisterListPref(kGuestOsContainers);
  registry->RegisterDictionaryPref(
      kGuestOsTerminalSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

}  // namespace guest_os::prefs
