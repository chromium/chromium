// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/help_app_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace ash::help_app::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kHelpAppHasCompletedNewDeviceChecklist, false);
  registry->RegisterBooleanPref(kHelpAppHasVisitedHowToPage, false);
}

}  // namespace ash::help_app::prefs
