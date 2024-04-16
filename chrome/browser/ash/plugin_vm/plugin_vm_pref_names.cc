// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"

#include "components/guest_os/guest_os_prefs.h"
#include "components/prefs/pref_registry_simple.h"

namespace plugin_vm {
namespace prefs {

// A boolean preference indicating whether Plugin VM is allowed by the
// corresponding user policy.
const char kPluginVmAllowed[] = "plugin_vm.allowed";
// A dictionary preference used to store information about PluginVm image.
// Stores a url for downloading PluginVm image and a SHA-256 hash for verifying
// finished download.
// For example: {
//   "url": "http://example.com/plugin_vm_image",
//   "hash": "842841a4c75a55ad050d686f4ea5f77e83ae059877fe9b6946aa63d3d057ed32"
// }
const char kPluginVmImage[] = "plugin_vm.image";
const char kPluginVmImageUrlKeyName[] = "url";
const char kPluginVmImageHashKeyName[] = "hash";

// A boolean preference representing whether there is a PluginVm image for
// this user on this device. Using vmc may cause this to go out-of-date, e.g.
// by renaming or deleting VMs.
const char kPluginVmImageExists[] = "plugin_vm.image_exists";

// Boolean preferences indicating whether Plugin VM is allowed to use certain
// devices.
const char kPluginVmPrintersAllowed[] = "plugin_vm.printers_allowed";
const char kPluginVmCameraAllowed[] = "plugin_vm.camera_allowed";
const char kPluginVmMicAllowed[] = "plugin_vm.mic_allowed";

// A string preference that specifies PluginVm licensing user id.
const char kPluginVmUserId[] = "plugin_vm.user_id";

// Preferences for storing engagement time data, as per
// GuestOsEngagementMetrics.
const char kEngagementPrefsPrefix[] = "plugin_vm.metrics";

// A boolean preference indicating whether data collection performed by PluginVm
// is allowed by the corresponding boolean user policy.
const char kPluginVmDataCollectionAllowed[] =
    "plugin_vm.data_collection_allowed";

// A uint64 preference indicating free disk space (in GB) required in order to
// proceed with Plugin VM installation
const char kPluginVmRequiredFreeDiskSpaceGB[] =
    "plugin_vm.required_free_disk_space";
constexpr int64_t kDefaultRequiredFreeDiskSpaceGB = 20LL;

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kPluginVmAllowed, false);
  registry->RegisterDictionaryPref(kPluginVmImage);
  registry->RegisterBooleanPref(kPluginVmImageExists, false);
  // TODO(crbug.com/40124674): For convenience this currently defaults to true,
  // but we'll need to revisit before launch.
  registry->RegisterBooleanPref(kPluginVmPrintersAllowed, true);
  registry->RegisterBooleanPref(kPluginVmCameraAllowed, false);
  registry->RegisterBooleanPref(kPluginVmMicAllowed, false);
  registry->RegisterStringPref(kPluginVmUserId, std::string());
  registry->RegisterBooleanPref(kPluginVmDataCollectionAllowed, false);
  registry->RegisterIntegerPref(kPluginVmRequiredFreeDiskSpaceGB,
                                kDefaultRequiredFreeDiskSpaceGB);

  guest_os::prefs::RegisterEngagementProfilePrefs(registry,
                                                  kEngagementPrefsPrefix);
}

}  // namespace prefs
}  // namespace plugin_vm
