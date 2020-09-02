// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_ACCESSIBILITY_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_ACCESSIBILITY_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

////////////////////////////////////////////////////////////////////////////////
//
// AccessibilityMetricsProvider
//
// A class used to provide frequent signals for AT or accessibility usage
// histograms on Win, Mac and Android, enable accurate counting of unique users.
//
////////////////////////////////////////////////////////////////////////////////
class AccessibilityMetricsProvider : public metrics::MetricsProvider {
 public:
  AccessibilityMetricsProvider();
  ~AccessibilityMetricsProvider() override;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_ACCESSIBILITY_METRICS_PROVIDER_H_
