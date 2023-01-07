// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_prefs.h"

#include "components/guest_os/guest_os_prefs.h"
#include "components/prefs/pref_registry_simple.h"

namespace borealis {
namespace prefs {

const char kBorealisInstalledOnDevice[] = "borealis.installed_on_device";

const char kBorealisVmTokenHash[] = "borealis.vm_token_hash";

const char kBorealisAllowedForUser[] = "borealis.allowed_for_user";

const char kEngagementPrefsPrefix[] = "borealis.metrics";

const char kBorealisMicAllowed[] = "borealis.microphone_allowed";

const char kExtraLaunchOptions[] = "borealis.extra_launch_options";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kBorealisInstalledOnDevice, false);
  registry->RegisterStringPref(kBorealisVmTokenHash, "");
  registry->RegisterBooleanPref(kBorealisAllowedForUser, true);
  registry->RegisterBooleanPref(kBorealisMicAllowed, false);
  registry->RegisterStringPref(kExtraLaunchOptions, std::string());
  guest_os::prefs::RegisterEngagementProfilePrefs(registry,
                                                  kEngagementPrefsPrefix);
}

}  // namespace prefs
}  // namespace borealis
