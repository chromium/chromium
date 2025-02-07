// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_ACCESSIBILITY_STATE_PROVIDER_H_
#define CHROME_BROWSER_METRICS_ACCESSIBILITY_STATE_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

// AccessibilityStateProvider adds information about enabled accessibility flags
// in the system profile.
class AccessibilityStateProvider : public metrics::MetricsProvider {
 public:
  AccessibilityStateProvider();
  ~AccessibilityStateProvider() override;

  AccessibilityStateProvider(const AccessibilityStateProvider&) = delete;
  AccessibilityStateProvider& operator=(const AccessibilityStateProvider&) =
      delete;

  // Provides state on accessibility flags to system profile in the client's
  // report.
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
};

#endif  // CHROME_BROWSER_METRICS_ACCESSIBILITY_STATE_PROVIDER_H_
