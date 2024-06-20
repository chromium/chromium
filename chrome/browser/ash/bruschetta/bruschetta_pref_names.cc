// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"

#include "components/prefs/pref_registry_simple.h"

namespace bruschetta::prefs {

const char kBruschettaInstalled[] = "bruschetta.installed";

const char kBruschettaInstallerConfiguration[] =
    "bruschetta.installer_configuration";

const char kPolicyDisplayNameKey[] = "display_name";
const char kPolicyLearnMoreUrlKey[] = "learn_more_url";

const char kBruschettaVMConfiguration[] = "bruschetta.vm_configuration";

const char kPolicyNameKey[] = "name";
const char kPolicyEnabledKey[] = "enabled_state";
const char kPolicyImageKey[] = "installer_image";
const char kPolicyPflashKey[] = "uefi_pflash";
const char kPolicyURLKey[] = "url";
const char kPolicyHashKey[] = "hash";
const char kPolicyVTPMKey[] = "vtpm";
const char kPolicyVTPMEnabledKey[] = "enabled";
const char kPolicyVTPMUpdateActionKey[] = "policy_update_action";
const char kPolicyOEMStringsKey[] = "oem_strings";
const char kPolicyDisplayOrderKey[] = "display_order";

// Boolean preference indiicating whether Bruschetta is allowed to use the mic.
const char kBruschettaMicAllowed[] = "bruschetta.mic_allowed";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kBruschettaInstalled, false);
  registry->RegisterBooleanPref(kBruschettaMicAllowed, false);
  registry->RegisterDictionaryPref(kBruschettaInstallerConfiguration);
  registry->RegisterDictionaryPref(kBruschettaVMConfiguration);
}

}  // namespace bruschetta::prefs
