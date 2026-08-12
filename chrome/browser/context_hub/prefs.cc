// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/prefs.h"

#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"

namespace context_hub::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kContextHubLastAutoTodosGenerationTime,
                             base::Time());
}

}  // namespace context_hub::prefs
