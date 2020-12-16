// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace lacros_prefs {

const char kShowedExperimentalBannerPref[] =
    "lacros.showed_experimental_banner";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kShowedExperimentalBannerPref,
                                /*default_value=*/false);
}

}  // namespace lacros_prefs
