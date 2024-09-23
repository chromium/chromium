// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_PREF_NAMES_H_

class PrefRegistrySimple;

namespace bruschetta::prefs {

// Set to true if Bruschetta is installed.
extern const char kBruschettaInstalled[];

// Mapped pref for the enterprise policy BruschettaInstallerConfiguration.
extern const char kBruschettaInstallerConfiguration[];
extern const char kPolicyDisplayNameKey[];
extern const char kPolicyLearnMoreUrlKey[];

// Mapped pref for the enterprise policy BruschettaVMConfiguration.
extern const char kBruschettaVMConfiguration[];
extern const char kPolicyNameKey[];
extern const char kPolicyEnabledKey[];
extern const char kPolicyImageKey[];
extern const char kPolicyPflashKey[];
extern const char kPolicyURLKey[];
extern const char kPolicyHashKey[];
extern const char kPolicyVTPMKey[];
extern const char kPolicyVTPMEnabledKey[];
extern const char kPolicyVTPMUpdateActionKey[];
extern const char kPolicyOEMStringsKey[];
extern const char kPolicyDisplayOrderKey[];

extern const char kBruschettaMicAllowed[];

enum class PolicyEnabledState {
  BLOCKED = 0,
  RUN_ALLOWED = 1,
  INSTALL_ALLOWED = 2,
};

enum class PolicyUpdateAction {
  NONE = 0,
  FORCE_SHUTDOWN_IF_MORE_RESTRICTED = 1,
  FORCE_SHUTDOWN_ALWAYS = 2,
};

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace bruschetta::prefs

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_PREF_NAMES_H_
