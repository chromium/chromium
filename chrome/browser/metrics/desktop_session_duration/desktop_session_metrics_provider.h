// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

#include <memory>

namespace metrics {

std::unique_ptr<MetricsProvider> CreateDesktopSessionMetricsProvider();

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_METRICS_PROVIDER_H_
