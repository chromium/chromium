// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_COLOR_METRICS_PROVIDER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_COLOR_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace {

constexpr char kPersonalizationKeyboardBacklightColorSettledHistogramName[] =
    "Ash.Personalization.KeyboardBacklight.Color.Settled";

constexpr char
    kPersonalizationKeyboardBacklightDisplayTypeSettledHistogramName[] =
        "Ash.Personalization.KeyboardBacklight.DisplayType.Settled";

}  // namespace

class KeyboardBacklightColorMetricsProvider : public metrics::MetricsProvider {
 public:
  KeyboardBacklightColorMetricsProvider();

  KeyboardBacklightColorMetricsProvider(
      const KeyboardBacklightColorMetricsProvider&) = delete;
  KeyboardBacklightColorMetricsProvider& operator=(
      const KeyboardBacklightColorMetricsProvider&) = delete;

  ~KeyboardBacklightColorMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto_unused) override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_COLOR_METRICS_PROVIDER_H_
