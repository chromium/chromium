// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_platform_features_metrics_provider.h"

#include "base/metrics/histogram_macros.h"
#include "ui/native_theme/native_theme.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DarkModeStatus {
  kUnavailable = 0,
  kLight = 1,
  kDark = 2,
  kMaxValue = kDark,
};

}  // namespace

DesktopPlatformFeaturesMetricsProvider::
    DesktopPlatformFeaturesMetricsProvider() {}
DesktopPlatformFeaturesMetricsProvider::
    ~DesktopPlatformFeaturesMetricsProvider() {}

void DesktopPlatformFeaturesMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  ui::NativeTheme* theme = ui::NativeTheme::GetInstanceForNativeUi();
  DarkModeStatus status = DarkModeStatus::kUnavailable;
  if (theme->SystemDarkModeSupported()) {
    status = theme->ShouldUseDarkColors() ? DarkModeStatus::kDark
                                          : DarkModeStatus::kLight;
  }
  UMA_HISTOGRAM_ENUMERATION("Browser.DarkModeStatus", status);
}
