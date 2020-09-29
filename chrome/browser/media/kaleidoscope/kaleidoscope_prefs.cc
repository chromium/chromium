// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace kaleidoscope {
namespace prefs {

const char kKaleidoscopeFirstRunCompleted[] =
    "kaleidoscope.first_run_completed";

const char kKaleidoscopeAutoSelectMediaFeeds[] =
    "kaleidoscope.auto_select_media_feeds";

const char kKaleidoscopePolicyEnabled[] = "kaleidoscope.enabled_by_policy";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kKaleidoscopeFirstRunCompleted, 0);
  registry->RegisterBooleanPref(kKaleidoscopeAutoSelectMediaFeeds, false);
  registry->RegisterBooleanPref(kKaleidoscopePolicyEnabled, true);
}

}  // namespace prefs
}  // namespace kaleidoscope
