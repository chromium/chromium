// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace borealis {
namespace prefs {

const char kBorealisInstalledOnDevice[] = "borealis.installed_on_device";

const char kBorealisAllowedForUser[] = "borealis.allowed_for_user";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kBorealisInstalledOnDevice, false);
  registry->RegisterBooleanPref(kBorealisAllowedForUser, true);
}

}  // namespace prefs
}  // namespace borealis
