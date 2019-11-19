// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"

#include "components/guest_os/guest_os_prefs.h"
#include "components/prefs/pref_registry_simple.h"

namespace plugin_vm {
namespace prefs {

// A dictionary preference used to store information about PluginVm image.
// Stores a url for downloading PluginVm image and a SHA-256 hash for verifying
// finished download.
// For example: {
//   "url": "http://example.com/plugin_vm_image",
//   "hash": "842841a4c75a55ad050d686f4ea5f77e83ae059877fe9b6946aa63d3d057ed32"
// }
const char kPluginVmImage[] = "plugin_vm.image";
// A boolean preference representing whether there is a PluginVm image for
// this user on this device.
const char kPluginVmImageExists[] = "plugin_vm.image_exists";
// Preferences for storing engagement time data, as per
// GuestOsEngagementMetrics.
const char kEngagementPrefsPrefix[] = "plugin_vm.metrics";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kPluginVmImage);
  registry->RegisterBooleanPref(kPluginVmImageExists, false);

  guest_os::prefs::RegisterEngagementProfilePrefs(registry,
                                                  kEngagementPrefsPrefix);
}

}  // namespace prefs
}  // namespace plugin_vm
