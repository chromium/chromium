// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_PREF_NAMES_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_PREF_NAMES_H_

class PrefRegistrySimple;

namespace plugin_vm {
namespace prefs {

extern const char kPluginVmImage[];
extern const char kPluginVmImageExists[];
extern const char kEngagementPrefsPrefix[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_PREF_NAMES_H_
