// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blocklist_extension_prefs.h"

#include "extensions/browser/extension_prefs.h"

namespace extensions {

namespace {

// If extension is blocklisted by Omaha attributes.
constexpr const char kPrefOmahaBlocklistState[] = "omaha_blocklist_state";

// If the user has acknowledged the blocklist state.
constexpr const char kPrefAcknowledgedBlocklistState[] =
    "acknowledged_blocklist_state";

// The default value to use for getting blocklist state from the pref.
constexpr BitMapBlocklistState kDefaultBitMapBlocklistState =
    BitMapBlocklistState::NOT_BLOCKLISTED;

}  // namespace

namespace blocklist_prefs {

void AddOmahaBlocklistState(const std::string& extension_id,
                            BitMapBlocklistState state,
                            extensions::ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state), ExtensionPrefs::BIT_MAP_PREF_ADD,
      kPrefOmahaBlocklistState, static_cast<int>(kDefaultBitMapBlocklistState));
}

void RemoveOmahaBlocklistState(const std::string& extension_id,
                               BitMapBlocklistState state,
                               extensions::ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state),
      ExtensionPrefs::BIT_MAP_PREF_REMOVE, kPrefOmahaBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
}

bool HasOmahaBlocklistState(const std::string& extension_id,
                            BitMapBlocklistState state,
                            extensions::ExtensionPrefs* extension_prefs) {
  int current_states = extension_prefs->GetBitMapPrefBits(
      extension_id, kPrefOmahaBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
  return (current_states & static_cast<int>(state)) != 0;
}

void AddAcknowledgedBlocklistState(
    const std::string& extension_id,
    BitMapBlocklistState state,
    extensions::ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state), ExtensionPrefs::BIT_MAP_PREF_ADD,
      kPrefAcknowledgedBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
}

void RemoveAcknowledgedBlocklistState(
    const std::string& extension_id,
    BitMapBlocklistState state,
    extensions::ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state),
      ExtensionPrefs::BIT_MAP_PREF_REMOVE, kPrefAcknowledgedBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
}

bool HasAcknowledgedBlocklistState(
    const std::string& extension_id,
    BitMapBlocklistState state,
    extensions::ExtensionPrefs* extension_prefs) {
  int current_states = extension_prefs->GetBitMapPrefBits(
      extension_id, kPrefAcknowledgedBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
  return (current_states & static_cast<int>(state)) != 0;
}

}  // namespace blocklist_prefs
}  // namespace extensions
