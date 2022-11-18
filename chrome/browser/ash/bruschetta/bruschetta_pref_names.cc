// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"

#include "components/prefs/pref_registry_simple.h"

namespace bruschetta::prefs {

const char kBruschettaVMConfiguration[] = "bruschetta.vm_configuration";

const char kPolicyNameKey[] = "name";
const char kPolicyEnabledKey[] = "enabled_state";
const char kPolicyImageKey[] = "installer_image";
const char kPolicyPflashKey[] = "uefi_pflash";
const char kPolicyUefiKey[] = "uefi_image";
const char kPolicyURLKey[] = "url";
const char kPolicyHashKey[] = "hash";
const char kPolicyVTPMKey[] = "vtpm";
const char kPolicyVTPMEnabledKey[] = "enabled";
const char kPolicyVTPMUpdateActionKey[] = "policy_update_action";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kBruschettaVMConfiguration);
}

}  // namespace bruschetta::prefs
