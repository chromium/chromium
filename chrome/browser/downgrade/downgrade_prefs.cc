// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_DOWNGRADE_PREFS_H_
#define CHROME_BROWSER_DOWNGRADE_DOWNGRADE_PREFS_H_

#include "chrome/browser/downgrade/downgrade_prefs.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace downgrade {

namespace {

constexpr int kDefaultMaxNumberOfSnapshots = 3;

}

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kUserDataSnapshotRetentionLimit,
                                kDefaultMaxNumberOfSnapshots);
}

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_DOWNGRADE_PREFS_H_
