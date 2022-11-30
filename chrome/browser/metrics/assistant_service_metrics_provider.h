// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_ASSISTANT_SERVICE_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_ASSISTANT_SERVICE_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

class AssistantServiceMetricsProvider : public metrics::MetricsProvider {
 public:
  AssistantServiceMetricsProvider();

  AssistantServiceMetricsProvider(const AssistantServiceMetricsProvider&) =
      delete;
  AssistantServiceMetricsProvider& operator=(
      const AssistantServiceMetricsProvider&) = delete;

  ~AssistantServiceMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto_unused) override;
};

#endif  // CHROME_BROWSER_METRICS_ASSISTANT_SERVICE_METRICS_PROVIDER_H_
