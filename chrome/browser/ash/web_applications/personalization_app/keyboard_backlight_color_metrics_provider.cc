// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/keyboard_backlight_color_metrics_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/shell.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"

KeyboardBacklightColorMetricsProvider::KeyboardBacklightColorMetricsProvider() =
    default;
KeyboardBacklightColorMetricsProvider::
    ~KeyboardBacklightColorMetricsProvider() = default;

void KeyboardBacklightColorMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (!ash::features::IsRgbKeyboardEnabled() ||
      !ash::Shell::Get()->rgb_keyboard_manager()->IsRgbKeyboardSupported()) {
    return;
  }

  PrefService* pref_service =
      ProfileManager::GetActiveUserProfile()->GetPrefs();
  DCHECK(pref_service);
  auto backlight_color =
      static_cast<ash::personalization_app::mojom::BacklightColor>(
          pref_service->GetInteger(
              ash::prefs::kPersonalizationKeyboardBacklightColor));
  base::UmaHistogramEnumeration(
      "Ash.Personalization.KeyboardBacklight.Color.Settled", backlight_color);
}
