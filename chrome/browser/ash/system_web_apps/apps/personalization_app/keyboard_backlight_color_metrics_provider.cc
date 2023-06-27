// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/keyboard_backlight_color_metrics_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/metrics/histogram_functions.h"

using DisplayType = ash::KeyboardBacklightColorController::DisplayType;

KeyboardBacklightColorMetricsProvider::KeyboardBacklightColorMetricsProvider() =
    default;
KeyboardBacklightColorMetricsProvider::
    ~KeyboardBacklightColorMetricsProvider() = default;

void KeyboardBacklightColorMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (!ash::Shell::HasInstance() ||
      !ash::Shell::Get()->rgb_keyboard_manager()->IsRgbKeyboardSupported()) {
    return;
  }

  auto* keyboard_backlight_color_controller =
      ash::Shell::Get()->keyboard_backlight_color_controller();

  const AccountId account_id =
      ash::Shell::Get()->session_controller()->GetActiveAccountId();

  const auto displayType =
      keyboard_backlight_color_controller->GetDisplayType(account_id);

  switch (displayType) {
    case DisplayType::kStatic: {
      base::UmaHistogramEnumeration(
          kPersonalizationKeyboardBacklightDisplayTypeSettledHistogramName,
          DisplayType::kStatic);
      auto backlight_color =
          keyboard_backlight_color_controller->GetBacklightColor(account_id);
      base::UmaHistogramEnumeration(
          kPersonalizationKeyboardBacklightColorSettledHistogramName,
          backlight_color);
      return;
    }
    case DisplayType::kMultiZone: {
      if (!ash::features::IsMultiZoneRgbKeyboardEnabled() ||
          ash::Shell::Get()->rgb_keyboard_manager()->GetZoneCount() <= 1) {
        return;
      }
      base::UmaHistogramEnumeration(
          kPersonalizationKeyboardBacklightDisplayTypeSettledHistogramName,
          DisplayType::kMultiZone);
      auto zone_colors =
          keyboard_backlight_color_controller->GetBacklightZoneColors(
              account_id);
      for (size_t i = 0; i < zone_colors.size(); i++) {
        base::UmaHistogramEnumeration(
            base::StringPrintf("Ash.Personalization.KeyboardBacklight."
                               "ZoneColors.Zone%zu.Settled",
                               i + 1),
            zone_colors[i]);
      }
      return;
    }
  }
}
