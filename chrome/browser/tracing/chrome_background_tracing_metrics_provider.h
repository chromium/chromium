// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_CHROME_BACKGROUND_TRACING_METRICS_PROVIDER_H_
#define CHROME_BROWSER_TRACING_CHROME_BACKGROUND_TRACING_METRICS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/tracing/common/background_tracing_metrics_provider.h"

class ChromeOSSystemProfileProvider;

namespace tracing {

// The background tracing manager will make sure traces are only uploaded on
// WiFi, or the traces are small when uploading over data, to make sure weekly
// upload quota for UMA metrics is not affected on Android.
class ChromeBackgroundTracingMetricsProvider
    : public BackgroundTracingMetricsProvider {
 public:
  explicit ChromeBackgroundTracingMetricsProvider(
      ChromeOSSystemProfileProvider* cros_system_profile_provider);

  ChromeBackgroundTracingMetricsProvider(
      const ChromeBackgroundTracingMetricsProvider&) = delete;
  ChromeBackgroundTracingMetricsProvider& operator=(
      const ChromeBackgroundTracingMetricsProvider&) = delete;

  ~ChromeBackgroundTracingMetricsProvider() override;

  // metrics::MetricsProvider:
  void DoInit() override;
  void AsyncInit(base::OnceClosure done_callback) override;

  void RecordCoreSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

 private:
  // owned by BackgroundTracingMetricsProvider::system_profile_providers_.
  raw_ptr<MetricsProvider> av_metrics_provider_ = nullptr;
  raw_ptr<MetricsProvider> chromeos_metrics_provider_ = nullptr;
  raw_ptr<ChromeOSSystemProfileProvider> cros_system_profile_provider_ =
      nullptr;
};

}  // namespace tracing

#endif  // CHROME_BROWSER_TRACING_CHROME_BACKGROUND_TRACING_METRICS_PROVIDER_H_
