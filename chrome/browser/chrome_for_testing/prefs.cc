// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_for_testing/prefs.h"

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chrome_for_testing {

namespace {

// All registered components are typically updated in less than 3 seconds, so
// set the default timeout anticipating slower networks.
constexpr base::TimeDelta kDefaultRequiredComponentsUpdateTimeout =
    base::Seconds(15);

}  // namespace

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kEnableUserEducationUI, false);
  registry->RegisterBooleanPref(prefs::kEnableSearchEngineChoiceDialog, false);
  registry->RegisterBooleanPref(prefs::kEnableVirtualClipboard, false);
  registry->RegisterListPref(prefs::kRequiredComponents, base::ListValue());
  registry->RegisterFilePathPref(prefs::kRequiredComponentsDir,
                                 base::FilePath());
  registry->RegisterTimeDeltaPref(prefs::kRequiredComponentsUpdateTimeout,
                                  kDefaultRequiredComponentsUpdateTimeout);
}

void ClearPrefs(PrefService* pref_service) {
  pref_service->ClearPrefsWithPrefixSilently("chrome_for_testing");
}

}  // namespace chrome_for_testing
