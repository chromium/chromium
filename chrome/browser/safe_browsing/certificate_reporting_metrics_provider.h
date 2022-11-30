// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_METRICS_PROVIDER_H_
#define CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

// CertificateReportingService doesn't do its own scheduling when retrying
// uploads of failed reports. Instead, it piggybacks off of the metrics service
// scheduler.
//
// When the metrics services requests metrics to be uploaded,
// CertificateReportingMetricsProvider looks up the CertificateReportingService
// for the current profile and tells it send all pending reports at once.
class CertificateReportingMetricsProvider : public metrics::MetricsProvider {
 public:
  CertificateReportingMetricsProvider();
  ~CertificateReportingMetricsProvider() override;

  // metrics:MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* unused) override;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_METRICS_PROVIDER_H_
