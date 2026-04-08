// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/downgrade_prefs.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace downgrade {

namespace {

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
constexpr int kDefaultMaxNumberOfSnapshots = 3;
#endif
}

void RegisterPrefs(PrefRegistrySimple* registry) {
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(prefs::kUserDataSnapshotRetentionLimit,
                                kDefaultMaxNumberOfSnapshots);
#endif
}

}  // namespace downgrade
