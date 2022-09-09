// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/metrics_private/chrome_metrics_private_delegate.h"

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

namespace extensions {

bool ChromeMetricsPrivateDelegate::IsCrashReportingEnabled() {
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}

}  // namespace extensions
