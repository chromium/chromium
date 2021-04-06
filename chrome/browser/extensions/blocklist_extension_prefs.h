// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLOCKLIST_EXTENSION_PREFS_H_
#define CHROME_BROWSER_EXTENSIONS_BLOCKLIST_EXTENSION_PREFS_H_

#include <string>

#include "chrome/browser/extensions/blocklist.h"
#include "extensions/browser/blocklist_state.h"

namespace extensions {
class ExtensionPrefs;

// Helper namespace for adding/removing/querying prefs for the blocklist.
namespace blocklist_prefs {

// Adds the `state` to the omaha blocklist state pref.
void AddOmahaBlocklistState(const std::string& extension_id,
                            BitMapBlocklistState state,
                            extensions::ExtensionPrefs* extension_prefs);
// Removes the `state` from the omaha blocklist state pref. It doesn't clear
// the other states in the pref.
void RemoveOmahaBlocklistState(const std::string& extension_id,
                               BitMapBlocklistState state,
                               extensions::ExtensionPrefs* extension_prefs);
// Checks whether the `extension_id` has the `state` in the omaha blocklist
// state pref.
bool HasOmahaBlocklistState(const std::string& extension_id,
                            BitMapBlocklistState state,
                            extensions::ExtensionPrefs* extension_prefs);

// Adds the `state` to the acknowledged blocklist state pref.
void AddAcknowledgedBlocklistState(const std::string& extension_id,
                                   BitMapBlocklistState state,
                                   extensions::ExtensionPrefs* extension_prefs);
// Removes the `state` from the acknowledged blocklist state pref. It doesn't
// clear the other states in the pref.
void RemoveAcknowledgedBlocklistState(
    const std::string& extension_id,
    BitMapBlocklistState state,
    extensions::ExtensionPrefs* extension_prefs);
// Checks whether the `extension_id` has the `state` in the acknowledged
// blocklist state pref.
bool HasAcknowledgedBlocklistState(const std::string& extension_id,
                                   BitMapBlocklistState state,
                                   extensions::ExtensionPrefs* extension_prefs);

}  // namespace blocklist_prefs
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLOCKLIST_EXTENSION_PREFS_H_
