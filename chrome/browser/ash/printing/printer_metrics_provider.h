// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTER_METRICS_PROVIDER_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTER_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace metrics {
class ChromeUserMetricsExtension;
}  // namespace metrics

namespace ash {

class PrinterMetricsProvider : public metrics::MetricsProvider {
 public:
  PrinterMetricsProvider();

  PrinterMetricsProvider(const PrinterMetricsProvider&) = delete;
  PrinterMetricsProvider& operator=(const PrinterMetricsProvider&) = delete;

  ~PrinterMetricsProvider() override;

  // metrics::MetricsProvider overrides:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTER_METRICS_PROVIDER_H_
