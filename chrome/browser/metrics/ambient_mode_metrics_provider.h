// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_AMBIENT_MODE_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_AMBIENT_MODE_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

class AmbientModeMetricsProvider : public metrics::MetricsProvider {
 public:
  AmbientModeMetricsProvider();

  AmbientModeMetricsProvider(const AmbientModeMetricsProvider&) = delete;
  AmbientModeMetricsProvider& operator=(const AmbientModeMetricsProvider&) =
      delete;

  ~AmbientModeMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto_unused) override;
};

#endif  // CHROME_BROWSER_METRICS_AMBIENT_MODE_METRICS_PROVIDER_H_
