// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_METRICS_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_METRICS_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"

namespace ash::personalization_app {

inline constexpr char kPersonalizationKeyboardBacklightColorHistogramName[] =
    "Ash.Personalization.KeyboardBacklight.Color";

// -----------------------------------------------------------------------------
// Histograms
// -----------------------------------------------------------------------------

void LogKeyboardBacklightColor(mojom::BacklightColor backlight_color);

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_METRICS_H_
