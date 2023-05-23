// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/pref_names.h"

#include "base/files/file_path.h"
#include "components/prefs/pref_registry_simple.h"

namespace prefs {

const char kScreenAILastUsedTimePrefName[] =
    "accessibility.screen_ai.last_used_time";
}  // namespace prefs

namespace screen_ai {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kScreenAILastUsedTimePrefName,
                             base::Time());
}

}  // namespace screen_ai
