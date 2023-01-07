// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_UPDATE_ENGINE_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_UPDATE_ENGINE_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

class UpdateEngineMetricsProvider : public metrics::MetricsProvider {
 public:
  UpdateEngineMetricsProvider() = default;

  UpdateEngineMetricsProvider(const UpdateEngineMetricsProvider&) = delete;
  UpdateEngineMetricsProvider& operator=(const UpdateEngineMetricsProvider&) =
      delete;

  ~UpdateEngineMetricsProvider() override = default;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto_unused) override;

 private:
  bool IsConsumerAutoUpdateToggleEligible();
};

#endif  // CHROME_BROWSER_METRICS_UPDATE_ENGINE_METRICS_PROVIDER_H_
