// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_prefs.h"

#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"

namespace contextual_cueing {

const char kContextualCueingDisabled[] = "contextual_cueing.disabled";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kContextualCueingDisabled, false);
}

}  // namespace contextual_cueing
