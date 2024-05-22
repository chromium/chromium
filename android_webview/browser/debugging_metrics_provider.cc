// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/debugging_metrics_provider.h"

#include "android_webview/browser/aw_devtools_server.h"
#include "base/android/build_info.h"
#include "base/metrics/histogram_functions.h"

namespace {

const char kisDebuggableHistogramName[] = "Android.WebView.isDebuggable";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DebuggingEnabled {
  kNotEnabled = 0,
  kEnabledBySetWebContentsDebugging = 1,
  kEnabledByDebuggableAppOrOS = 2,
  kMaxValue = kEnabledByDebuggableAppOrOS,
};

void RecordMetricsImpl() {
  base::android::BuildInfo* build_info =
      base::android::BuildInfo::GetInstance();
  if (build_info->is_debug_app() || build_info->is_debug_android()) {
    base::UmaHistogramEnumeration(
        kisDebuggableHistogramName,
        DebuggingEnabled::kEnabledByDebuggableAppOrOS);
  } else if (android_webview::IsAwDevToolsServerStarted()) {
    base::UmaHistogramEnumeration(
        kisDebuggableHistogramName,
        DebuggingEnabled::kEnabledBySetWebContentsDebugging);

  } else {
    base::UmaHistogramEnumeration(kisDebuggableHistogramName,
                                  DebuggingEnabled::kNotEnabled);
  }
}

}  // namespace

namespace android_webview {

void DebuggingMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  RecordMetricsImpl();
}

}  // namespace android_webview
