// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace dino {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kDinoBookmarkPromptMaxTimesShownCounter,
                                0);
}

}  // namespace dino
