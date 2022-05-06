// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_prefs.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash::personalization_app::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      ash::prefs::kPersonalizationKeyboardBacklightColor,
      static_cast<int>(mojom::BacklightColor::kWallpaper));
}

}  // namespace ash::personalization_app::prefs
