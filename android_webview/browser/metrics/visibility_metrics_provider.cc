// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/visibility_metrics_provider.h"

#include "android_webview/browser/metrics/visibility_metrics_logger.h"

namespace android_webview {

VisibilityMetricsProvider::VisibilityMetricsProvider(
    VisibilityMetricsLogger* logger)
    : logger_(logger) {}

VisibilityMetricsProvider::~VisibilityMetricsProvider() = default;

void VisibilityMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  logger_->RecordMetrics();
}

}  // namespace android_webview