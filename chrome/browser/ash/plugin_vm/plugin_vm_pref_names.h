// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_PREF_NAMES_H_

class PrefRegistrySimple;

namespace plugin_vm {
namespace prefs {

extern const char kPluginVmAllowed[];
extern const char kPluginVmImage[];
extern const char kPluginVmImageUrlKeyName[];
extern const char kPluginVmImageHashKeyName[];
extern const char kPluginVmImageExists[];
extern const char kPluginVmPrintersAllowed[];
extern const char kPluginVmCameraAllowed[];
extern const char kPluginVmMicAllowed[];
extern const char kPluginVmUserId[];
extern const char kEngagementPrefsPrefix[];
extern const char kPluginVmDataCollectionAllowed[];
extern const char kPluginVmRequiredFreeDiskSpaceGB[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_PREF_NAMES_H_
