// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/keyboard_backlight_color_metrics_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/metrics/histogram_functions.h"

KeyboardBacklightColorMetricsProvider::KeyboardBacklightColorMetricsProvider() =
    default;
KeyboardBacklightColorMetricsProvider::
    ~KeyboardBacklightColorMetricsProvider() = default;

void KeyboardBacklightColorMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (!ash::features::IsRgbKeyboardEnabled() || !ash::Shell::HasInstance() ||
      !ash::Shell::Get()->rgb_keyboard_manager()->IsRgbKeyboardSupported()) {
    return;
  }

  auto* keyboard_backlight_color_controller =
      ash::Shell::Get()->keyboard_backlight_color_controller();
  auto backlight_color = keyboard_backlight_color_controller->GetBacklightColor(
      ash::Shell::Get()->session_controller()->GetActiveAccountId());
  base::UmaHistogramEnumeration(
      "Ash.Personalization.KeyboardBacklight.Color.Settled", backlight_color);
}
